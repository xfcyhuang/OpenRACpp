#pragma once

import std;

namespace ora::game {

struct TerrainTile {
  std::uint16_t u2_type{};
  std::uint8_t u1_index{};
};

struct ResourceTile {
  std::uint8_t u1_type{};
  std::uint8_t u1_density{};
};

struct MapBin {
  int int4_width{};
  int int4_height{};
  std::uint32_t u4_tiles_offset{};
  std::uint32_t u4_heights_offset{};
  std::uint32_t u4_resources_offset{};
  std::vector<TerrainTile> vec_tiles;
  std::vector<ResourceTile> vec_resources;

  [[nodiscard]] const TerrainTile& tile_at(int int4_i, int int4_j) const {
    return vec_tiles[static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_height) + int4_j];
  }

  [[nodiscard]] const ResourceTile& resource_at(int int4_i, int int4_j) const {
    return vec_resources[static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_height) + int4_j];
  }
};

[[nodiscard]] std::expected<MapBin, std::string> map_bin_parse(
    std::span<const std::uint8_t> span1u_data, int int4_expected_width, int int4_expected_height);

}
