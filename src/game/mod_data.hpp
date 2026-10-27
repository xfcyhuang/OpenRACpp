#pragma once
import std;

#include "fluent/fluent_bundle.hpp"

#include "fs/file_system.hpp"
#include "fs/folder.hpp"
#include "game/fonts.hpp"
#include "game/local_player_profile.hpp"
#include "game/manifest.hpp"
#include "game/mod_credits.hpp"
#include "game/ruleset.hpp"
#include "game/game_speed.hpp"
#include "terrain/map_grid.hpp"
#include "terrain/terrain_info.hpp"
#include "ui/hotkey.hpp"

namespace ora::game {

class InstalledMods final {
 public:

  explicit InstalledMods(std::string str_mods_root);

  const Manifest* Find(std::string_view str_id) const;

  void InstallResolver(fs::FileSystem& fileSystem) const;

 private:
  std::string str_modsRoot_;
  std::vector<std::pair<std::string, std::unique_ptr<Manifest>>> vec_mods_;
  std::vector<std::unique_ptr<fs::Folder>> vec_packages_;

};

class ModData final {
 public:

  ModData(const Manifest& manifest_source, const InstalledMods& installedMods,
          std::string str_engine_dir);

  const Manifest& ManifestRef() const { return *manifest_; }
  fs::FileSystem& ModFiles() { return fs_modFiles_; }
  Ruleset& DefaultRules();

  void InitializeLoaders(const fluent::FileOpenFn& fn_open,
                         const yaml::MiniYaml* ptr_map_fluent_definitions = nullptr);

  map::MapGrid& GetOrCreateMapGrid();

  const map::ITerrainInfo& GetTerrainInfo(std::string_view str_id);

  const std::vector<std::pair<std::string, std::unique_ptr<map::ITerrainInfo>>>&
  TerrainInfos();

  GameSpeeds& GetOrCreateGameSpeeds();

  const ModCredits& GetOrCreateModCredits();

  const std::string& EngineDir() const { return str_engine_dir_; }

  Fonts& GetOrCreateFonts();

  ui::HotkeyManager& Hotkeys();
  const PlayerDatabase& GetOrCreatePlayerDatabase();

  std::vector<std::vector<yaml::MiniYamlNode>> GetRulesYaml();

 private:
  std::unique_ptr<Manifest> manifest_;
  fs::FileSystem fs_modFiles_;
  std::unique_ptr<Ruleset> ruleset_default_;
  std::unique_ptr<map::MapGrid> grid_;
  std::unique_ptr<GameSpeeds> game_speeds_;
  std::unique_ptr<Fonts> fonts_;
  std::optional<ModCredits> opt_mod_credits_;
  std::string str_engine_dir_;
  std::unique_ptr<ui::HotkeyManager> hotkeys_;
  std::unique_ptr<PlayerDatabase> player_database_;
  std::vector<std::pair<std::string, std::unique_ptr<map::ITerrainInfo>>>
      vec_terrain_info_;
};

}
