#pragma once

import std;

#include "game/map_bin.hpp"

namespace ora::game {

struct TerrainTemplate {
  std::uint16_t u2_id{};
  int int4_size_x{1};
  int int4_size_y{1};
  bool b_pickany{false};
  std::vector<std::string> vec_str_images{};
  std::vector<std::optional<std::uint8_t>> vec_opt1u_frame_terrain{};

  [[nodiscard]] bool contains_frame(int int4_index) const {
    return int4_index >= 0 && static_cast<std::size_t>(int4_index) < vec_opt1u_frame_terrain.size();
  }
};

struct Tileset {
  std::string str_id;
  std::string str_name;
  int int4_tile_size_x{24};
  int int4_tile_size_y{24};
  std::vector<std::string> vec_str_types;
  std::vector<TerrainTemplate> vec_templates;
  std::uint8_t u1_default_walkable{};

  [[nodiscard]] const TerrainTemplate* find_template(std::uint16_t u2_id) const {
    const auto it_hit{std::find_if(vec_templates.begin(), vec_templates.end(),
        [&](const TerrainTemplate& tpl) { return tpl.u2_id == u2_id; })};
    return it_hit == vec_templates.end() ? nullptr : &*it_hit;
  }

  [[nodiscard]] std::expected<std::uint8_t, std::string> terrain_index_of_type(std::string_view str_type) const {
    const auto it_hit{std::lower_bound(vec_str_types.begin(), vec_str_types.end(), str_type)};
    if (it_hit == vec_str_types.end() || *it_hit != str_type)
      return std::unexpected(std::format("Tileset '{}' lacks terrain type '{}'", str_id, str_type));
    return static_cast<std::uint8_t>(it_hit - vec_str_types.begin());
  }

  [[nodiscard]] bool tile_is_valid(const TerrainTile& tile) const {
    const TerrainTemplate* tpl{find_template(tile.u2_type)};
    if (tpl == nullptr) return false;
    if (!tpl->contains_frame(tile.u1_index)) return false;
    return tpl->vec_opt1u_frame_terrain[tile.u1_index].has_value();
  }

  [[nodiscard]] std::uint8_t terrain_index_of_tile(const TerrainTile& tile) const {
    const TerrainTemplate* tpl{find_template(tile.u2_type)};
    const std::optional<std::uint8_t> opt1u_terrain{tpl != nullptr && tpl->contains_frame(tile.u1_index)
        ? tpl->vec_opt1u_frame_terrain[tile.u1_index]
        : std::optional<std::uint8_t>{}};
    if (opt1u_terrain.has_value() && *opt1u_terrain != 255u) return *opt1u_terrain;
    return u1_default_walkable;
  }

  [[nodiscard]] TerrainTile default_terrain_tile() const {
    return TerrainTile{vec_templates.front().u2_id, 0};
  }
};

[[nodiscard]] std::expected<Tileset, std::string> tileset_load(
    std::string_view str_yaml_text, std::string_view str_source_name);

}
