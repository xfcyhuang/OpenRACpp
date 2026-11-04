#pragma once

import std;

namespace ora::game::render {

struct PaletteTable {
  std::array<std::uint32_t, 256> arr4u_colors{};
};

[[nodiscard]] std::expected<PaletteTable, std::string> palette_from_pal_bytes(
    std::span<const std::uint8_t> span1u_bytes, std::string_view str_source_name);

void palette_remap_indexes(PaletteTable& pal_table, std::span<const int> vec4i_transparent,
    std::span<const int> vec4i_shadow);

}
