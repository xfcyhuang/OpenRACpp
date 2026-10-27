#include "formats/png.hpp"

#include <miniz.h>

#include "formats/crc32.hpp"
#include "formats/span_reader.hpp"

namespace ora::fmt {
namespace {

constexpr std::array<std::byte, 8> kSignature = {std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
                                                 std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A}};

enum class PngFilter : std::uint8_t { None, Sub, Up, Average, Paeth };

enum class PngColorType : std::uint8_t { Indexed = 1, Color = 2, Alpha = 4 };

constexpr PngColorType operator|(PngColorType type_a, PngColorType type_b) {
  return static_cast<PngColorType>(static_cast<std::uint8_t>(type_a) | static_cast<std::uint8_t>(type_b));
}

bool IsPaletted(std::uint8_t uint1_bit_depth, PngColorType type_color) {
  if (uint1_bit_depth <= 8 && type_color == (PngColorType::Indexed | PngColorType::Color))
    return true;

  if (uint1_bit_depth == 8 && type_color == (PngColorType::Color | PngColorType::Alpha))
    return false;

  if (uint1_bit_depth == 8 && type_color == PngColorType::Color)
    return false;

  throw std::runtime_error("Unknown pixel format");
}

std::byte Average(std::byte a, std::byte b) {
  return static_cast<std::byte>((std::to_integer<int>(a) + std::to_integer<int>(b)) / 2);
}

std::byte AddBytes(std::byte a, std::byte b) {
  return static_cast<std::byte>(static_cast<std::uint8_t>(std::to_integer<int>(a) + std::to_integer<int>(b)));
}

std::byte Paeth(std::byte a, std::byte b, std::byte c) {
  const auto int_a = std::to_integer<int>(a);
  const auto int_b = std::to_integer<int>(b);
  const auto int_c = std::to_integer<int>(c);
  const auto int4_p = int_a + int_b - int_c;
  const auto int4_pa = std::abs(int4_p - int_a);
  const auto int4_pb = std::abs(int4_p - int_b);
  const auto int4_pc = std::abs(int4_p - int_c);

  return (int4_pa <= int4_pb && int4_pa <= int4_pc) ? a : (int4_pb <= int4_pc) ? b : c;
}

std::int32_t DivRoundAway(std::int32_t int4_a, std::int32_t int4_b) { return (int4_a + int4_b - 1) / int4_b; }

void SkipClamped(SpanReader& reader_source, std::int64_t int8_count) {
  reader_source.Seek(std::min(reader_source.Length(), reader_source.Position() + int8_count));
}

std::uint32_t ReadBE32(const std::byte* ptr) {
  return (std::to_integer<std::uint32_t>(ptr[0]) << 24) | (std::to_integer<std::uint32_t>(ptr[1]) << 16) |
         (std::to_integer<std::uint32_t>(ptr[2]) << 8) | std::to_integer<std::uint32_t>(ptr[3]);
}

std::vector<std::byte> PullIdatChain(SpanReader& reader_source, std::int32_t int4_initial_len) {
  auto vec_out = std::vector<std::byte>{};
  auto int4_remaining = int4_initial_len;
  auto b_eof = false;

  while (!b_eof) {
    if (int4_remaining == 0) {

      if (reader_source.Position() + 12 > reader_source.Length())
        throw std::runtime_error("Invalid PNG file - no end chunk found.");
      SkipClamped(reader_source, 4);
      const auto span_header = reader_source.ReadBytes(8);
      if (ReadBE32(span_header.data() + 4) == Png::ChunkIDAT) {
        int4_remaining = static_cast<std::int32_t>(ReadBE32(span_header.data()));
      } else {

        reader_source.Skip(-8);
        b_eof = true;
        break;
      }
    }

    const auto int4_to_read = static_cast<std::int32_t>(
        std::min<std::int64_t>(int4_remaining, reader_source.Length() - reader_source.Position()));
    if (int4_to_read <= 0)
      throw std::runtime_error("Unexpected end of stream in IDAT chunk.");

    const auto span_chunk = reader_source.ReadBytes(int4_to_read);
    vec_out.insert(vec_out.end(), span_chunk.begin(), span_chunk.end());
    int4_remaining -= int4_to_read;
  }

  return vec_out;
}

std::vector<std::byte> ZlibInflate(const std::vector<std::byte>& vec_src) {
  if (vec_src.empty())
    throw std::runtime_error("ZLibException: unexpected end of zlib stream");

  auto uint8_dest_len = static_cast<std::size_t>(
      std::max<std::int64_t>(1024, static_cast<std::int64_t>(vec_src.size()) * 8));
  while (true) {
    auto vec_dest = std::vector<std::byte>(uint8_dest_len);
    auto mz8_out_len = static_cast<mz_ulong>(uint8_dest_len);
    const auto int4_status = mz_uncompress(reinterpret_cast<unsigned char*>(vec_dest.data()), &mz8_out_len,
                                           reinterpret_cast<const unsigned char*>(vec_src.data()),
                                           static_cast<mz_ulong>(vec_src.size()));
    if (int4_status == MZ_OK) {
      vec_dest.resize(static_cast<std::size_t>(mz8_out_len));
      return vec_dest;
    }
    if (int4_status == MZ_BUF_ERROR && uint8_dest_len < (std::size_t{1} << 32)) {
      uint8_dest_len *= 4;
      continue;
    }
    throw std::runtime_error("ZLibException: corrupt zlib data");
  }
}

void WritePngChunk(std::vector<std::byte>& vec_output, std::uint32_t uint4_type, std::span<const std::byte> vec_input) {
  const auto put_be32 = [&vec_output](std::size_t st_pos, std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_output[st_pos + static_cast<std::size_t>(int4_i)] = static_cast<std::byte>(uint4_v >> (8 * (3 - int4_i)));
  };

  const auto st_header = vec_output.size();
  vec_output.resize(st_header + 8 + vec_input.size() + 4);
  put_be32(st_header, static_cast<std::uint32_t>(vec_input.size()));
  put_be32(st_header + 4, uint4_type);
  std::ranges::copy(vec_input, vec_output.begin() + static_cast<std::ptrdiff_t>(st_header + 8));

  auto uint4_crc = 0xFFFFFFFFu;
  uint4_crc = CRC32::Update(uint4_crc, std::span<const std::byte>{vec_output}.subspan(st_header + 4, 4));
  if (!vec_input.empty())
    uint4_crc = CRC32::Update(uint4_crc, vec_input);
  put_be32(st_header + 8 + vec_input.size(), CRC32::Finish(uint4_crc));
}

char AsciiChar(unsigned char uint1_c) { return uint1_c <= 0x7F ? static_cast<char>(uint1_c) : '?'; }

}

bool Png::Verify(std::span<const std::byte> vec_file) {
  return vec_file.size() >= 8 && std::ranges::equal(vec_file.first(8), std::span<const std::byte, 8>{kSignature});
}

Png::Png(std::span<const std::byte> vec_file) {
  if (!Verify(vec_file))
    throw std::runtime_error("PNG Signature is bogus");

  auto reader_source = SpanReader{vec_file};
  reader_source.Skip(8);

  auto b_header_parsed = false;
  auto b_data_parsed = false;
  auto uint1_bit_depth = std::uint8_t{8};
  type_frame_ = gfx::SpriteFrameType::Rgba32;

  while (true) {
    if (reader_source.Length() - reader_source.Position() < 8)
      throw std::runtime_error("Invalid PNG file - no end chunk found.");
    const auto span_header = reader_source.ReadBytes(8);
    const auto int4_length = static_cast<std::int32_t>(ReadBE32(span_header.data()));
    const auto uint4_type = ReadBE32(span_header.data() + 4);

    if (!b_header_parsed && uint4_type != ChunkIHDR)
      throw std::runtime_error("Invalid PNG file - header does not appear first.");

    switch (uint4_type) {
      case ChunkIHDR: {
        if (b_header_parsed)
          throw std::runtime_error("Invalid PNG file - duplicate header.");

        const auto span_ihdr = reader_source.ReadBytes(13);
        int4_width_ = static_cast<std::int32_t>(ReadBE32(span_ihdr.data()));
        int4_height_ = static_cast<std::int32_t>(ReadBE32(span_ihdr.data() + 4));
        uint1_bit_depth = std::to_integer<std::uint8_t>(span_ihdr[8]);
        const auto type_color = static_cast<PngColorType>(std::to_integer<std::uint8_t>(span_ihdr[9]));

        if (IsPaletted(uint1_bit_depth, type_color))
          type_frame_ = gfx::SpriteFrameType::Indexed8;
        else if (type_color == PngColorType::Color)
          type_frame_ = gfx::SpriteFrameType::Rgb24;

        const auto uint1_compression = std::to_integer<std::uint8_t>(span_ihdr[10]);

        const auto uint1_interlace = std::to_integer<std::uint8_t>(span_ihdr[12]);

        if (uint1_compression != 0)
          throw std::runtime_error("Compression method not supported");

        if (uint1_interlace != 0)
          throw std::runtime_error("Interlacing not supported");

        if (int4_width_ <= 0 || int4_height_ <= 0 ||
            static_cast<std::uint64_t>(int4_width_) * static_cast<std::uint64_t>(int4_height_) *
                    static_cast<std::uint64_t>(PixelStride()) >
                (std::uint64_t{1} << 32))
          throw std::runtime_error("OverflowException");

        vec_data_.assign(static_cast<std::size_t>(int4_width_) * static_cast<std::size_t>(int4_height_) *
                             static_cast<std::size_t>(PixelStride()),
                         std::byte{0});

        SkipClamped(reader_source, 4);
        b_header_parsed = true;
        break;
      }

      case ChunkPLTE: {
        if (int4_length % 3 != 0)
          throw std::runtime_error("Invalid PLTE chunk length.");

        const auto int4_count = int4_length / 3;
        vec_palette_ = std::vector<PngColor>(static_cast<std::size_t>(int4_count));

        const auto span_plte = reader_source.ReadBytes(int4_length);
        for (auto int4_i = 0; int4_i < int4_count; int4_i++) {
          const auto int4_offset = int4_i * 3;
          (*vec_palette_)[static_cast<std::size_t>(int4_i)] =
              PngColor::FromRgb(std::to_integer<std::uint8_t>(span_plte[static_cast<std::size_t>(int4_offset)]),
                                std::to_integer<std::uint8_t>(span_plte[static_cast<std::size_t>(int4_offset + 1)]),
                                std::to_integer<std::uint8_t>(span_plte[static_cast<std::size_t>(int4_offset + 2)]));
        }

        SkipClamped(reader_source, 4);
        break;
      }

      case ChunkTRNS: {
        if (!vec_palette_.has_value())
          throw std::runtime_error("Non-Palette indexed PNG are not supported.");

        const auto span_trns = reader_source.ReadBytes(int4_length);
        for (auto int4_i = 0; int4_i < int4_length && int4_i < static_cast<std::int32_t>((*vec_palette_).size());
             int4_i++)
          (*vec_palette_)[static_cast<std::size_t>(int4_i)] =
              (*vec_palette_)[static_cast<std::size_t>(int4_i)].WithAlpha(
                  std::to_integer<std::uint8_t>(span_trns[static_cast<std::size_t>(int4_i)]));

        SkipClamped(reader_source, 4);
        break;
      }

      case ChunkIDAT:
        if (b_data_parsed)
          throw std::runtime_error("Invalid PNG file - discontinuous IDAT chunks.");

        b_data_parsed = true;
        ProcessIDAT(reader_source, int4_length, uint1_bit_depth);
        break;

      case ChunkTEXT: {
        const auto span_text = reader_source.ReadBytes(int4_length);

        const auto iter_null = std::ranges::find(span_text, std::byte{0});
        if (iter_null == span_text.end())
          return;

        const auto st_null = static_cast<std::size_t>(iter_null - span_text.begin());
        auto str_key = std::string{};
        for (auto st_i = std::size_t{0}; st_i < st_null; st_i++)
          str_key.push_back(AsciiChar(std::to_integer<unsigned char>(span_text[st_i])));
        auto str_value = std::string{};
        for (auto st_i = st_null + 1; st_i < span_text.size(); st_i++)
          str_value.push_back(AsciiChar(std::to_integer<unsigned char>(span_text[st_i])));

        const auto iter_existing = std::ranges::find_if(
            vec_embedded_data_, [&](const auto& pair_kv) { return pair_kv.first == str_key; });
        if (iter_existing != vec_embedded_data_.end())
          iter_existing->second = str_value;
        else
          vec_embedded_data_.emplace_back(std::move(str_key), std::move(str_value));

        SkipClamped(reader_source, 4);
        break;
      }

      case ChunkIEND: {
        if (type_frame_ == gfx::SpriteFrameType::Indexed8 && !vec_palette_.has_value())
          throw std::runtime_error("Non-Palette indexed PNG are not supported.");

        SkipClamped(reader_source, 4);
        return;
      }

      default:

        SkipClamped(reader_source, static_cast<std::int64_t>(int4_length) + 4);
        break;
    }
  }
}

void Png::ProcessIDAT(SpanReader& reader_source, std::int32_t int4_first_chunk_length, std::uint8_t uint1_bit_depth) {
  const auto int4_px_stride = PixelStride();
  const auto int4_row_stride = int4_width_ * int4_px_stride;
  const auto int4_pixels_per_byte = 8 / uint1_bit_depth;
  const auto int4_source_row_stride = DivRoundAway(int4_row_stride, int4_pixels_per_byte);

  const auto vec_zsrc = PullIdatChain(reader_source, int4_first_chunk_length);
  const auto vec_ds = ZlibInflate(vec_zsrc);
  std::size_t st_ds_pos = 0;

  auto vec_prev_line = std::vector<std::byte>(static_cast<std::size_t>(int4_row_stride), std::byte{0});
  auto vec_prev = std::span<const std::byte>{vec_prev_line};

  for (auto int4_y = 0; int4_y < int4_height_; int4_y++) {
    if (st_ds_pos >= vec_ds.size())
      break;

    const auto type_filter = static_cast<PngFilter>(std::to_integer<std::uint8_t>(vec_ds[st_ds_pos++]));
    const auto span_line =
        std::span<std::byte>{vec_data_}.subspan(static_cast<std::size_t>(int4_y) * static_cast<std::size_t>(int4_row_stride),
                                                static_cast<std::size_t>(int4_row_stride));

    if (st_ds_pos + static_cast<std::size_t>(int4_source_row_stride) > vec_ds.size())
      throw std::runtime_error("EndOfStream: unexpected end of stream in IDAT row");
    std::ranges::copy_n(vec_ds.begin() + static_cast<std::ptrdiff_t>(st_ds_pos), int4_source_row_stride, span_line.begin());
    st_ds_pos += static_cast<std::size_t>(int4_source_row_stride);

    if (uint1_bit_depth < 8) {
      const auto int4_mask = (1 << uint1_bit_depth) - 1;
      for (auto int4_i = int4_source_row_stride - 1; int4_i >= 0; int4_i--) {
        const auto int4_packed = std::to_integer<int>(span_line[static_cast<std::size_t>(int4_i)]);
        for (auto int4_j = 0; int4_j < int4_pixels_per_byte; int4_j++) {
          const auto int4_dest = int4_i * int4_pixels_per_byte + int4_j;
          if (int4_dest < static_cast<std::int32_t>(span_line.size()))
            span_line[static_cast<std::size_t>(int4_dest)] =
                static_cast<std::byte>((int4_packed >> (8 - (int4_j + 1) * uint1_bit_depth)) & int4_mask);
        }
      }
    }

    switch (type_filter) {
      case PngFilter::None:
        break;
      case PngFilter::Sub:
        for (auto int4_i = int4_px_stride; int4_i < static_cast<std::int32_t>(span_line.size()); int4_i++)
          span_line[static_cast<std::size_t>(int4_i)] =
              std::byte{AddBytes(span_line[static_cast<std::size_t>(int4_i)], span_line[static_cast<std::size_t>(int4_i - int4_px_stride)])};
        break;
      case PngFilter::Up:
        for (auto int4_i = 0; int4_i < static_cast<std::int32_t>(span_line.size()); int4_i++)
          span_line[static_cast<std::size_t>(int4_i)] =
              std::byte{AddBytes(span_line[static_cast<std::size_t>(int4_i)], vec_prev[static_cast<std::size_t>(int4_i)])};
        break;
      case PngFilter::Average:
        for (auto int4_i = 0; int4_i < int4_px_stride; int4_i++)
          span_line[static_cast<std::size_t>(int4_i)] =
              std::byte{AddBytes(span_line[static_cast<std::size_t>(int4_i)], Average(std::byte{0}, vec_prev[static_cast<std::size_t>(int4_i)]))};
        for (auto int4_i = int4_px_stride; int4_i < static_cast<std::int32_t>(span_line.size()); int4_i++)
          span_line[static_cast<std::size_t>(int4_i)] = std::byte{
              AddBytes(span_line[static_cast<std::size_t>(int4_i)], Average(span_line[static_cast<std::size_t>(int4_i - int4_px_stride)],
                                                    vec_prev[static_cast<std::size_t>(int4_i)]))};
        break;
      case PngFilter::Paeth:
        for (auto int4_i = 0; int4_i < int4_px_stride; int4_i++)
          span_line[static_cast<std::size_t>(int4_i)] =
              std::byte{AddBytes(span_line[static_cast<std::size_t>(int4_i)], Paeth(std::byte{0}, vec_prev[static_cast<std::size_t>(int4_i)], std::byte{0}))};
        for (auto int4_i = int4_px_stride; int4_i < static_cast<std::int32_t>(span_line.size()); int4_i++)
          span_line[static_cast<std::size_t>(int4_i)] = std::byte{
              AddBytes(span_line[static_cast<std::size_t>(int4_i)], Paeth(span_line[static_cast<std::size_t>(int4_i - int4_px_stride)],
                                                  vec_prev[static_cast<std::size_t>(int4_i)],
                                                  vec_prev[static_cast<std::size_t>(int4_i - int4_px_stride)]))};
        break;
      default:
        throw std::runtime_error("Unsupported Filter");
    }

    vec_prev = span_line;
  }

}

Png::Png(std::vector<std::byte> vec_data, gfx::SpriteFrameType type_frame, int int4_width, int int4_height,
         std::optional<std::vector<PngColor>> vec_palette, std::vector<std::pair<std::string, std::string>> vec_embedded_data)
    : vec_palette_(std::move(vec_palette)), type_frame_(type_frame), vec_embedded_data_(std::move(vec_embedded_data)) {
  auto uint8_expect = static_cast<std::uint64_t>(int4_width) * static_cast<std::uint64_t>(int4_height);
  if (!vec_palette_.has_value())
    uint8_expect *= 4;

  if (vec_data.size() != uint8_expect)
    throw std::runtime_error("Input data does not match expected length");

  int4_width_ = int4_width;
  int4_height_ = int4_height;

  switch (type_frame) {
    case gfx::SpriteFrameType::Indexed8:
    case gfx::SpriteFrameType::Rgba32:
    case gfx::SpriteFrameType::Rgb24:

      vec_data_ = std::move(vec_data);
      if (type_frame_ != gfx::SpriteFrameType::Indexed8)
        vec_palette_.reset();
      break;

    case gfx::SpriteFrameType::Bgra32:
    case gfx::SpriteFrameType::Bgr24: {

      vec_data_.assign(vec_data.size(), std::byte{0});
      const auto int4_stride = PixelStride();
      const auto uint8_elements =
          static_cast<std::size_t>(int4_width) * static_cast<std::size_t>(int4_height) * static_cast<std::size_t>(int4_stride);
      if (type_frame_ == gfx::SpriteFrameType::Bgra32) {
        for (auto st_i = std::size_t{0}; st_i + 3 < uint8_elements; st_i += 4) {
          vec_data_[st_i + 0] = vec_data[st_i + 2];
          vec_data_[st_i + 1] = vec_data[st_i + 1];
          vec_data_[st_i + 2] = vec_data[st_i + 0];
          vec_data_[st_i + 3] = vec_data[st_i + 3];
        }
      } else {
        for (auto st_i = std::size_t{0}; st_i + 2 < uint8_elements; st_i += 3) {
          vec_data_[st_i + 0] = vec_data[st_i + 2];
          vec_data_[st_i + 1] = vec_data[st_i + 1];
          vec_data_[st_i + 2] = vec_data[st_i + 0];
        }
      }
      break;
    }
  }
}

std::vector<std::byte> Png::Save() const {
  auto vec_output = std::vector<std::byte>{};
  vec_output.reserve(vec_data_.size() + 256);
  vec_output.insert(vec_output.end(), kSignature.begin(), kSignature.end());

  {
    auto vec_header = std::vector<std::byte>(13);
    const auto put_be32 = [&vec_header](std::size_t st_pos, std::uint32_t uint4_v) {
      for (auto int4_i = 0; int4_i < 4; int4_i++)
        vec_header[st_pos + static_cast<std::size_t>(int4_i)] = static_cast<std::byte>(uint4_v >> (8 * (3 - int4_i)));
    };
    put_be32(0, static_cast<std::uint32_t>(int4_width_));
    put_be32(4, static_cast<std::uint32_t>(int4_height_));
    vec_header[8] = std::byte{8};

    const auto uint1_color_type =
        type_frame_ == gfx::SpriteFrameType::Indexed8
            ? static_cast<std::uint8_t>(PngColorType::Indexed | PngColorType::Color)
            : type_frame_ == gfx::SpriteFrameType::Rgb24 ? static_cast<std::uint8_t>(PngColorType::Color)
                                                         : static_cast<std::uint8_t>(PngColorType::Color | PngColorType::Alpha);
    vec_header[9] = static_cast<std::byte>(uint1_color_type);

    vec_header[10] = std::byte{0};
    vec_header[11] = std::byte{0};
    vec_header[12] = std::byte{0};

    WritePngChunk(vec_output, ChunkIHDR, vec_header);
  }

  auto b_alpha_palette = false;
  if (vec_palette_.has_value()) {
    auto vec_palette_bytes = std::vector<std::byte>{};
    vec_palette_bytes.reserve((*vec_palette_).size() * 3);
    for (const auto& color_c : *vec_palette_) {
      vec_palette_bytes.push_back(static_cast<std::byte>(color_c.uint1_r));
      vec_palette_bytes.push_back(static_cast<std::byte>(color_c.uint1_g));
      vec_palette_bytes.push_back(static_cast<std::byte>(color_c.uint1_b));
      b_alpha_palette |= color_c.uint1_a > 0;
    }
    WritePngChunk(vec_output, ChunkPLTE, vec_palette_bytes);
  }

  if (b_alpha_palette) {
    auto vec_alpha = std::vector<std::byte>{};
    vec_alpha.reserve((*vec_palette_).size());
    for (const auto& color_c : *vec_palette_)
      vec_alpha.push_back(static_cast<std::byte>(color_c.uint1_a));
    WritePngChunk(vec_output, ChunkTRNS, vec_alpha);
  }

  {
    auto vec_rows = std::vector<std::byte>{};
    const auto int4_row_stride = int4_width_ * PixelStride();
    vec_rows.reserve((static_cast<std::size_t>(int4_row_stride) + 1) * static_cast<std::size_t>(int4_height_));
    for (auto int4_y = 0; int4_y < int4_height_; int4_y++) {
      vec_rows.push_back(std::byte{0});
      vec_rows.insert(vec_rows.end(), vec_data_.begin() + static_cast<std::ptrdiff_t>(int4_y) * int4_row_stride,
                      vec_data_.begin() + static_cast<std::ptrdiff_t>(int4_y + 1) * int4_row_stride);
    }

    auto vec_compressed = std::vector<std::byte>(static_cast<std::size_t>(mz_compressBound(static_cast<mz_ulong>(vec_rows.size()))));
    auto mz8_compressed_len = static_cast<mz_ulong>(vec_compressed.size());
    if (mz_compress2(reinterpret_cast<unsigned char*>(vec_compressed.data()), &mz8_compressed_len,
                     reinterpret_cast<const unsigned char*>(vec_rows.data()), static_cast<mz_ulong>(vec_rows.size()),
                     MZ_BEST_COMPRESSION) != MZ_OK)
      throw std::runtime_error("ZLibException: compression failed");
    vec_compressed.resize(static_cast<std::size_t>(mz8_compressed_len));

    WritePngChunk(vec_output, ChunkIDAT, vec_compressed);
  }

  for (const auto& [str_key, str_value] : vec_embedded_data_) {
    auto vec_text = std::vector<std::byte>{};
    vec_text.reserve(str_key.size() + str_value.size() + 1);
    for (const char char_c : str_key)
      vec_text.push_back(static_cast<std::byte>(AsciiChar(static_cast<unsigned char>(char_c))));
    vec_text.push_back(std::byte{0});
    for (const char char_c : str_value)
      vec_text.push_back(static_cast<std::byte>(AsciiChar(static_cast<unsigned char>(char_c))));
    WritePngChunk(vec_output, ChunkTEXT, vec_text);
  }

  WritePngChunk(vec_output, ChunkIEND, {});
  return vec_output;
}

}
