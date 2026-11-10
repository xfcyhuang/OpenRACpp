#pragma once

import std;

namespace ora::game::render {

struct PngImage {
  int int4_width{};
  int int4_height{};
  std::vector<std::uint32_t> vec_u4_argb;
};

[[nodiscard]] std::expected<PngImage, std::string> png_decode(std::span<const std::uint8_t> span1u_input);

}
