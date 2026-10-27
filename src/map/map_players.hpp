#pragma once
import std;

#include "core/color.hpp"
#include "map/cell_region.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::map {

struct PlayerReference {
  std::string Name{};
  std::string Palette{};
  std::string Bot{};
  std::string StartingUnitsClass{};
  bool AllowBots{true};
  bool Playable{false};
  bool Required{false};
  bool OwnsWorld{false};
  bool Spectating{false};
  bool NonCombatant{false};

  bool LockFaction{false};
  std::string Faction{};

  bool LockColor{false};
  core::Color Color{core::Color::FromArgbRaw(0xFF4B4B4B)};

  CPos HomeLocation{CPos::Zero()};
  bool LockSpawn{false};
  int Spawn{0};
  bool LockTeam{false};
  int Team{0};
  bool LockHandicap{false};
  int Handicap{0};

  std::vector<std::string> Allies{};
  std::vector<std::string> Enemies{};

  PlayerReference() = default;
  explicit PlayerReference(const yaml::MiniYaml& my,
                           core::Color color_default_fallback);

  std::string ToString() const { return Name; }
};

class MapPlayers final {
 public:
  static constexpr int kMaximumPlayerCount = 63;

  std::vector<std::pair<std::string, PlayerReference>> Players{};

  MapPlayers() = default;

  MapPlayers(std::span<const yaml::MiniYamlNode> player_definitions,
             core::Color color_default_fallback);

  const PlayerReference* Find(std::string_view name) const;

};

}
