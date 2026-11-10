module ora.game.render;

import std;
import ora.game.filesystem;

namespace ora::game::render {

namespace {

constexpr std::uint8_t k1uSignature[8]{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

std::uint32_t be32(const std::uint8_t* p1u) {
  return (static_cast<std::uint32_t>(p1u[0]) << 24) | (static_cast<std::uint32_t>(p1u[1]) << 16)
      | (static_cast<std::uint32_t>(p1u[2]) << 8) | static_cast<std::uint32_t>(p1u[3]);
}

std::uint32_t adler32(std::span<const std::uint8_t> span1u_data) {
  std::uint32_t u4_a{1};
  std::uint32_t u4_b{0};
  for (const std::uint8_t u1u_byte : span1u_data) {
    u4_a = (u4_a + u1u_byte) % 65521;
    u4_b = (u4_b + u4_a) % 65521;
  }
  return (u4_b << 16) | u4_a;
}

std::uint8_t paeth(std::uint8_t u1u_a, std::uint8_t u1u_b, std::uint8_t u1u_c) {
  const int int4_p{static_cast<int>(u1u_a) + static_cast<int>(u1u_b) - static_cast<int>(u1u_c)};
  const int int4_pa{std::abs(int4_p - static_cast<int>(u1u_a))};
  const int int4_pb{std::abs(int4_p - static_cast<int>(u1u_b))};
  const int int4_pc{std::abs(int4_p - static_cast<int>(u1u_c))};
  if (int4_pa <= int4_pb && int4_pa <= int4_pc) return u1u_a;
  if (int4_pb <= int4_pc) return u1u_b;
  return u1u_c;
}

std::uint8_t average(std::uint8_t u1u_a, std::uint8_t u1u_b) {
  return static_cast<std::uint8_t>((static_cast<int>(u1u_a) + static_cast<int>(u1u_b)) / 2);
}

constexpr std::uint32_t premultiply(std::uint32_t u4_argb) {
  const std::uint32_t u4_a{u4_argb >> 24};
  if (u4_a == 255) return u4_argb;
  if (u4_a == 0) return 0;
  std::uint32_t u4_out{u4_a << 24};
  for (const int int4_shift : {16, 8, 0}) {
    const std::uint32_t u4_c{(u4_argb >> int4_shift) & 0xFF};
    const std::uint32_t u4_scaled{u4_c * u4_a + 128};
    u4_out |= ((u4_scaled + (u4_scaled >> 8)) >> 8) << int4_shift;
  }
  return u4_out;
}

}

std::expected<PngImage, std::string> png_decode(std::span<const std::uint8_t> span1u_input) {
  bool b_signature_ok{span1u_input.size() >= 8};
  for (std::size_t sz_i{0}; b_signature_ok && sz_i < 8; ++sz_i)
    if (span1u_input[sz_i] != k1uSignature[sz_i]) b_signature_ok = false;
  if (!b_signature_ok) return std::unexpected(std::string{"PNG Signature is bogus"});

  std::size_t sz_pos{8};
  bool b_header_parsed{};
  int int4_width{};
  int int4_height{};
  std::uint8_t u1u_bit_depth{};
  std::uint8_t u1u_color_type{};
  std::vector<std::array<std::uint32_t, 4>> vec_rgba_palette;
  std::vector<std::uint8_t> vec1u_zlib;
  bool b_data_parsed{};
  bool b_prev_idat{};

  while (sz_pos < span1u_input.size()) {
    if (span1u_input.size() - sz_pos < 8)
      return std::unexpected(std::string{"Invalid PNG file - no end chunk found."});
    const std::uint32_t u4_length{be32(span1u_input.data() + sz_pos)};
    const std::uint32_t u4_type{be32(span1u_input.data() + sz_pos + 4)};
    const std::size_t sz_data_begin{sz_pos + 8};
    if (span1u_input.size() - sz_data_begin < u4_length + 4)
      return std::unexpected(std::string{"Invalid PNG file - no end chunk found."});
    const std::span<const std::uint8_t> span1u_chunk{
        span1u_input.data() + sz_data_begin, static_cast<std::size_t>(u4_length)};

    if (!b_header_parsed && u4_type != 0x49484452u)
      return std::unexpected(std::string{"Invalid PNG file - header does not appear first."});

    if (u4_type == 0x49484452u) {
      if (b_header_parsed) return std::unexpected(std::string{"Invalid PNG file - duplicate header."});
      if (u4_length < 13) return std::unexpected(std::string{"Invalid PNG file - no end chunk found."});
      int4_width = static_cast<int>(be32(span1u_chunk.data()));
      int4_height = static_cast<int>(be32(span1u_chunk.data() + 4));
      u1u_bit_depth = span1u_chunk[8];
      u1u_color_type = span1u_chunk[9];
      const std::uint8_t u1u_compression{span1u_chunk[10]};
      const std::uint8_t u1u_interlace{span1u_chunk[12]};
      if (u1u_compression != 0)
        return std::unexpected(std::string{"Compression method not supported"});
      if (u1u_interlace != 0)
        return std::unexpected(std::string{"Interlacing not supported"});
      const bool b_indexed_low{u1u_color_type == 3 && u1u_bit_depth >= 1 && u1u_bit_depth <= 8};
      const bool b_plain_8{(u1u_color_type == 2 || u1u_color_type == 6) && u1u_bit_depth == 8};
      if (u1u_color_type == 3 ? !b_indexed_low : !b_plain_8)
        return std::unexpected(std::string{"Unknown pixel format"});
      if (int4_width <= 0 || int4_height <= 0)
        return std::unexpected(std::string{"Invalid PNG file - no end chunk found."});
      b_header_parsed = true;
    } else if (u4_type == 0x504C5445u) {
      if (u4_length % 3 != 0) return std::unexpected(std::string{"Invalid PLTE chunk length."});
      vec_rgba_palette.clear();
      vec_rgba_palette.reserve(u4_length / 3);
      for (std::uint32_t u4_i{0}; u4_i < u4_length; u4_i += 3) {
        vec_rgba_palette.push_back({static_cast<std::uint32_t>(span1u_chunk[u4_i]),
                                    static_cast<std::uint32_t>(span1u_chunk[u4_i + 1]),
                                    static_cast<std::uint32_t>(span1u_chunk[u4_i + 2]), 255u});
      }
    } else if (u4_type == 0x74524E53u) {
      if (vec_rgba_palette.empty())
        return std::unexpected(std::string{"Non-Palette indexed PNG are not supported."});
      for (std::uint32_t u4_i{0}; u4_i < u4_length && u4_i < vec_rgba_palette.size(); ++u4_i)
        vec_rgba_palette[u4_i][3] = static_cast<std::uint32_t>(span1u_chunk[u4_i]);
    } else if (u4_type == 0x49444154u) {
      if (b_data_parsed && !b_prev_idat)
        return std::unexpected(std::string{"Invalid PNG file - discontinuous IDAT chunks."});
      b_data_parsed = true;
      vec1u_zlib.insert(vec1u_zlib.end(), span1u_chunk.begin(), span1u_chunk.end());
    } else if (u4_type == 0x49454E44u) {
      if (u1u_color_type == 3 && vec_rgba_palette.empty())
        return std::unexpected(std::string{"Non-Palette indexed PNG are not supported."});
      if (!b_data_parsed)
        return std::unexpected(std::string{"Invalid PNG file - no end chunk found."});
      break;
    }

    b_prev_idat = u4_type == 0x49444154u;
    sz_pos = sz_data_begin + static_cast<std::size_t>(u4_length) + 4;
  }

  if (!b_header_parsed || !b_data_parsed)
    return std::unexpected(std::string{"Invalid PNG file - no end chunk found."});
  if (vec1u_zlib.size() < 6)
    return std::unexpected(std::string{"Invalid PNG file - no end chunk found."});

  const std::span<const std::uint8_t> span1u_deflate{vec1u_zlib.data() + 2, vec1u_zlib.size() - 6};
  auto res_inflated{filesystem::inflate_raw(span1u_deflate)};
  if (!res_inflated) return std::unexpected(res_inflated.error());
  if (adler32(*res_inflated) != be32(vec1u_zlib.data() + vec1u_zlib.size() - 4))
    return std::unexpected(std::string{"Invalid PNG file - corrupt zlib stream."});

  const int int4_px_stride{u1u_color_type == 3 ? 1 : u1u_color_type == 2 ? 3 : 4};
  const std::size_t sz_row_stride{
      static_cast<std::size_t>(int4_width) * static_cast<std::size_t>(int4_px_stride)};
  const int int4_pixels_per_byte{u1u_bit_depth == 0 ? 1 : 8 / static_cast<int>(u1u_bit_depth)};
  const std::size_t sz_source_row_stride{
      int4_pixels_per_byte <= 1
          ? sz_row_stride
          : (sz_row_stride + static_cast<std::size_t>(int4_pixels_per_byte) - 1)
                / static_cast<std::size_t>(int4_pixels_per_byte)};
  const std::size_t sz_expected{sz_source_row_stride * static_cast<std::size_t>(int4_height)
      + static_cast<std::size_t>(int4_height)};
  if (res_inflated->size() != sz_expected)
    return std::unexpected(std::string{"Invalid PNG file - IDAT length mismatch."});

  std::vector<std::uint8_t> vec1u_prev(sz_row_stride, 0);
  std::vector<std::uint8_t> vec1u_line(sz_row_stride);
  PngImage image;
  image.int4_width = int4_width;
  image.int4_height = int4_height;
  image.vec_u4_argb.resize(static_cast<std::size_t>(int4_width) * static_cast<std::size_t>(int4_height));

  std::size_t sz_src{0};
  for (int int4_y{0}; int4_y < int4_height; ++int4_y) {
    const std::uint8_t u1u_filter{(*res_inflated)[sz_src++]};
    for (std::size_t sz_i{0}; sz_i < sz_source_row_stride; ++sz_i)
      vec1u_line[sz_i] = (*res_inflated)[sz_src + sz_i];
    sz_src += sz_source_row_stride;

    if (int4_pixels_per_byte > 1) {
      const std::uint32_t u4_mask{(1u << u1u_bit_depth) - 1};
      for (std::size_t sz_i{sz_source_row_stride}; sz_i-- > 0;) {
        const std::uint8_t u1u_packed{vec1u_line[sz_i]};
        for (int int4_j{0}; int4_j < int4_pixels_per_byte; ++int4_j) {
          const std::size_t sz_dest{sz_i * static_cast<std::size_t>(int4_pixels_per_byte)
              + static_cast<std::size_t>(int4_j)};
          if (sz_dest < sz_row_stride)
            vec1u_line[sz_dest] = static_cast<std::uint8_t>(
                (u1u_packed >> (8 - (int4_j + 1) * static_cast<int>(u1u_bit_depth))) & u4_mask);
        }
      }
    }

    switch (u1u_filter) {
      case 0:
        break;
      case 1:
        for (std::size_t sz_i{static_cast<std::size_t>(int4_px_stride)}; sz_i < sz_row_stride; ++sz_i)
          vec1u_line[sz_i] = static_cast<std::uint8_t>(vec1u_line[sz_i] + vec1u_line[sz_i - int4_px_stride]);
        break;
      case 2:
        for (std::size_t sz_i{0}; sz_i < sz_row_stride; ++sz_i)
          vec1u_line[sz_i] = static_cast<std::uint8_t>(vec1u_line[sz_i] + vec1u_prev[sz_i]);
        break;
      case 3:
        for (std::size_t sz_i{0}; sz_i < static_cast<std::size_t>(int4_px_stride); ++sz_i)
          vec1u_line[sz_i] = static_cast<std::uint8_t>(
              vec1u_line[sz_i] + average(0, vec1u_prev[sz_i]));
        for (std::size_t sz_i{static_cast<std::size_t>(int4_px_stride)}; sz_i < sz_row_stride; ++sz_i)
          vec1u_line[sz_i] = static_cast<std::uint8_t>(
              vec1u_line[sz_i] + average(vec1u_line[sz_i - int4_px_stride], vec1u_prev[sz_i]));
        break;
      case 4:
        for (std::size_t sz_i{0}; sz_i < static_cast<std::size_t>(int4_px_stride); ++sz_i)
          vec1u_line[sz_i] = static_cast<std::uint8_t>(
              vec1u_line[sz_i] + paeth(0, vec1u_prev[sz_i], 0));
        for (std::size_t sz_i{static_cast<std::size_t>(int4_px_stride)}; sz_i < sz_row_stride; ++sz_i)
          vec1u_line[sz_i] = static_cast<std::uint8_t>(vec1u_line[sz_i]
              + paeth(vec1u_line[sz_i - int4_px_stride], vec1u_prev[sz_i],
                      vec1u_prev[sz_i - int4_px_stride]));
        break;
      default:
        return std::unexpected(std::string{"Unsupported Filter"});
    }

    for (std::size_t sz_i{0}; sz_i < sz_row_stride; ++sz_i) vec1u_prev[sz_i] = vec1u_line[sz_i];

    std::size_t sz_px{0};
    for (int int4_x{0}; int4_x < int4_width; ++int4_x) {
      std::uint32_t u4_pixel{};
      if (u1u_color_type == 3) {
        const std::size_t sz_index{vec1u_line[sz_px++]};
        if (sz_index >= vec_rgba_palette.size())
          return std::unexpected(std::string{"Invalid PNG file - palette index out of range."});
        const auto& rgba{vec_rgba_palette[sz_index]};
        u4_pixel = (rgba[3] << 24) | (rgba[0] << 16) | (rgba[1] << 8) | rgba[2];
      } else {
        const std::uint32_t u4_r{vec1u_line[sz_px++]};
        const std::uint32_t u4_g{vec1u_line[sz_px++]};
        const std::uint32_t u4_b{vec1u_line[sz_px++]};
        const std::uint32_t u4_a{u1u_color_type == 6 ? vec1u_line[sz_px++] : 255u};
        u4_pixel = (u4_a << 24) | (u4_r << 16) | (u4_g << 8) | u4_b;
      }
      image.vec_u4_argb[static_cast<std::size_t>(int4_y) * static_cast<std::size_t>(int4_width)
          + static_cast<std::size_t>(int4_x)] = premultiply(u4_pixel);
    }
  }

  return image;
}

}
