#pragma once

import std;

namespace ora::game::render {

struct ShpTdFrame {
  int int4_width{};
  int int4_height{};
  int int4_off_x{};
  int int4_off_y{};
  bool b_trimmed{};
  std::vector<std::uint8_t> vec1u_data;
};

struct ShpTdSprite {
  int int4_frame_width{};
  int int4_frame_height{};
  std::vector<ShpTdFrame> vec_frames;
};

[[nodiscard]] std::expected<ShpTdSprite, std::string> shp_td_load(
    std::span<const std::uint8_t> span1u_bytes, std::string_view str_source_name);

}
