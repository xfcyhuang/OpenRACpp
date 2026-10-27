#include "fs/file_system.hpp"

#include "fs/folder.hpp"
#include "fs/zip_file.hpp"

namespace ora::fs {

namespace {

namespace stdfs = std::filesystem;

template <typename T, typename Pred>
T LastOrDefault(const std::vector<T>& items, Pred pred) {

  for (auto it = items.rbegin(); it != items.rend(); ++it)
    if (pred(*it))
      return *it;
  return nullptr;
}

}

FileSystem::FileSystem(std::vector<std::unique_ptr<IPackageLoader>> packageLoaders)
    : vec_packageLoaders_{std::move(packageLoaders)} {

  vec_packageLoaders_.push_back(std::make_unique<ZipFileLoader>());
}

void FileSystem::SetPathResolver(std::function<std::string(const std::string&)> fn_resolve) {
  fn_pathResolver_ = std::move(fn_resolve);
}

void FileSystem::SetModPackageResolver(
    std::function<IReadOnlyPackage*(const std::string&)> fn_resolve_mod) {
  fn_modResolver_ = std::move(fn_resolve_mod);
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

  const std::string resolvedPath =
      fn_pathResolver_ ? fn_pathResolver_(filename) : filename;
  const bool hasExplicitSplit = resolvedPath.find('|') != std::string::npos;
  std::error_code ec;
  if (!hasExplicitSplit && stdfs::is_directory(stdfs::path{resolvedPath}, ec))
    return std::make_unique<Folder>(resolvedPath);

  IReadOnlyPackage* parent = nullptr;
  std::string subPath;
  if (TryGetPackageContaining(filename, parent, subPath))
    return parent->OpenPackage(subPath, *this);

  const std::vector<char> bytes = Open(filename);
  std::unique_ptr<IReadOnlyPackage> package;
  if (TryParsePackage(bytes, filename, package))
    return package;

  return nullptr;
}

void FileSystem::Mount(const std::string& name, const std::string& explicitName) {

  const bool optional = !name.empty() && name[0] == '~';
  const std::string realName = optional ? name.substr(1) : name;

  try {
    if (!realName.empty() && realName[0] == '$') {

      const std::string str_mod = realName.substr(1);
      if (!fn_modResolver_)
        throw std::runtime_error{std::format(
            "Could not load mod '{}': no InstalledMods resolver installed.", str_mod)};
      IReadOnlyPackage* pkg_mod = fn_modResolver_(str_mod);
      if (pkg_mod == nullptr)
        throw std::runtime_error{
            std::format("Could not load mod '{}'.", str_mod)};
      MountBorrowed(pkg_mod, explicitName);
      return;
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

void FileSystem::MountBorrowed(IReadOnlyPackage* package, const std::string& explicitName) {

  const bool alreadyMounted =
      std::find(vec_mounted_.begin(), vec_mounted_.end(), package) != vec_mounted_.end();
  if (!alreadyMounted) {
    vec_mounted_.push_back(package);
    if (!explicitName.empty())
      map_explicitMounts_.emplace(explicitName, package);
    for (const std::string& fn : package->Contents())
      map_fileIndex_[fn].push_back(package);
  } else {

    vec_mounted_.push_back(package);
    for (const std::string& fn : package->Contents()) {
      auto& list = map_fileIndex_[fn];
      list.erase(std::remove(list.begin(), list.end(), package), list.end());
      list.push_back(package);
    }
  }
}

void FileSystem::Mount(std::unique_ptr<IReadOnlyPackage> package,
                       const std::string& explicitName) {
  IReadOnlyPackage* raw = package.get();
  const bool alreadyMounted =
      std::find(vec_mounted_.begin(), vec_mounted_.end(), raw) != vec_mounted_.end();
  if (alreadyMounted) {

    package.release();
    for (const std::string& fn : raw->Contents()) {
      auto& list = map_fileIndex_[fn];
      list.erase(std::remove(list.begin(), list.end(), raw), list.end());
      list.push_back(raw);
    }
    vec_mounted_.push_back(raw);
    return;
  }

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
    return true;

  for (auto& [fn, list] : map_fileIndex_)
    list.erase(std::remove(list.begin(), list.end(), package), list.end());

  for (auto it = map_explicitMounts_.begin(); it != map_explicitMounts_.end();)
    it = it->second == package ? map_explicitMounts_.erase(it) : std::next(it);

  std::erase_if(vec_owned_, [package](const auto& p) { return p.get() == package; });
  return true;
}

void FileSystem::UnmountAll() {

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

std::vector<IReadOnlyPackage*> FileSystem::MountedPackages() const {
  std::vector<IReadOnlyPackage*> vec_packages;
  std::set<const IReadOnlyPackage*> set_seen;
  for (IReadOnlyPackage* p : vec_mounted_)
    if (set_seen.insert(p).second)
      vec_packages.push_back(p);
  return vec_packages;
}

std::optional<std::string> FileSystem::GetPrefix(
    const IReadOnlyPackage* package) const {
  for (const auto& [str_name, ptr_package] : map_explicitMounts_)
    if (ptr_package == package)
      return str_name;
  return std::nullopt;
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

  const auto it = map_fileIndex_.find(path);
  package = nullptr;
  if (it != map_fileIndex_.end())
    package = LastOrDefault(it->second,
                            [&](IReadOnlyPackage* p) { return p->Contains(path); });
  filename = path;
  return package != nullptr;
}

std::optional<std::vector<char>> FileSystem::GetFromCache(const std::string& filename) const {

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

  if (explicitSplit != std::string::npos)
    return false;

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

  return map_fileIndex_.contains(filename);
}

std::string FileSystem::ResolveCaseInsensitivePath(const std::string& path) {

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

}
