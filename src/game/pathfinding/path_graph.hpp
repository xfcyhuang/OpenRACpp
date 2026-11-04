#pragma once

import std;

#include "game/map_bin.hpp"
#include "game/miniyaml.hpp"
#include "game/pathfinding/priority_queue.hpp"
#include "game/tileset.hpp"

namespace ora::game::pathfinding {

constexpr std::int16_t kCostUnreachable{32767};

struct LocomotorTerrainSpeed {
  std::string str_type;
  std::int32_t int4_speed{};
  std::int32_t int4_cost{};
};

struct LocomotorDef {
  std::string str_name;
  std::vector<LocomotorTerrainSpeed> vec_speeds;
};

struct LocomotorMap {
  std::int32_t int4_width{};
  std::int32_t int4_height{};
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};
  std::int32_t int4_min_cost{};
  std::vector<std::int16_t> vec2_cost;
  std::vector<std::int32_t> vec4_speed;
};

[[nodiscard]] std::expected<LocomotorDef, std::string> locomotor_def_from_node(
    const MiniYamlNode& node_locomotor);

[[nodiscard]] LocomotorMap locomotor_map_build(const MapBin& bin_map, const Tileset& tileset,
                                               const LocomotorDef& def_locomotor, std::int32_t int4_left,
                                               std::int32_t int4_top, std::int32_t int4_right,
                                               std::int32_t int4_bottom);

[[nodiscard]] bool map_contains(const LocomotorMap& map, CPos cell);

[[nodiscard]] std::int16_t movement_cost_for_cell(const LocomotorMap& map, CPos cell);

[[nodiscard]] std::int32_t terrain_speed_for_cell(const LocomotorMap& map, CPos cell);

}  // namespace ora::game::pathfinding
