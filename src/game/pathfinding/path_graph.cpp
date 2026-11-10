module ora.game.pathfinding;

import std;
import ora.game;
import ora.game.miniyaml;

namespace ora::game::pathfinding {

namespace {

const MiniYamlNode* find_child(const std::vector<MiniYamlNode>& vec_nodes, std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

std::optional<std::int32_t> parse_int(std::string_view str_value) {
  std::int32_t int4_out{};
  const char* ptr_first{str_value.data()};
  const char* ptr_last{str_value.data() + str_value.size()};
  const auto res{std::from_chars(ptr_first, ptr_last, int4_out)};
  if (res.ec != std::errc{} || res.ptr != ptr_last) return std::nullopt;
  return int4_out;
}

}

std::expected<LocomotorDef, std::string> locomotor_def_from_node(const MiniYamlNode& node_locomotor) {
  LocomotorDef def;
  if (const MiniYamlNode* node_name{find_child(node_locomotor.vec_children, "Name")})
    def.str_name = node_name->str_value;

  const MiniYamlNode* node_speeds{find_child(node_locomotor.vec_children, "TerrainSpeeds")};
  if (node_speeds == nullptr)
    return std::unexpected("locomotor " + def.str_name + " lacks TerrainSpeeds");

  for (const MiniYamlNode& node_type : node_speeds->vec_children) {
    const auto opt_speed{parse_int(node_type.str_value)};
    if (!opt_speed.has_value())
      return std::unexpected("locomotor " + def.str_name + " bad speed for " + node_type.str_key);
    if (*opt_speed <= 0) continue;

    LocomotorTerrainSpeed entry;
    entry.str_type = node_type.str_key;
    entry.int4_speed = *opt_speed;
    entry.int4_cost = 10000 / *opt_speed;
    if (const MiniYamlNode* node_cost{find_child(node_type.vec_children, "PathingCost")}) {
      const auto opt_cost{parse_int(node_cost->str_value)};
      if (!opt_cost.has_value())
        return std::unexpected("locomotor " + def.str_name + " bad PathingCost for " + node_type.str_key);
      entry.int4_cost = *opt_cost;
    }
    def.vec_speeds.push_back(std::move(entry));
  }

  return def;
}

LocomotorMap locomotor_map_build(const MapBin& bin_map, const Tileset& tileset,
                                 const LocomotorDef& def_locomotor, std::int32_t int4_left,
                                 std::int32_t int4_top, std::int32_t int4_right,
                                 std::int32_t int4_bottom) {
  LocomotorMap map;
  map.int4_width = bin_map.int4_width;
  map.int4_height = bin_map.int4_height;
  map.int4_left = int4_left;
  map.int4_top = int4_top;
  map.int4_right = int4_right;
  map.int4_bottom = int4_bottom;

  std::vector<std::int16_t> vec2_cost_by_idx(tileset.vec_str_types.size(), kCostUnreachable);
  std::vector<std::int32_t> vec4_speed_by_idx(tileset.vec_str_types.size(), 0);
  for (const LocomotorTerrainSpeed& speed : def_locomotor.vec_speeds) {
    const auto res_idx{tileset.terrain_index_of_type(speed.str_type)};
    if (!res_idx.has_value()) continue;
    vec2_cost_by_idx[*res_idx] = static_cast<std::int16_t>(speed.int4_cost);
    vec4_speed_by_idx[*res_idx] = speed.int4_speed;
  }

  map.int4_min_cost = kCostUnreachable;
  for (const LocomotorTerrainSpeed& speed : def_locomotor.vec_speeds)
    if (speed.int4_cost < map.int4_min_cost) map.int4_min_cost = speed.int4_cost;

  map.vec2_cost.assign(static_cast<std::size_t>(map.int4_width) * map.int4_height, 0);
  map.vec4_speed.assign(static_cast<std::size_t>(map.int4_width) * map.int4_height, 0);
  for (std::int32_t int4_x{0}; int4_x < map.int4_width; ++int4_x)
    for (std::int32_t int4_y{0}; int4_y < map.int4_height; ++int4_y) {
      const std::size_t idx{static_cast<std::size_t>(int4_x) * map.int4_height + int4_y};
      const std::uint8_t u1_terrain{tileset.terrain_index_of_tile(bin_map.tile_at(int4_x, int4_y))};
      map.vec2_cost[idx] = vec2_cost_by_idx[u1_terrain];
      map.vec4_speed[idx] = vec4_speed_by_idx[u1_terrain];
    }

  return map;
}

bool map_contains(const LocomotorMap& map, CPos cell) {
  return cell.int4_x >= map.int4_left && cell.int4_x < map.int4_right &&
         cell.int4_y >= map.int4_top && cell.int4_y < map.int4_bottom;
}

std::int16_t movement_cost_for_cell(const LocomotorMap& map, CPos cell) {
  if (!map_contains(map, cell)) return kCostUnreachable;
  return map.vec2_cost[static_cast<std::size_t>(cell.int4_x) * map.int4_height + cell.int4_y];
}

std::int32_t terrain_speed_for_cell(const LocomotorMap& map, CPos cell) {
  if (!map_contains(map, cell)) return 0;
  return map.vec4_speed[static_cast<std::size_t>(cell.int4_x) * map.int4_height + cell.int4_y];
}

}
