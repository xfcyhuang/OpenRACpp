// UPSTREAM: OpenRA.Game/FileSystem/FileSystem.cs @7d57605 L20-303(逐语义重写)
// 全文件仅经 file_system.hpp 的 import std; 引入标准库(工程门禁:
// 禁止 #include <标准头>)。
// All standard-library entities come via import std; in file_system.hpp
// (project gate: no #include <std headers>).
#include "fs/file_system.hpp"

#include "fs/folder.hpp"
#include "fs/zip_file.hpp"

namespace ora::fs {

namespace {

namespace stdfs = std::filesystem;

// 尾部查找:列表中最后一个满足谓词的元素(上游 LINQ LastOrDefault)
// Tail search: the last element satisfying pred (upstream LINQ LastOrDefault).
template <typename T, typename Pred>
T LastOrDefault(const std::vector<T>& items, Pred pred) {  // T 为指针类型;未命中返回 nullptr
                                                       // T is a pointer type; nullptr on miss.
  for (auto it = items.rbegin(); it != items.rend(); ++it)
    if (pred(*it))
      return *it;
  return nullptr;
}

}  // namespace

FileSystem::FileSystem(std::vector<std::unique_ptr<IPackageLoader>> packageLoaders)
    : vec_packageLoaders_{std::move(packageLoaders)} {
  // zip 加载器内建追加(FileSystem.cs L47-49)
  // The zip loader is appended built-in (FileSystem.cs L47-49).
  vec_packageLoaders_.push_back(std::make_unique<ZipFileLoader>());
}

bool FileSystem::TryParsePackage(std::span<const char> bytes, const std::string& filename,
                                 std::unique_ptr<IReadOnlyPackage>& package) {
  package.reset();
  for (const auto& loader : vec_packageLoaders_)
    if (loader->TryParsePackage(bytes, filename, *this, package))
      return true;
  return false;
}

std::unique_ptr<IReadOnlyPackage> FileSystem::OpenPackage(const std::string& filename) {
  // 裸目录最常见,优先尝试(FileSystem.cs L64-67;Platform.ResolvePath 属
  // Game 层,Phase 1 直接使用原名,已登记 COVERAGE)
  // Raw directories are the most common case, try them first (FileSystem.cs
  // L64-67; Platform.ResolvePath belongs to the Game layer — Phase 1 uses the
  // name as-is, registered in COVERAGE).
  const bool hasExplicitSplit = filename.find('|') != std::string::npos;
  std::error_code ec;
  if (!hasExplicitSplit && stdfs::is_directory(stdfs::path{filename}, ec))
    return std::make_unique<Folder>(filename);

  // 其他包的子路径需特殊处理
  // Children of another package need special handling.
  IReadOnlyPackage* parent = nullptr;
  std::string subPath;
  if (TryGetPackageContaining(filename, parent, subPath))
    return parent->OpenPackage(subPath, *this);

  // 正常打开:经挂载索引读入内容并按签名嗅探;Open 找不到时抛异常
  // Normal open: read through the mount index and sniff by signature; a
  // missing file makes Open throw.
  const std::vector<char> bytes = Open(filename);
  std::unique_ptr<IReadOnlyPackage> package;
  if (TryParsePackage(bytes, filename, package))
    return package;

  return nullptr;
}

void FileSystem::Mount(const std::string& name, const std::string& explicitName) {
  // '~' 前缀 = 可选挂载,任何失败静默吞掉(FileSystem.cs L85-88/L112-114)
  // The '~' prefix marks an optional mount; any failure is silently
  // swallowed (FileSystem.cs L85-88/L112-114).
  const bool optional = !name.empty() && name[0] == '~';
  const std::string realName = optional ? name.substr(1) : name;

  try {
    if (!realName.empty() && realName[0] == '$') {
      // 已登记偏离:'$' mod 引用挂载需 installedMods/Manifest(Phase 2)
      // Registered deviation: the '$' mod-reference mount needs
      // installedMods/Manifest (Phase 2).
      throw std::runtime_error{std::format(
          "Could not load mod '{}': '$' mounts require Manifest support (Phase 2).",
          realName.substr(1))};
    }

    auto package = OpenPackage(realName);
    if (package == nullptr)
      throw std::runtime_error{std::format(
          "Could not open package '{}', file not found or its format is not supported.",
          realName)};

    Mount(std::move(package), explicitName);
  } catch (...) {
    if (!optional)
      throw;
  }
}

void FileSystem::Mount(std::unique_ptr<IReadOnlyPackage> package,
                       const std::string& explicitName) {
  IReadOnlyPackage* raw = package.get();
  const bool alreadyMounted =
      std::find(vec_mounted_.begin(), vec_mounted_.end(), raw) != vec_mounted_.end();
  if (alreadyMounted) {
    // 已挂载:计数 +1 并提升文件加载优先级(索引中移到列表尾,
    // FileSystem.cs L119-129);调用方保留所有权(mod 包不受控,Phase 2)
    // Already mounted: bump the count and the loading priority (move to the
    // tail of each index list, FileSystem.cs L119-129); the caller keeps
    // ownership (mod packages are not owned by us, Phase 2).
    package.release();
    for (const std::string& fn : raw->Contents()) {
      auto& list = map_fileIndex_[fn];
      list.erase(std::remove(list.begin(), list.end(), raw), list.end());
      list.push_back(raw);
    }
    vec_mounted_.push_back(raw);
    return;
  }

  // 首次挂载:接管所有权并登记索引
  // First mount: take ownership and register in the index.
  vec_owned_.push_back(std::move(package));
  vec_mounted_.push_back(raw);

  if (!explicitName.empty())
    map_explicitMounts_[explicitName] = raw;

  for (const std::string& fn : raw->Contents())
    map_fileIndex_[fn].push_back(raw);
}

bool FileSystem::Unmount(const IReadOnlyPackage* package) {
  const auto slot = std::find(vec_mounted_.begin(), vec_mounted_.end(), package);
  if (slot == vec_mounted_.end())
    return false;

  const auto mountCount = static_cast<std::size_t>(
      std::count(vec_mounted_.begin(), vec_mounted_.end(), package));
  vec_mounted_.erase(slot);
  if (mountCount > 1)
    return true;  // 仅减计数,索引优先级不回落(上游 L166 只减计数)
                  // Count decremented only; index priority does not fall back
                  // (upstream L166 only decrements).

  // 最后一次引用:从索引移除、清理显式挂载、释放所有权
  // Last reference: drop from the index, clear explicit mounts, release
  // ownership.
  for (auto& [fn, list] : map_fileIndex_)
    list.erase(std::remove(list.begin(), list.end(), package), list.end());
  // 注:上游 fileIndex 保留空列表(Exists 仍按"曾有键"为真),此处对齐
  // NB: upstream keeps the empty lists in fileIndex (Exists stays true for a
  // once-mounted name); we match that.

  for (auto it = map_explicitMounts_.begin(); it != map_explicitMounts_.end();)
    it = it->second == package ? map_explicitMounts_.erase(it) : std::next(it);

  std::erase_if(vec_owned_, [package](const auto& p) { return p.get() == package; });
  return true;
}

void FileSystem::UnmountAll() {
  // 上游对所有非 mod 包 Dispose 后全清(FileSystem.cs L171-182)
  // Upstream disposes every non-mod package, then clears everything
  // (FileSystem.cs L171-182).
  vec_owned_.clear();
  vec_mounted_.clear();
  map_explicitMounts_.clear();
  map_fileIndex_.clear();
}

std::vector<std::string> FileSystem::MountedPackageNames() const {
  std::vector<std::string> names;
  names.reserve(vec_mounted_.size());
  for (const IReadOnlyPackage* p : vec_mounted_)
    names.push_back(p->Name());
  return names;
}

std::vector<char> FileSystem::Open(const std::string& filename) const {
  std::vector<char> bytes;
  if (!TryOpen(filename, bytes))
    throw std::runtime_error{std::format("File not found: {}", filename)};
  return bytes;
}

bool FileSystem::TryGetPackageContaining(const std::string& path, IReadOnlyPackage*& package,
                                         std::string& filename) const {
  const std::size_t explicitSplit = path.find('|');
  if (explicitSplit != std::string::npos && explicitSplit > 0) {
    const auto it = map_explicitMounts_.find(path.substr(0, explicitSplit));
    if (it != map_explicitMounts_.end()) {
      package = it->second;
      filename = path.substr(explicitSplit + 1);
      return true;
    }
  }

  // Cache 索引语义:缺失键产生空列表,不视为错误
  // Cache index semantics: a missing key yields an empty list, not an error.
  const auto it = map_fileIndex_.find(path);
  package = nullptr;
  if (it != map_fileIndex_.end())
    package = LastOrDefault(it->second,
                            [&](IReadOnlyPackage* p) { return p->Contains(path); });
  filename = path;
  return package != nullptr;
}

std::optional<std::vector<char>> FileSystem::GetFromCache(const std::string& filename) const {
  // fileIndex[filename].LastOrDefault(x => x.Contains(filename)) → GetStream
  // (FileSystem.cs L193-199)
  const auto it = map_fileIndex_.find(filename);
  if (it == map_fileIndex_.end())
    return std::nullopt;
  IReadOnlyPackage* package = LastOrDefault(
      it->second, [&](IReadOnlyPackage* p) { return p->Contains(filename); });
  if (package == nullptr)
    return std::nullopt;
  return package->GetStream(filename);
}

bool FileSystem::TryOpen(const std::string& filename, std::vector<char>& bytes) const {
  // 显式前缀("pkg|file")优先
  // The explicit prefix ("pkg|file") goes first.
  const std::size_t explicitSplit = filename.find('|');
  if (explicitSplit != std::string::npos && explicitSplit > 0) {
    const auto it = map_explicitMounts_.find(filename.substr(0, explicitSplit));
    if (it != map_explicitMounts_.end()) {
      auto s = it->second->GetStream(filename.substr(explicitSplit + 1));
      if (s.has_value()) {
        bytes = std::move(*s);
        return true;
      }
    }
  }

  if (auto cached = GetFromCache(filename); cached.has_value()) {
    bytes = std::move(*cached);
    return true;
  }

  // 文件应在显式包中(但没找到)——不再按文件名回退(文件名含非法 '|';
  // 上游 TODO 注释保留语义)
  // The file should live in an explicit package (but wasn't found there) —
  // do not fall back to the plain filename (it contains the invalid '|';
  // upstream TODO comment kept).
  if (explicitSplit != std::string::npos)
    return false;

  // 逐包询问(fallback,上游 TODO 后清理时可移除)
  // Ask each package in mount order (fallback; removable once the upstream
  // TODO cleanups land).
  IReadOnlyPackage* package =
      LastOrDefault(vec_mounted_, [&](IReadOnlyPackage* p) { return p->Contains(filename); });
  if (package != nullptr) {
    auto s = package->GetStream(filename);
    if (s.has_value()) {
      bytes = std::move(*s);
      return true;
    }
  }

  return false;
}

bool FileSystem::Exists(const std::string& filename) const {
  const std::size_t explicitSplit = filename.find('|');
  if (explicitSplit != std::string::npos && explicitSplit > 0) {
    const auto it = map_explicitMounts_.find(filename.substr(0, explicitSplit));
    if (it != map_explicitMounts_.end() && it->second->Contains(filename.substr(explicitSplit + 1)))
      return true;
  }

  // 上游语义:索引中存在键即存在(不复查包的 Contains)
  // Upstream semantics: presence of the index key (no re-check of Contains).
  return map_fileIndex_.contains(filename);
}

std::string FileSystem::ResolveCaseInsensitivePath(const std::string& path) {
  // 逐级在真实目录项中找忽略大小写匹配(过滤 "." 段;相对路径无根 → 空)
  // Resolve segment by segment against real entries, case-insensitively
  // (filtering "." segments; a rootless relative path yields empty).
  const stdfs::path p{path};
  stdfs::path resolved = p.root_path();
  if (resolved.empty())
    return {};
  for (const auto& seg : p.relative_path()) {
    if (seg == ".")
      continue;
    bool found = false;
    std::error_code ec;
    for (const auto& it : stdfs::directory_iterator(resolved, ec)) {
      std::string candidate = it.path().filename().generic_string();
      const std::string want = seg.generic_string();
      if (candidate.size() == want.size() &&
          std::ranges::equal(candidate, want, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
          })) {
        resolved = it.path();
        found = true;
        break;
      }
    }
    if (!found)
      return {};
  }
  return resolved.generic_string();
}

}  // namespace ora::fs
