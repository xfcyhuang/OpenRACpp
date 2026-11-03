#include "game/render/tmp_ra.hpp"

namespace ora::game::render {

namespace {

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> span1u, std::size_t sz_off) {
  return static_cast<std::uint16_t>(span1u[sz_off]) | (static_cast<std::uint16_t>(span1u[sz_off + 1]) << 8);
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> span1u, std::size_t sz_off) {
  return static_cast<std::uint32_t>(span1u[sz_off]) | (static_cast<std::uint32_t>(span1u[sz_off + 1]) << 8)
      | (static_cast<std::uint32_t>(span1u[sz_off + 2]) << 16)
      | (static_cast<std::uint32_t>(span1u[sz_off + 3]) << 24);
}

}

std::expected<TmpRaSprite, std::string> tmp_ra_load(
    std::span<const std::uint8_t> span1u_bytes, std::string_view str_source_name) {
  if (span1u_bytes.size() < 40)
    return std::unexpected(std::format("{}: TmpRA 头不足 40 字节", str_source_name));
  if (read_u32(span1u_bytes, 20) != 0 || read_u16(span1u_bytes, 26) != 0x2c73)
    return std::unexpected(std::format("{}: 非 TmpRA 签名", str_source_name));

  TmpRaSprite sprite_out;
  sprite_out.int4_width = read_u16(span1u_bytes, 0);
  sprite_out.int4_height = read_u16(span1u_bytes, 2);
  const std::uint32_t u4_img_start{read_u32(span1u_bytes, 16)};
  const std::int32_t int4_index_end{static_cast<std::int32_t>(read_u32(span1u_bytes, 28))};
  const std::int32_t int4_index_start{static_cast<std::int32_t>(read_u32(span1u_bytes, 36))};

  if (int4_index_start < 0 || int4_index_end < int4_index_start
      || static_cast<std::size_t>(int4_index_end) > span1u_bytes.size())
    return std::unexpected(
        std::format("{}: TmpRA 帧索引区 [{}, {}) 非法", str_source_name, int4_index_start, int4_index_end));

  const std::size_t sz_count{static_cast<std::size_t>(int4_index_end - int4_index_start)};
  const std::size_t sz_tile{static_cast<std::size_t>(sprite_out.int4_width)
      * static_cast<std::size_t>(sprite_out.int4_height)};
  sprite_out.vec_frames.resize(sz_count);
  for (std::size_t sz_i{0}; sz_i < sz_count; ++sz_i) {
    const std::uint8_t u1_b{span1u_bytes[static_cast<std::size_t>(int4_index_start) + sz_i]};
    if (u1_b == 255) continue;
    const std::size_t sz_offset{static_cast<std::size_t>(u4_img_start)
        + static_cast<std::size_t>(u1_b) * sz_tile};
    if (sz_offset + sz_tile > span1u_bytes.size())
      return std::unexpected(std::format("{}: TmpRA 帧 {} 像素区越界", str_source_name, sz_i));
    sprite_out.vec_frames[sz_i].int4_width = sprite_out.int4_width;
    sprite_out.vec_frames[sz_i].int4_height = sprite_out.int4_height;
    sprite_out.vec_frames[sz_i].vec1u_data.assign(span1u_bytes.begin() + static_cast<std::ptrdiff_t>(sz_offset),
        span1u_bytes.begin() + static_cast<std::ptrdiff_t>(sz_offset + sz_tile));
  }
  return sprite_out;
}

}
