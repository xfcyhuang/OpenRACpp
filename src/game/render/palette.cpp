#include "game/render/palette.hpp"

namespace ora::game::render {

std::expected<PaletteTable, std::string> palette_from_pal_bytes(
    std::span<const std::uint8_t> span1u_bytes, std::string_view str_source_name) {
  if (span1u_bytes.size() != 256 * 3)
    return std::unexpected(std::format("{}: pal 字节数 {} != 768", str_source_name, span1u_bytes.size()));

  PaletteTable pal_out;
  for (int int4_i{0}; int4_i < 256; ++int4_i) {
    const std::size_t sz_base{static_cast<std::size_t>(int4_i) * 3};
    std::uint8_t u1_r{static_cast<std::uint8_t>(span1u_bytes[sz_base] << 2)};
    std::uint8_t u1_g{static_cast<std::uint8_t>(span1u_bytes[sz_base + 1] << 2)};
    std::uint8_t u1_b{static_cast<std::uint8_t>(span1u_bytes[sz_base + 2] << 2)};
    u1_r = static_cast<std::uint8_t>(u1_r | (u1_r >> 6));
    u1_g = static_cast<std::uint8_t>(u1_g | (u1_g >> 6));
    u1_b = static_cast<std::uint8_t>(u1_b | (u1_b >> 6));
    pal_out.arr4u_colors[static_cast<std::size_t>(int4_i)] =
        (255u << 24) | (static_cast<std::uint32_t>(u1_r) << 16)
        | (static_cast<std::uint32_t>(u1_g) << 8) | static_cast<std::uint32_t>(u1_b);
  }
  return pal_out;
}

void palette_remap_indexes(PaletteTable& pal_table, std::span<const int> vec4i_transparent,
    std::span<const int> vec4i_shadow) {
  for (const int int4_i : vec4i_transparent)
    pal_table.arr4u_colors[static_cast<std::size_t>(int4_i)] = 0;
  for (const int int4_i : vec4i_shadow)
    pal_table.arr4u_colors[static_cast<std::size_t>(int4_i)] = 140u << 24;
}

}
