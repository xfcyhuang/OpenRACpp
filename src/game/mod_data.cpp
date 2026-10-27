import std;
#include "game/mod_data.hpp"
#include "game/platform.hpp"
#include "game/settings.hpp"
#include "fs/d2k_sound_resources.hpp"
#include "fs/mix_file.hpp"
#include "fluent/fluent_provider.hpp"
#include "meta/field_loader.hpp"
#include "terrain/terrain_info.hpp"

namespace ora::game {

namespace {

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

}

InstalledMods::InstalledMods(std::string str_mods_root)
    : str_modsRoot_{std::move(str_mods_root)} {
  namespace stdfs = std::filesystem;

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

ModData::ModData(const Manifest& manifest_source, const InstalledMods& installedMods,
                 std::string str_engine_dir)
    : manifest_{std::make_unique<Manifest>(manifest_source.Id(), manifest_source.Package())},

      fs_modFiles_{MakePackageLoaders(manifest_->PackageFormats())} {

  str_engine_dir_ = str_engine_dir;
  const PathResolver resolver_paths{std::move(str_engine_dir), std::string{}};
  fs_modFiles_.SetPathResolver(
      [resolver_paths](const std::string& str_path) { return resolver_paths(str_path); });
  installedMods.InstallResolver(fs_modFiles_);

  const std::string_view sv_loader = manifest_->FileSystemLoaderName();
  if (sv_loader != "ContentInstallerFileSystem" && sv_loader != "DefaultFileSystemLoader")
    throw std::runtime_error{std::format(
        "Unknown FileSystem loader `{}` (Phase 2 支持集之外)", sv_loader)};

  const yaml::MiniYaml& yaml_fs = manifest_->FileSystemNode();
  const auto mount_packages = [&fileSystem = fs_modFiles_](const yaml::MiniYaml& yaml_node) {

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

  const yaml::MiniYaml* yaml_system = find_section("SystemPackages");
  if (yaml_system == nullptr)
    throw meta::MissingFieldsException(std::vector<std::string>{"SystemPackages"});
  mount_packages(*yaml_system);

  if (const yaml::MiniYaml* yaml_content = find_section("ContentPackages")) {
    for (const yaml::MiniYamlNode& node : yaml_content->Nodes) {
      try {
        const std::string str_name = node.Key != nullptr ? *node.Key : std::string{};
        const std::string str_explicit =
            node.Value.Value != nullptr ? *node.Value.Value : std::string{};
        fs_modFiles_.Mount(str_name, str_explicit);
      } catch (...) {

      }
    }
  }

}

Ruleset& ModData::DefaultRules() {
  if (ruleset_default_ == nullptr)
    ruleset_default_ = Ruleset::LoadDefaults(*this);
  return *ruleset_default_;
}

GameSpeeds& ModData::GetOrCreateGameSpeeds() {
  if (game_speeds_ == nullptr) {
    if (const yaml::MiniYaml* node = manifest_->MergedNode("GameSpeeds"))
      game_speeds_ = std::make_unique<GameSpeeds>(*node);
    else
      game_speeds_ = std::make_unique<GameSpeeds>();
  }
  return *game_speeds_;
}

Fonts& ModData::GetOrCreateFonts() {

  if (fonts_ == nullptr) {
    fonts_ = std::make_unique<Fonts>();
    if (const yaml::MiniYaml* node = manifest_->MergedNode("Fonts"))
      fonts_->vec_font_list = Fonts::Load(*node);
  }
  return *fonts_;
}

const ModCredits& ModData::GetOrCreateModCredits() {
  if (!opt_mod_credits_.has_value())
    opt_mod_credits_ =
        ModCreditsFromYaml(manifest_->GlobalModDataNode("ModCredits"));
  return *opt_mod_credits_;
}

ui::HotkeyManager& ModData::Hotkeys() {
  if (hotkeys_ == nullptr) {
    RawSettingsSection& section_keys = Settings::Instance().RawSection("Keys");
    std::vector<ui::HotkeyManager::SettingsOverride> vec_overrides;
    for (const auto& node : section_keys.Nodes())
      if (node.Key != nullptr && node.Value.Value != nullptr)
        vec_overrides.push_back(
            ui::HotkeyManager::SettingsOverride{*node.Key,
                                                *node.Value.Value});

    hotkeys_ = std::make_unique<ui::HotkeyManager>(
        [this](const std::string& str_file) {
          return fs_modFiles_.Open(str_file);
        },
        manifest_->Hotkeys(), vec_overrides);

    hotkeys_->SetPersistSink(
        [&section_keys](const std::string& str_name,
                        std::optional<std::string> str_value) {
          section_keys.RemoveNodesNamed(str_name);
          if (str_value)
            section_keys.AddNode(str_name, *str_value);
        });
  }
  return *hotkeys_;
}

const PlayerDatabase& ModData::GetOrCreatePlayerDatabase() {
  if (player_database_ == nullptr) {
    if (const yaml::MiniYaml* node =
            manifest_->GlobalModDataNode("PlayerDatabase"))
      player_database_ = std::make_unique<PlayerDatabase>(
          PlayerDatabaseFromYaml(*node));
    else
      player_database_ = std::make_unique<PlayerDatabase>();
  }
  return *player_database_;
}

map::MapGrid& ModData::GetOrCreateMapGrid() {

  if (grid_ == nullptr) {
    if (const yaml::MiniYaml* node = manifest_->MergedNode("MapGrid"))
      grid_ = std::make_unique<map::MapGrid>(*node);
    else
      grid_ = std::make_unique<map::MapGrid>();
  }
  return *grid_;
}

const map::ITerrainInfo& ModData::GetTerrainInfo(std::string_view str_id) {
  TerrainInfos();

  for (const auto& [key, info] : vec_terrain_info_)
    if (key == str_id)
      return *info;

  throw std::runtime_error("The given key was not present in the dictionary.");
}

const std::vector<std::pair<std::string, std::unique_ptr<map::ITerrainInfo>>>&
ModData::TerrainInfos() {

  if (vec_terrain_info_.empty()) {
    const std::string& loader = manifest_->TerrainFormat();

    if (loader != "DefaultTerrain" && loader != "DefaultTerrainLoader")
      throw std::runtime_error(std::format(
          "Unable to find a terrain loader for type '{}'.", loader));

    for (const std::string& file : manifest_->TileSets()) {
      auto terrain = std::make_unique<map::DefaultTerrain>(
          fs_modFiles_, file);
      vec_terrain_info_.emplace_back(terrain->Id(), std::move(terrain));
    }
  }

  return vec_terrain_info_;
}

std::vector<std::vector<yaml::MiniYamlNode>> ModData::GetRulesYaml() {
  yaml::StringPool& pool_rules = yaml::MiniYaml::GlobalPool();
  std::vector<std::vector<yaml::MiniYamlNode>> vec_out;
  for (const std::string& str_file : manifest_->Rules()) {
    const std::vector<char> bytes = fs_modFiles_.Open(str_file);
    vec_out.push_back(yaml::MiniYaml::FromStream(
        std::string_view{bytes.data(), bytes.size()}, str_file, false,
        pool_rules));
  }
  return vec_out;
}

void ModData::InitializeLoaders(const fluent::FileOpenFn& fn_open,
                                const yaml::MiniYaml* ptr_map_fluent_definitions) {
  fluent::FluentProvider::Instance().Initialize(
      manifest_->FluentCulture(), manifest_->FluentMessages(), fn_open,
      ptr_map_fluent_definitions);
}

}
