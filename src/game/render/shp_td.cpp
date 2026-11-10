#include "game/render/shp_td.hpp"

#include "game/render/lcw.hpp"
#include "game/render/xor_delta.hpp"

namespace ora::game::render {

namespace {

constexpr std::uint8_t kFmtXorPrev{0x20};
constexpr std::uint8_t kFmtXorLcw{0x40};
constexpr std::uint8_t kFmtLcw{0x80};

struct FrameHeader {
  std::uint32_t u4_file_offset{};
  std::uint8_t u1_format{};
  std::uint16_t u2_ref_offset{};
  std::uint16_t u2_ref_format{};
  int int4_ref_index{-1};
  std::vector<std::uint8_t> vec1u_data;
};

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> span1u, std::size_t sz_off) {
  return static_cast<std::uint16_t>(span1u[sz_off]) | (static_cast<std::uint16_t>(span1u[sz_off + 1]) << 8);
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> span1u, std::size_t sz_off) {
  return static_cast<std::uint32_t>(span1u[sz_off]) | (static_cast<std::uint32_t>(span1u[sz_off + 1]) << 8)
      | (static_cast<std::uint32_t>(span1u[sz_off + 2]) << 16)
      | (static_cast<std::uint32_t>(span1u[sz_off + 3]) << 24);
}

[[nodiscard]] bool is_shp_td(std::span<const std::uint8_t> span1u_bytes) {
  const std::uint16_t u2_count{read_u16(span1u_bytes, 0)};
  if (u2_count == 0) return false;
  const std::size_t sz_final_offset{14 + 8 * static_cast<std::size_t>(u2_count)};
  if (sz_final_offset + 4 > span1u_bytes.size()) return false;
  if (read_u32(span1u_bytes, sz_final_offset) != span1u_bytes.size()) return false;
  const std::uint8_t u1_fmt{span1u_bytes[17]};
  return u1_fmt == kFmtXorPrev || u1_fmt == kFmtXorLcw || u1_fmt == kFmtLcw;
}

[[nodiscard]] std::expected<bool, std::string> decompress_frame(
    std::span<const std::uint8_t> span1u_shp_bytes, std::size_t sz_shp_bytes_file_offset,
    std::vector<FrameHeader>& vec_headers, std::size_t sz_index, int int4_width, int int4_height,
    int int4_depth) {
  FrameHeader& hdr{vec_headers[sz_index]};
  if (hdr.u1_format == kFmtXorPrev || hdr.u1_format == kFmtXorLcw) {
    if (int4_depth > static_cast<int>(vec_headers.size()))
      return std::unexpected("Format20/40 headers contain infinite loop");
    if (hdr.int4_ref_index < 0
        || static_cast<std::size_t>(hdr.int4_ref_index) >= vec_headers.size())
      return std::unexpected(std::format("Reference doesn't point to image data {}->{}",
          hdr.u4_file_offset, hdr.u2_ref_offset));

    const std::size_t sz_pixels{static_cast<std::size_t>(int4_width)
        * static_cast<std::size_t>(int4_height)};
    if (vec_headers[static_cast<std::size_t>(hdr.int4_ref_index)].vec1u_data.empty()
        && !(int4_width == 0 || int4_height == 0)) {
      const auto res_ref{decompress_frame(span1u_shp_bytes, sz_shp_bytes_file_offset, vec_headers,
          static_cast<std::size_t>(hdr.int4_ref_index), int4_width, int4_height, int4_depth + 1)};
      if (!res_ref) return std::unexpected(res_ref.error());
    }
    hdr.vec1u_data = vec_headers[static_cast<std::size_t>(hdr.int4_ref_index)].vec1u_data;
    if (hdr.vec1u_data.size() != sz_pixels)
      hdr.vec1u_data.assign(sz_pixels, 0);

    const std::size_t sz_src_offset{hdr.u4_file_offset >= sz_shp_bytes_file_offset
            ? hdr.u4_file_offset - sz_shp_bytes_file_offset
            : 0};
    const auto res_xor{xor_delta_decode_into(span1u_shp_bytes, sz_src_offset, hdr.vec1u_data)};
    if (!res_xor) return std::unexpected(res_xor.error());
    return true;
  }

  if (hdr.u1_format == kFmtLcw) {
    std::vector<std::uint8_t> vec1u_image(static_cast<std::size_t>(int4_width)
        * static_cast<std::size_t>(int4_height), 0);
    const std::size_t sz_src_offset{hdr.u4_file_offset >= sz_shp_bytes_file_offset
            ? hdr.u4_file_offset - sz_shp_bytes_file_offset
            : 0};
    const auto res_lcw{lcw_decode_into(span1u_shp_bytes, sz_src_offset, vec1u_image)};
    if (!res_lcw) return std::unexpected(res_lcw.error());
    hdr.vec1u_data = std::move(vec1u_image);
    return true;
  }

  return std::unexpected(std::format("未知帧格式 0x{:X}", hdr.u1_format));
}

}

std::expected<ShpTdSprite, std::string> shp_td_load(
    std::span<const std::uint8_t> span1u_bytes, std::string_view str_source_name) {
  if (span1u_bytes.size() < 18) return std::unexpected(std::format("{}: 文件过短", str_source_name));
  if (!is_shp_td(span1u_bytes)) return std::unexpected(std::format("{}: 非 ShpTD 签名", str_source_name));

  ShpTdSprite sprite_out;
  const std::size_t sz_count{read_u16(span1u_bytes, 0)};
  sprite_out.int4_frame_width = read_u16(span1u_bytes, 6);
  sprite_out.int4_frame_height = read_u16(span1u_bytes, 8);
  const std::size_t sz_shp_bytes_file_offset{14 + 8 * (sz_count + 2)};
  if (sz_shp_bytes_file_offset > span1u_bytes.size())
    return std::unexpected(std::format("{}: 帧头表越界", str_source_name));
  const std::span<const std::uint8_t> span1u_shp_bytes{span1u_bytes.data() + sz_shp_bytes_file_offset,
      span1u_bytes.size() - sz_shp_bytes_file_offset};

  std::vector<FrameHeader> vec_headers(sz_count);
  for (std::size_t sz_i{0}; sz_i < sz_count; ++sz_i) {
    const std::size_t sz_off{14 + 8 * sz_i};
    FrameHeader& hdr{vec_headers[sz_i]};
    const std::uint32_t u4_data{read_u32(span1u_bytes, sz_off)};
    hdr.u4_file_offset = u4_data & 0xFFFFFF;
    hdr.u1_format = static_cast<std::uint8_t>(u4_data >> 24);
    hdr.u2_ref_offset = read_u16(span1u_bytes, sz_off + 4);
    hdr.u2_ref_format = read_u16(span1u_bytes, sz_off + 6);
  }

  for (std::size_t sz_i{0}; sz_i < sz_count; ++sz_i) {
    FrameHeader& hdr{vec_headers[sz_i]};
    if (hdr.u1_format == kFmtXorPrev) {
      if (sz_i == 0)
        return std::unexpected(std::format("Reference doesn't point to image data {}->{}",
            hdr.u4_file_offset, hdr.u2_ref_offset));
      hdr.int4_ref_index = static_cast<int>(sz_i) - 1;
    } else if (hdr.u1_format == kFmtXorLcw) {
      const auto it_hit{std::find_if(vec_headers.begin(), vec_headers.end(),
          [&](const FrameHeader& h) { return h.u4_file_offset == hdr.u2_ref_offset; })};
      if (it_hit == vec_headers.end())
        return std::unexpected(std::format("Reference doesn't point to image data {}->{}",
            hdr.u4_file_offset, hdr.u2_ref_offset));
      hdr.int4_ref_index = static_cast<int>(it_hit - vec_headers.begin());
    }
  }

  for (std::size_t sz_i{0}; sz_i < sz_count; ++sz_i) {
    if (sprite_out.int4_frame_width == 0 || sprite_out.int4_frame_height == 0) {
      sprite_out.vec_frames.push_back(ShpTdFrame{});
      continue;
    }
    const auto res{decompress_frame(span1u_shp_bytes, sz_shp_bytes_file_offset, vec_headers, sz_i,
        sprite_out.int4_frame_width, sprite_out.int4_frame_height, 0)};
    if (!res) return std::unexpected(res.error());

    ShpTdFrame frm_out;
    const std::vector<std::uint8_t>& vec1u_orig{vec_headers[sz_i].vec1u_data};
    const int int4_w{sprite_out.int4_frame_width};
    const int int4_h{sprite_out.int4_frame_height};
    int int4_top{int4_h - 1};
    int int4_bottom{0};
    int int4_left{int4_w - 1};
    int int4_right{0};
    for (int int4_y{0}; int4_y < int4_h; ++int4_y)
      for (int int4_x{0}; int4_x < int4_w; ++int4_x) {
        if (vec1u_orig[static_cast<std::size_t>(int4_y) * static_cast<std::size_t>(int4_w)
            + static_cast<std::size_t>(int4_x)]
            != 0) {
          if (int4_y < int4_top) int4_top = int4_y;
          if (int4_y > int4_bottom) int4_bottom = int4_y;
          if (int4_x < int4_left) int4_left = int4_x;
          if (int4_x > int4_right) int4_right = int4_x;
        }
      }
    int int4_trim_w{int4_right - int4_left + 1};
    int int4_trim_h{int4_bottom - int4_top + 1};

    if ((int4_trim_w - int4_w) % 2 != 0) {
      if (int4_left > 0)
        --int4_left;
      else
        ++int4_right;
      ++int4_trim_w;
    }
    if ((int4_trim_h - int4_h) % 2 != 0) {
      if (int4_top > 0)
        --int4_top;
      else
        ++int4_bottom;
      ++int4_trim_h;
    }

    if (int4_trim_w == int4_w && int4_trim_h == int4_h) {
      frm_out.int4_width = int4_w;
      frm_out.int4_height = int4_h;
      frm_out.vec1u_data = vec1u_orig;
    } else if (int4_trim_w > 0 && int4_trim_h > 0) {
      frm_out.int4_width = int4_trim_w;
      frm_out.int4_height = int4_trim_h;
      frm_out.int4_off_x = (int4_left + int4_right - int4_w + 1) / 2;
      frm_out.int4_off_y = (int4_top + int4_bottom - int4_h + 1) / 2;
      frm_out.b_trimmed = true;
      frm_out.vec1u_data.reserve(static_cast<std::size_t>(int4_trim_w)
          * static_cast<std::size_t>(int4_trim_h));
      for (int int4_y{0}; int4_y < int4_trim_h; ++int4_y)
        for (int int4_x{0}; int4_x < int4_trim_w; ++int4_x)
          frm_out.vec1u_data.push_back(vec1u_orig[static_cast<std::size_t>(int4_y + int4_top)
                  * static_cast<std::size_t>(int4_w)
              + static_cast<std::size_t>(int4_x + int4_left)]);
    }
    sprite_out.vec_frames.push_back(std::move(frm_out));
  }
  return sprite_out;
}

}
