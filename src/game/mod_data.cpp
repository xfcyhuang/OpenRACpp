// UPSTREAM: OpenRA.Game/ModData.cs @b6fc03f(mod_data.hpp 的实现;
//          Phase 2 最小集挂载链,见 hpp 头注)
//          Implementation of mod_data.hpp — the Phase 2 minimal mount
//          chain; see the hpp header notes.
import std;
#include "game/mod_data.hpp"
#include "game/platform.hpp"
#include "fs/d2k_sound_resources.hpp"
#include "fs/mix_file.hpp"
#include "meta/field_loader.hpp"

namespace ora::game {

namespace {

/// ObjectCreator.GetLoaders<IPackageLoader>(ModData.cs L63)的 C++ 等价:
/// 按 Manifest.PackageFormats 的加载器名构造。未知名 = 上游
/// InvalidOperationException 文本逐字。
/// The C++ equivalent of ObjectCreator.GetLoaders<IPackageLoader>
/// (ModData.cs L63): construct per the loader names in
/// Manifest.PackageFormats. An unknown name reproduces the upstream
/// InvalidOperationException text verbatim.
std::vector<std::unique_ptr<fs::IPackageLoader>> MakePackageLoaders(
    const std::vector<std::string>& vec_formats) {
  auto vec_loaders = std::vector<std::unique_ptr<fs::IPackageLoader>>{};
  for (const std::string& str_format : vec_formats) {
    if (str_format == "Mix")
      vec_loaders.push_back(std::make_unique<fs::MixLoader>());
    else if (str_format == "D2kSoundResources")
      vec_loaders.push_back(std::make_unique<fs::D2kSoundResourcesLoader>());
    else
      throw std::runtime_error{std::format(
          "Unable to find a package loader for type '{}'.", str_format)};
  }
  return vec_loaders;
}

}  // namespace

// ———— InstalledMods ————

InstalledMods::InstalledMods(std::string str_mods_root)
    : str_modsRoot_{std::move(str_mods_root)} {
  namespace stdfs = std::filesystem;

  // 目录名序发现(见 mod_data.hpp 裁剪注记)
  // Discovery in directory-name order (see the scope note in
  // mod_data.hpp).
  std::vector<std::string> vec_ids;
  std::error_code ec;
  for (const auto& entry : stdfs::directory_iterator{str_modsRoot_, ec}) {
    if (!entry.is_directory())
      continue;
    const std::string str_id = entry.path().filename().string();
    if (stdfs::exists(entry.path() / "mod.yaml"))
      vec_ids.push_back(str_id);
  }
  std::ranges::sort(vec_ids);

  for (const std::string& str_id : vec_ids) {
    auto pkg_folder = std::make_unique<fs::Folder>(str_modsRoot_ + "/" + str_id);
    try {
      auto manifest = std::make_unique<Manifest>(str_id, *pkg_folder);
      vec_packages_.push_back(std::move(pkg_folder));
      vec_mods_.emplace_back(str_id, std::move(manifest));
    } catch (const std::exception&) {
      // mod.yaml 解析失败(缺节等)的 mod 不入表(上游 InstalledMods 发现
      // 失败同样跳过该 mod)
      // A mod whose mod.yaml fails to parse stays out of the table (upstream
      // discovery skips failing mods likewise).
    }
  }
}

const Manifest* InstalledMods::Find(std::string_view str_id) const {
  for (const auto& [str_name, manifest] : vec_mods_)
    if (str_name == str_id)
      return manifest.get();
  return nullptr;
}

void InstalledMods::InstallResolver(fs::FileSystem& fileSystem) const {
  fileSystem.SetModPackageResolver(
      [this](const std::string& str_mod) -> fs::IReadOnlyPackage* {
        const Manifest* manifest = Find(str_mod);
        return manifest != nullptr ? &manifest->Package() : nullptr;
      });
}

// ———— ModData ————

ModData::ModData(const Manifest& manifest_source, const InstalledMods& installedMods,
                 std::string str_engine_dir)
    : manifest_{std::make_unique<Manifest>(manifest_source.Id(), manifest_source.Package())},
      // PackageLoaders → FileSystem 构造(ModData.cs L63-65):包格式加载器
      // 在 FileSystem 创建时注入(内建 zip 加载器仍随后自动追加)。
      // PackageLoaders → the FileSystem construction (ModData.cs L63-65):
      // the package-format loaders inject at FileSystem creation (the
      // built-in zip loader is still appended afterwards).
      fs_modFiles_{MakePackageLoaders(manifest_->PackageFormats())} {
  // Platform.ResolvePath 接线 + '$mod' 解析器(ModData 构造器 L61-64 的等价)
  // The Platform.ResolvePath wiring + the '$mod' resolver (the equivalent of
  // the ModData ctor L61-64).
  const PathResolver resolver_paths{std::move(str_engine_dir), std::string{}};
  fs_modFiles_.SetPathResolver(
      [resolver_paths](const std::string& str_path) { return resolver_paths(str_path); });
  installedMods.InstallResolver(fs_modFiles_);

  // FileSystem 节点挂载(ModData.cs L66-69):loader 名分派。
  // Phase 2 覆盖 ra/cnc/d2k/mod/ts/all 的 "ContentInstallerFileSystem" 与
  // "DefaultFileSystemLoader" 两族 —— 二者挂载行为在规则加载面等价:
  // SystemPackages 必挂、ContentPackages/RequiredContentFiles 可失败
  // (ContentInstaller 的 isContentAvailable 属内容安装流程,Phase 6)
  // The FileSystem-section mount (ModData.cs L66-69): dispatch on the loader
  // name. Phase 2 covers the "ContentInstallerFileSystem" and
  // "DefaultFileSystemLoader" families used by ra/cnc/d2k/mod/ts/all —
  // identical at the rules-loading surface: SystemPackages mount hard,
  // ContentPackages/RequiredContentFiles may fail (ContentInstaller's
  // isContentAvailable belongs to the content-install flow, Phase 6).
  const std::string_view sv_loader = manifest_->FileSystemLoaderName();
  if (sv_loader != "ContentInstallerFileSystem" && sv_loader != "DefaultFileSystemLoader")
    throw std::runtime_error{std::format(
        "Unknown FileSystem loader `{}` (Phase 2 支持集之外)", sv_loader)};

  const yaml::MiniYaml& yaml_fs = manifest_->FileSystemNode();
  const auto mount_packages = [&fileSystem = fs_modFiles_](const yaml::MiniYaml& yaml_node) {
    // LoadPackages(DefaultFileSystemLoader.cs L26-38 /
    // ContentInstallerFileSystemLoader.cs L73-82):节点键 = 包名,值 = 显式名
    // LoadPackages (DefaultFileSystemLoader.cs L26-38 /
    // ContentInstallerFileSystemLoader.cs L73-82): node key = package name,
    // value = explicit mount name.
    for (const yaml::MiniYamlNode& node : yaml_node.Nodes) {
      const std::string str_name = node.Key != nullptr ? *node.Key : std::string{};
      const std::string str_explicit =
          node.Value.Value != nullptr ? *node.Value.Value : std::string{};
      fileSystem.Mount(str_name, str_explicit);
    }
  };

  const auto find_section = [&yaml_fs](std::string_view sv_key) -> const yaml::MiniYaml* {
    for (const yaml::MiniYamlNode& node : yaml_fs.Nodes)
      if (node.Key != nullptr && std::string_view{*node.Key} == sv_key)
        return &node.Value;
    return nullptr;
  };

  // SystemPackages:必挂(ContentInstallerFileSystemLoader.cs L75-77;
  // 缺节抛 MissingFieldsException)
  // SystemPackages: hard requirement (ContentInstallerFileSystemLoader.cs
  // L75-77; a missing section raises MissingFieldsException).
  const yaml::MiniYaml* yaml_system = find_section("SystemPackages");
  if (yaml_system == nullptr)
    throw meta::MissingFieldsException(std::vector<std::string>{"SystemPackages"});
  mount_packages(*yaml_system);

  // ContentPackages:逐项可失败(L79-90:catch → isContentAvailable=false;
  // Phase 2 无安装流程,失败即忽略)
  // ContentPackages: per-item failure tolerated (L79-90: catch →
  // isContentAvailable=false; Phase 2 has no install flow, failures are
  // ignored).
  if (const yaml::MiniYaml* yaml_content = find_section("ContentPackages")) {
    for (const yaml::MiniYamlNode& node : yaml_content->Nodes) {
      try {
        const std::string str_name = node.Key != nullptr ? *node.Key : std::string{};
        const std::string str_explicit =
            node.Value.Value != nullptr ? *node.Value.Value : std::string{};
        fs_modFiles_.Mount(str_name, str_explicit);
      } catch (...) {
        // 未安装游戏内容:上游记 isContentAvailable=false 后继续
        // Game content not installed: upstream records
        // isContentAvailable=false and carries on.
      }
    }
  }

  // RequiredContentFiles:仅存在性检查触发安装(L92-96);Phase 2 无安装,
  // 跳过
  // RequiredContentFiles: existence checks only trigger the installer
  // (L92-96); Phase 2 skips.
}

Ruleset& ModData::DefaultRules() {
  if (ruleset_default_ == nullptr)
    ruleset_default_ = Ruleset::LoadDefaults(*this);
  return *ruleset_default_;
}

}  // namespace ora::game
