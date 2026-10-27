#pragma once
import std;

#include "game/actor_info.hpp"
#include "game/game_records.hpp"
#include "fs/file_system.hpp"
#include "terrain/terrain_info.hpp"

namespace ora::game {

struct MapFileSystemFace {
  std::function<std::vector<char>(const std::string&)> fn_open;
  std::function<bool(const std::string&, std::vector<char>&)> fn_try_open;
  std::function<bool(const std::string&)> fn_exists;
};

class Ruleset final {
 public:

  static constexpr std::array<std::string_view, 4> kSystemActors{
      "player", "editorplayer", "world", "editorworld"};

  Ruleset(std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>> vec_actors,
          std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>> vec_weapons,
          std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_voices,
          std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_notifications,
          std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>> vec_music,
          std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences,
          const map::ITerrainInfo* terrain_info = nullptr);

  const ActorInfo* FindActor(std::string_view str_key) const;
  const WeaponInfo* FindWeapon(std::string_view str_key) const;

  Ruleset() = default;

  const map::ITerrainInfo& TerrainInfo() const { return *terrain_info_; }

  const std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>>& Actors() const {
    return borrowed_actors_ != nullptr ? borrowed_actors_->vec_actors_
                                       : vec_actors_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>>& Weapons() const {
    return borrowed_weapons_ != nullptr ? borrowed_weapons_->vec_weapons_
                                        : vec_weapons_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>>& Voices() const {
    return borrowed_voices_ != nullptr ? borrowed_voices_->vec_voices_
                                       : vec_voices_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>>& Notifications() const {
    return borrowed_notifications_ != nullptr
               ? borrowed_notifications_->vec_notifications_
               : vec_notifications_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>>& Music() const {
    return borrowed_music_ != nullptr ? borrowed_music_->vec_music_
                                      : vec_music_;
  }
  const std::vector<std::pair<std::string, yaml::MiniYamlNode>>& ModelSequences() const {
    return borrowed_modelSequences_ != nullptr
               ? borrowed_modelSequences_->vec_modelSequences_
               : vec_modelSequences_;
  }

  static std::unique_ptr<Ruleset> LoadDefaults(class ModData& mod_data);

  static std::unique_ptr<Ruleset> Load(
      ModData& mod_data, const MapFileSystemFace& map_files,
      std::string_view str_tile_set, const yaml::MiniYaml* map_rules,
      const yaml::MiniYaml* map_weapons, const yaml::MiniYaml* map_voices,
      const yaml::MiniYaml* map_notifications, const yaml::MiniYaml* map_music,
      const yaml::MiniYaml* map_model_sequences);

  static std::unique_ptr<Ruleset> LoadDefaultsForTileSet(
      ModData& mod_data, std::string_view str_tile_set);

  static bool AnyCustomYaml(const yaml::MiniYaml* yaml);

  static bool AnyFlaggedTraits(ModData& mod_data,
                               const std::vector<yaml::MiniYamlNode>& vec_actors);

  static bool DefinesUnsafeCustomRules(
      ModData& mod_data, const MapFileSystemFace& map_files,
      const yaml::MiniYaml* map_rules, const yaml::MiniYaml* map_weapons,
      const yaml::MiniYaml* map_voices, const yaml::MiniYaml* map_notifications,
      const yaml::MiniYaml* map_sequences);

 private:
  std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>> vec_actors_;
  std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>> vec_weapons_;
  std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_voices_;
  std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_notifications_;
  std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>> vec_music_;
  std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences_;
  const map::ITerrainInfo* terrain_info_ = nullptr;

  const Ruleset* borrowed_actors_ = nullptr;
  const Ruleset* borrowed_weapons_ = nullptr;
  const Ruleset* borrowed_voices_ = nullptr;
  const Ruleset* borrowed_notifications_ = nullptr;
  const Ruleset* borrowed_music_ = nullptr;
  const Ruleset* borrowed_modelSequences_ = nullptr;
};

}
