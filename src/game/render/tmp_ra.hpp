#pragma once

import std;

namespace ora::game::render {

struct TmpRaFrame {
  int int4_width{};
  int int4_height{};
  std::vector<std::uint8_t> vec1u_data;
};

struct TmpRaSprite {
  int int4_width{};
  int int4_height{};
  std::vector<TmpRaFrame> vec_frames;
};

[[nodiscard]] std::expected<TmpRaSprite, std::string> tmp_ra_load(
    std::span<const std::uint8_t> span1u_bytes, std::string_view str_source_name);

}
