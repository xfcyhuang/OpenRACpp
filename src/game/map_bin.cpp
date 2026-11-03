#include "game/map_bin.hpp"

namespace ora::game {

namespace {

[[nodiscard]] std::uint16_t u16_le(std::span<const std::uint8_t> span1u_data, std::size_t sz_off) {
  return static_cast<std::uint16_t>(span1u_data[sz_off] | (span1u_data[sz_off + 1] << 8));
}

[[nodiscard]] std::uint32_t u32_le(std::span<const std::uint8_t> span1u_data, std::size_t sz_off) {
  return static_cast<std::uint32_t>(span1u_data[sz_off]) | (static_cast<std::uint32_t>(span1u_data[sz_off + 1]) << 8)
      | (static_cast<std::uint32_t>(span1u_data[sz_off + 2]) << 16)
      | (static_cast<std::uint32_t>(span1u_data[sz_off + 3]) << 24);
}

}

std::expected<MapBin, std::string> map_bin_parse(
    std::span<const std::uint8_t> span1u_data, int int4_expected_width, int int4_expected_height) {
  if (span1u_data.size() < 5) return std::unexpected(std::string{"map.bin 头截断"});

  const int int4_format{span1u_data[0]};
  const int int4_width{u16_le(span1u_data, 1)};
  const int int4_height{u16_le(span1u_data, 3)};
  if (int4_width != int4_expected_width || int4_height != int4_expected_height)
    return std::unexpected(std::string{"Invalid tile data"});

  MapBin bin_out;
  bin_out.int4_width = int4_width;
  bin_out.int4_height = int4_height;

  if (int4_format == 1) {
    bin_out.u4_tiles_offset = 5;
    bin_out.u4_heights_offset = 0;
    bin_out.u4_resources_offset = 5u + 3u * static_cast<std::uint32_t>(int4_width * int4_height);
  } else if (int4_format == 2) {
    if (span1u_data.size() < 17) return std::unexpected(std::string{"map.bin fmt2 头截断"});
    bin_out.u4_tiles_offset = u32_le(span1u_data, 5);
    bin_out.u4_heights_offset = u32_le(span1u_data, 9);
    bin_out.u4_resources_offset = u32_le(span1u_data, 13);
  } else {
    return std::unexpected(std::format("Unknown binary map format '{}'", int4_format));
  }

  const std::size_t sz_cells{static_cast<std::size_t>(int4_width) * static_cast<std::size_t>(int4_height)};

  if (bin_out.u4_tiles_offset > 0) {
    if (static_cast<std::size_t>(bin_out.u4_tiles_offset) + sz_cells * 3u > span1u_data.size())
      return std::unexpected(std::string{"map.bin tile 区截断"});
    bin_out.vec_tiles.resize(sz_cells);
    for (int int4_i{0}; int4_i < int4_width; ++int4_i) {
      for (int int4_j{0}; int4_j < int4_height; ++int4_j) {
        const std::size_t sz_off{bin_out.u4_tiles_offset
            + (static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_height) + int4_j) * 3u};
        std::uint8_t u1_index{static_cast<std::uint8_t>(span1u_data[sz_off + 2])};
        if (u1_index == 255u) u1_index = static_cast<std::uint8_t>(int4_i % 4 + int4_j % 4 * 4);
        bin_out.vec_tiles[static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_height) + int4_j]
            = TerrainTile{u16_le(span1u_data, sz_off), u1_index};
      }
    }
  }

  if (bin_out.u4_resources_offset > 0) {
    if (static_cast<std::size_t>(bin_out.u4_resources_offset) + sz_cells * 2u > span1u_data.size())
      return std::unexpected(std::string{"map.bin resource 区截断"});
    bin_out.vec_resources.resize(sz_cells);
    for (std::size_t sz_c{0}; sz_c < sz_cells; ++sz_c) {
      const std::size_t sz_off{bin_out.u4_resources_offset + sz_c * 2u};
      bin_out.vec_resources[sz_c] = ResourceTile{span1u_data[sz_off], span1u_data[sz_off + 1]};
    }
  }

  return bin_out;
}

}
