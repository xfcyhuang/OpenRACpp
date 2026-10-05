// UPSTREAM: NONE —— Pfim v0.11.3 逐语义移植(见 dds.hpp 头注的文件清单);
// 上游 OpenRA 消费面 DdsLoader.cs @b6fc03f。RGB565 插值为 float 逐字
// (Lerp + (byte)(x+0.5f) 截断);mip 尺寸的 Width/2^i 走 double 截断;
// CompressedDds 统一走 InMemoryDecode 控制流(D65);D66 = BC4/5/DX10
// 系显式拒绝。
// UPSTREAM: NONE — the verbatim-semantics port of Pfim v0.11.3 (see
// dds.hpp's header note for the file list); the OpenRA consumer face is
// DdsLoader.cs @b6fc03f. The RGB565 interpolation keeps the float math
// verbatim (Lerp + the (byte)(x+0.5f) truncation); mip dimensions use the
// double-truncating Width/2^i; CompressedDds unifies on the InMemoryDecode
// control flow (D65); D66 = the explicit rejection of the BC4/5/DX10
// family.
#include "formats/dds.hpp"

#include "formats/span_reader.hpp"

namespace ora::fmt {
namespace {

/// CompressionAlgorithm(DdsHeader.cs L14-107,枚举值逐值)。
/// CompressionAlgorithm (DdsHeader.cs L14-107, value for value).
enum class CompressionAlgorithm : std::uint32_t {
  None = 0,
  D3DFMT_DXT1 = 827611204,
  D3DFMT_DXT2 = 844388420,
  D3DFMT_DXT3 = 861165636,
  D3DFMT_DXT4 = 877942852,
  D3DFMT_DXT5 = 894720068,
  DX10 = 808540228,
  ATI1 = 826889281,
  BC4U = 1429488450,
  BC4S = 1395934018,
  ATI2 = 843666497,
  BC5U = 1429553986,
  BC5S = 1395999554,
};

/// Pfim.ImageFormat 名(异常文本与帧映射用)。| The Pfim.ImageFormat names
/// (for exception texts and the frame mapping).
enum class PfimFormat { Rgb8, R5g5b5, R5g6b5, R5g5b5a1, Rgba16, Rgb24, Rgba32 };

const char* NameOf(PfimFormat type_format) {
  switch (type_format) {
    case PfimFormat::Rgb8: return "Rgb8";
    case PfimFormat::R5g5b5: return "R5g5b5";
    case PfimFormat::R5g6b5: return "R5g6b5";
    case PfimFormat::R5g5b5a1: return "R5g5b5a1";
    case PfimFormat::Rgba16: return "Rgba16";
    case PfimFormat::Rgb24: return "Rgb24";
    case PfimFormat::Rgba32: return "Rgba32";
  }
  return "";
}

/// DdsPixelFormatFlags(DdsHeader.cs L149-217,仅 AlphaPixels 位被解码路径
/// 观测)。| DdsPixelFormatFlags (DdsHeader.cs L149-217; only the
/// AlphaPixels bit is observable on the decode path).
constexpr std::uint32_t kAlphaPixels = 0x1;

/// Util.Stride(Util.cs L241-253):4 字节对齐;两条 ArgumentException 消息
/// 逐字(与 targa.cpp 各自独立保持两端口同构)。
/// Util.Stride (Util.cs L241-253): 4-byte aligned; the two
/// ArgumentException messages verbatim (kept structurally independent from
/// targa.cpp's copy).
std::int32_t PfimStride(std::int32_t int4_width, std::int32_t int4_pixel_depth) {
  if (int4_width <= 0)
    throw std::runtime_error("Width must be greater than zero");
  if (int4_pixel_depth <= 0)
    throw std::runtime_error("Pixel depth must be greater than zero");
  const auto int4_bytes_per_pixel = (int4_pixel_depth + 7) / 8;
  return 4 * ((int4_width * int4_bytes_per_pixel + 3) / 4);
}

std::uint32_t ReadLE32(const std::byte* ptr) {
  return std::to_integer<std::uint32_t>(ptr[0]) | (std::to_integer<std::uint32_t>(ptr[1]) << 8) |
         (std::to_integer<std::uint32_t>(ptr[2]) << 16) | (std::to_integer<std::uint32_t>(ptr[3]) << 24);
}

/// DdsHeader(DdsHeader.cs L154-258):魔数 542327876 + 124 字节头 + 32
/// 字节像素格式,小端;三条 "Not a valid …" 异常逐字(像素格式尺寸消息
/// 含上游 `${…}` 的字面 $ 怪癖)。Flags/Pitch/Caps* 解析后不可观测,逐字
/// 读跳。
/// DdsHeader (DdsHeader.cs L154-258): magic 542327876 + the 124-byte
/// header + the 32-byte pixel format, little-endian; the three "Not a
/// valid …" exceptions verbatim (the pixel-format-size message keeps
/// upstream's literal-$ `${…}` quirk). Flags/Pitch/Caps* are unreadable
/// after the parse and skipped byte for byte.
struct DdsHeader {
  std::uint32_t uint4_height = 0;
  std::uint32_t uint4_width = 0;
  std::uint32_t uint4_mip_map_count = 0;
  std::uint32_t uint4_flags_pixel_format = 0;
  CompressionAlgorithm type_four_cc = CompressionAlgorithm::None;
  std::uint32_t uint4_rgb_bit_count = 0;
  std::uint32_t uint4_r_bit_mask = 0;
  std::uint32_t uint4_g_bit_mask = 0;
  std::uint32_t uint4_b_bit_mask = 0;
  std::uint32_t uint4_a_bit_mask = 0;
};

DdsHeader ParseDdsHeader(SpanReader& reader_source) {
  if (reader_source.Length() - reader_source.Position() < 4 + 124)
    throw std::runtime_error("EndOfStream: dds header truncated");

  if (ReadLE32(reader_source.ReadBytes(4).data()) != 542327876u)
    throw std::runtime_error("Not a valid DDS");

  const auto span_buf = reader_source.ReadBytes(124);
  const auto at = [&span_buf](std::size_t st_i) { return ReadLE32(span_buf.data() + st_i); };

  auto header = DdsHeader{};
  if (at(0) != 124)
    throw std::runtime_error("Not a valid header size");
  // dword 序(魔数后):Size@0 Flags@4 Height@8 Width@12 Pitch@16 Depth@20
  // MipMapCount@24 Reserved1[11]@28..71 pfSize@72 pfFlags@76 FourCC@80
  // RGBBitCount@84 masks@88..103 Caps@104(不可观测跳过)。
  // The dword order (after the magic): Size@0 Flags@4 Height@8 Width@12
  // Pitch@16 Depth@20 MipMapCount@24 Reserved1[11]@28..71 pfSize@72
  // pfFlags@76 FourCC@80 RGBBitCount@84 masks@88..103 Caps@104 (skipped,
  // unobservable).
  header.uint4_height = at(8);
  header.uint4_width = at(12);
  header.uint4_mip_map_count = at(24);
  if (at(72) != 32)
    throw std::runtime_error("Expected pixel size to be 32, not: $" + std::to_string(at(72)));
  header.uint4_flags_pixel_format = at(76);
  header.type_four_cc = static_cast<CompressionAlgorithm>(at(80));
  header.uint4_rgb_bit_count = at(84);
  header.uint4_r_bit_mask = at(88);
  header.uint4_g_bit_mask = at(92);
  header.uint4_b_bit_mask = at(96);
  header.uint4_a_bit_mask = at(100);
  return header;
}

/// ColorFloatRgb(Colors.cs L23-56):RGB565 浮点展开 + Lerp + (byte)(x+
/// 0.5f) 截断 —— 逐字。| ColorFloatRgb (Colors.cs L23-56): the RGB565
/// float expansion + Lerp + the (byte)(x+0.5f) truncation — verbatim.
struct ColorFloatRgb {
  float float_r = 0;
  float float_g = 0;
  float float_b = 0;

  static ColorFloatRgb FromRgb565(std::uint16_t uint2_rgb565) {
    constexpr float kF5 = 255.0f / 31.0f;
    constexpr float kF6 = 255.0f / 63.0f;
    return ColorFloatRgb{
        static_cast<float>((uint2_rgb565 & 0x1f)) * kF5,
        static_cast<float>((uint2_rgb565 & 0x7E0) >> 5) * kF6,
        static_cast<float>((uint2_rgb565 & 0xF800) >> 11) * kF5,
    };
  }

  ColorFloatRgb Lerp(const ColorFloatRgb& other, float float_blend) const {
    return ColorFloatRgb{float_r + float_blend * (other.float_r - float_r),
                         float_g + float_blend * (other.float_g - float_g),
                         float_b + float_blend * (other.float_b - float_b)};
  }

  std::uint8_t R8() const { return static_cast<std::uint8_t>(float_r + 0.5f); }
  std::uint8_t G8() const { return static_cast<std::uint8_t>(float_g + 0.5f); }
  std::uint8_t B8() const { return static_cast<std::uint8_t>(float_b + 0.5f); }
};

/// Bc5Dds.ExtractGradient(Bc5Dds.cs L53-75):8 级 alpha 梯度(整除截断,
/// 两分支)。| Bc5Dds.ExtractGradient (Bc5Dds.cs L53-75): the 8-entry
/// alpha gradient (integer-division truncation, both branches).
std::size_t ExtractGradient(std::array<std::uint8_t, 8>& arr_gradient, std::span<const std::byte> vec_stream,
                            std::size_t st_b_index) {
  const auto uint1_endpoint0 = std::to_integer<std::uint8_t>(vec_stream[st_b_index++]);
  const auto uint1_endpoint1 = std::to_integer<std::uint8_t>(vec_stream[st_b_index++]);
  arr_gradient[0] = uint1_endpoint0;
  arr_gradient[1] = uint1_endpoint1;

  if (uint1_endpoint0 > uint1_endpoint1) {
    for (auto int4_i = 1; int4_i < 7; int4_i++)
      arr_gradient[static_cast<std::size_t>(1 + int4_i)] =
          static_cast<std::uint8_t>(((7 - int4_i) * uint1_endpoint0 + int4_i * uint1_endpoint1) / 7);
  } else {
    for (auto int4_i = 1; int4_i < 5; ++int4_i)
      arr_gradient[static_cast<std::size_t>(1 + int4_i)] =
          static_cast<std::uint8_t>(((5 - int4_i) * uint1_endpoint0 + int4_i * uint1_endpoint1) / 5);
    arr_gradient[6] = 0;
    arr_gradient[7] = 255;
  }
  return st_b_index;
}

enum class BlockKind { Dxt1, Dxt3, Dxt5 };

/// 单块解码(Dxt1/3/5Dds.cs 的 Decode 覆写):行内 4 像素 × 4 行,行尾
/// `dataIndex += 4×(stride−4)` 的落位逐字;返回消费后的 streamIndex。
/// The single-block decode (the Dxt1/3/5Dds.cs Decode overrides): 4
/// pixels × 4 rows, with the row-end `dataIndex += 4×(stride−4)` landing
/// verbatim; returns the consumed streamIndex.
std::size_t DecodeBlock(BlockKind type_kind, std::span<const std::byte> vec_stream, std::span<std::byte> vec_data,
                        std::size_t st_stream_index, std::size_t st_data_index, std::uint32_t uint4_stride) {
  const auto stride = static_cast<std::size_t>(uint4_stride);
  const auto read_u16 = [&vec_stream, &st_stream_index] {
    const auto uint1_lo = std::to_integer<std::uint16_t>(vec_stream[st_stream_index++]);
    return static_cast<std::uint16_t>(uint1_lo | (std::to_integer<std::uint16_t>(vec_stream[st_stream_index++]) << 8));
  };
  struct Color888 {
    std::uint8_t r, g, b;
  };

  if (type_kind == BlockKind::Dxt3) {
    // Dxt3Dds.Decode(Dxt3Dds.cs L36-85):alpha 双指针 + 颜色码;4 位
    // alpha |<<4 复制。| Dxt3Dds.Decode (Dxt3Dds.cs L36-85): the alpha
    // double pointer + the color codes; the 4-bit alpha |<<4 replication.
    const auto st_alpha_ptr = st_stream_index;
    st_stream_index += 8;

    const auto uint2_color0 = read_u16();
    const auto uint2_color1 = read_u16();
    const auto color_c0 = ColorFloatRgb::FromRgb565(uint2_color0);
    const auto color_c1 = ColorFloatRgb::FromRgb565(uint2_color1);
    const auto color_l13 = color_c0.Lerp(color_c1, 1.0f / 3.0f);
    const auto color_l23 = color_c0.Lerp(color_c1, 2.0f / 3.0f);
    const std::array<Color888, 4> arr_colors{Color888{color_c0.R8(), color_c0.G8(), color_c0.B8()},
                                             Color888{color_c1.R8(), color_c1.G8(), color_c1.B8()},
                                             Color888{color_l13.R8(), color_l13.G8(), color_l13.B8()},
                                             Color888{color_l23.R8(), color_l23.G8(), color_l23.B8()}};

    for (auto int4_i = 0; int4_i < 4; int4_i++) {
      const auto uint1_row_val = std::to_integer<std::uint8_t>(vec_stream[st_stream_index++]);
      auto uint2_row_alpha =
          std::to_integer<std::uint16_t>(vec_stream[st_alpha_ptr + static_cast<std::size_t>(int4_i) * 2]);
      uint2_row_alpha |= static_cast<std::uint16_t>(
          std::to_integer<std::uint16_t>(vec_stream[st_alpha_ptr + static_cast<std::size_t>(int4_i) * 2 + 1]) << 8);

      for (auto int4_j = 0; int4_j < 8; int4_j += 2) {
        auto uint1_current_alpha = static_cast<std::uint8_t>((uint2_row_alpha >> (int4_j * 2)) & 0x0f);
        uint1_current_alpha |= static_cast<std::uint8_t>(uint1_current_alpha << 4);
        const auto& color = arr_colors[(uint1_row_val >> int4_j) & 0x03];
        vec_data[st_data_index++] = static_cast<std::byte>(color.r);
        vec_data[st_data_index++] = static_cast<std::byte>(color.g);
        vec_data[st_data_index++] = static_cast<std::byte>(color.b);
        vec_data[st_data_index++] = static_cast<std::byte>(uint1_current_alpha);
      }
      st_data_index += 4 * (stride - 4);
    }
    return st_stream_index;
  }

  if (type_kind == BlockKind::Dxt5) {
    // Dxt5Dds.Decode(Dxt5Dds.cs L26-74):alpha 梯度 + 48 位 3 位码。
    // Dxt5Dds.Decode (Dxt5Dds.cs L26-74): the alpha gradient + the
    // 48-bit 3-bit codes.
    std::array<std::uint8_t, 8> arr_alpha{};
    st_stream_index = ExtractGradient(arr_alpha, vec_stream, st_stream_index);

    std::uint64_t uint8_alpha_codes = std::to_integer<std::uint64_t>(vec_stream[st_stream_index++]);
    for (auto int4_i = 0; int4_i < 5; int4_i++)
      uint8_alpha_codes |= std::to_integer<std::uint64_t>(vec_stream[st_stream_index++]) << (8 * (int4_i + 1));

    const auto uint2_color0 = read_u16();
    const auto uint2_color1 = read_u16();
    const auto color_c0 = ColorFloatRgb::FromRgb565(uint2_color0);
    const auto color_c1 = ColorFloatRgb::FromRgb565(uint2_color1);
    const auto color_l13 = color_c0.Lerp(color_c1, 1.0f / 3.0f);
    const auto color_l23 = color_c0.Lerp(color_c1, 2.0f / 3.0f);
    const std::array<Color888, 4> arr_colors{Color888{color_c0.R8(), color_c0.G8(), color_c0.B8()},
                                             Color888{color_c1.R8(), color_c1.G8(), color_c1.B8()},
                                             Color888{color_l13.R8(), color_l13.G8(), color_l13.B8()},
                                             Color888{color_l23.R8(), color_l23.G8(), color_l23.B8()}};

    for (auto int4_alpha_shift = 0; int4_alpha_shift < 48; int4_alpha_shift += 12) {
      const auto uint1_row_val = std::to_integer<std::uint8_t>(vec_stream[st_stream_index++]);
      for (auto int4_j = 0; int4_j < 4; int4_j++) {
        const auto uint1_alpha_index =
            static_cast<std::uint8_t>((uint8_alpha_codes >> (int4_alpha_shift + 3 * int4_j)) & 0x07);
        const auto& color = arr_colors[(uint1_row_val >> (int4_j * 2)) & 0x03];
        vec_data[st_data_index++] = static_cast<std::byte>(color.r);
        vec_data[st_data_index++] = static_cast<std::byte>(color.g);
        vec_data[st_data_index++] = static_cast<std::byte>(color.b);
        vec_data[st_data_index++] = static_cast<std::byte>(arr_alpha[uint1_alpha_index]);
      }
      st_data_index += 4 * (stride - 4);
    }
    return st_stream_index;
  }

  // Dxt1Dds.Decode(Dxt1Dds.cs L38-93):color0>color1 插值 / ≤ 的透穿两
  // 分支。| Dxt1Dds.Decode (Dxt1Dds.cs L38-93): the interpolated branch
  // of color0>color1 and the ≤ punch-through branch.
  const auto uint2_color0 = read_u16();
  const auto uint2_color1 = read_u16();
  const auto color_c0 = ColorFloatRgb::FromRgb565(uint2_color0);
  const auto color_c1 = ColorFloatRgb::FromRgb565(uint2_color1);

  struct Color8888 {
    std::uint8_t r, g, b, a;
  };
  std::array<Color8888, 4> arr_colors{};
  if (uint2_color0 > uint2_color1) {
    arr_colors[0] = {color_c0.R8(), color_c0.G8(), color_c0.B8(), 255};
    arr_colors[1] = {color_c1.R8(), color_c1.G8(), color_c1.B8(), 255};
    const auto color_l13 = color_c0.Lerp(color_c1, 1.0f / 3.0f);
    const auto color_l23 = color_c0.Lerp(color_c1, 2.0f / 3.0f);
    arr_colors[2] = {color_l13.R8(), color_l13.G8(), color_l13.B8(), 255};
    arr_colors[3] = {color_l23.R8(), color_l23.G8(), color_l23.B8(), 255};
  } else {
    arr_colors[0] = {color_c0.R8(), color_c0.G8(), color_c0.B8(), 255};
    arr_colors[1] = {color_c1.R8(), color_c1.G8(), color_c1.B8(), 255};
    const auto color_l50 = color_c0.Lerp(color_c1, 0.5f);
    arr_colors[2] = {color_l50.R8(), color_l50.G8(), color_l50.B8(), 255};
    arr_colors[3] = {0, 0, 0, 0};
  }

  for (auto int4_i = 0; int4_i < 4; int4_i++) {
    const auto uint1_row_val = std::to_integer<std::uint8_t>(vec_stream[st_stream_index++]);
    for (auto int4_j = 0; int4_j < 8; int4_j += 2) {
      const auto& color = arr_colors[(uint1_row_val >> int4_j) & 0x03];
      vec_data[st_data_index++] = static_cast<std::byte>(color.r);
      vec_data[st_data_index++] = static_cast<std::byte>(color.g);
      vec_data[st_data_index++] = static_cast<std::byte>(color.b);
      vec_data[st_data_index++] = static_cast<std::byte>(color.a);
    }
    st_data_index += 4 * (stride - 4);
  }
  return st_stream_index;
}

/// MipMapOffset(MipMapOffset.cs)。| MipMapOffset (MipMapOffset.cs).
struct MipMapOffset {
  std::int32_t int4_width = 0;
  std::int32_t int4_height = 0;
  std::int32_t int4_stride = 0;
  std::size_t st_data_offset = 0;
  std::size_t st_data_len = 0;
};

struct DdsImage {
  std::vector<std::byte> vec_data;
  std::int32_t int4_width = 0;
  std::int32_t int4_height = 0;
  PfimFormat type_format = PfimFormat::Rgba32;
};

/// (int)(Header.Width / Math.Pow(2, i)) 的 double 截断形态。
/// The double-truncating (int)(Header.Width / Math.Pow(2, i)) form.
std::int32_t MipDim(std::uint32_t uint4_dim, std::int32_t int4_shift) {
  return static_cast<std::int32_t>(static_cast<double>(uint4_dim) / std::pow(2.0, static_cast<double>(int4_shift)));
}

std::int32_t CalcBlocks(std::int32_t int4_pixels) { return std::max(1, (int4_pixels + 3) / 4); }

/// UncompressedDds.DataDecode(UncompressedDds.cs L129-207):ImageInfo 位
/// 深/掩码判定、mip 链分配、Fill/InnerFillUnaligned 两形态填充、Swap 通
/// 道交换(Rgb24 逐 mip 行交换 / Rgba32 全缓冲 4 步交换 / Rgba16 半字节
/// 交换)。
/// UncompressedDds.DataDecode (UncompressedDds.cs L129-207): the
/// ImageInfo bit-depth/mask probes, the mip-chain allocation, the
/// Fill/InnerFillUnaligned two-shape filling, and the Swap channel
/// exchange (Rgb24 per-mip row swaps / Rgba32 whole-buffer 4-stride swaps
/// / the Rgba16 nibble swap).
DdsImage DecodeUncompressed(const DdsHeader& header, std::span<const std::byte> vec_rest) {
  const auto int4_width = static_cast<std::int32_t>(header.uint4_width);
  const auto int4_height = static_cast<std::int32_t>(header.uint4_height);
  const auto b_rgb_swapped = header.uint4_r_bit_mask < header.uint4_g_bit_mask;

  PfimFormat type_format;
  switch (header.uint4_rgb_bit_count) {
    case 8:
      type_format = PfimFormat::Rgb8;
      break;
    case 16: {
      // SixteenBitImageFormat(UncompressedDds.cs L99-119)。
      // SixteenBitImageFormat (UncompressedDds.cs L99-119).
      if (header.uint4_a_bit_mask == 0xF000 && header.uint4_r_bit_mask == 0xF00 && header.uint4_g_bit_mask == 0xF0 &&
          header.uint4_b_bit_mask == 0xF)
        type_format = PfimFormat::Rgba16;
      else if ((header.uint4_flags_pixel_format & kAlphaPixels) != 0)
        type_format = PfimFormat::R5g5b5a1;
      else if (header.uint4_g_bit_mask == 0x7e0)
        type_format = PfimFormat::R5g6b5;
      else
        type_format = PfimFormat::R5g5b5;
      break;
    }
    case 24: type_format = PfimFormat::Rgb24; break;
    case 32: type_format = PfimFormat::Rgba32; break;
    default:
      throw std::runtime_error("Unrecognized rgb bit count: " + std::to_string(header.uint4_rgb_bit_count));
  }

  const auto int4_bits_per_pixel = static_cast<std::int32_t>(header.uint4_rgb_bit_count);
  const auto int4_bytes_per_pixel = int4_bits_per_pixel / 8;
  const auto int4_stride = PfimStride(int4_width, int4_bits_per_pixel);
  const auto st_data_len =
      static_cast<std::size_t>(int4_stride) * static_cast<std::size_t>(std::max<std::int32_t>(1, int4_height));

  std::size_t st_total_len = st_data_len;
  auto vec_mips = std::vector<MipMapOffset>{};
  if (header.uint4_mip_map_count > 1) {
    vec_mips.resize(header.uint4_mip_map_count - 1);
    for (std::size_t st_i = 0; st_i < vec_mips.size(); st_i++) {
      const auto int4_mip_width =
          std::max<std::int32_t>(1, MipDim(header.uint4_width, static_cast<std::int32_t>(st_i) + 1));
      const auto int4_mip_height =
          std::max<std::int32_t>(1, MipDim(header.uint4_height, static_cast<std::int32_t>(st_i) + 1));
      const auto int4_mip_stride = PfimStride(int4_mip_width, int4_bits_per_pixel);
      const auto st_mip_len = static_cast<std::size_t>(int4_mip_stride) * static_cast<std::size_t>(int4_mip_height);
      vec_mips[st_i] = MipMapOffset{int4_mip_width, int4_mip_height, int4_mip_stride, st_total_len, st_mip_len};
      st_total_len += st_mip_len;
    }
  }

  auto vec_data = std::vector<std::byte>(st_total_len, std::byte{0});

  // 逐层填充:行紧排对齐 → Fill 直拷;否则 InnerFillUnaligned 按行宽拷
  // 入 stride 行。| Per-level fill: tight rows → the Fill straight copy;
  // else InnerFillUnaligned copies row widths into strided rows.
  std::size_t st_src = 0;
  const auto fill_level = [&](std::size_t st_dest, std::size_t st_len, std::int32_t int4_level_width,
                              std::int32_t int4_level_stride) {
    const auto int4_row_bytes = int4_level_width * int4_bytes_per_pixel;
    if (static_cast<std::size_t>(int4_row_bytes) == static_cast<std::size_t>(int4_level_stride)) {
      if (st_src + st_len > vec_rest.size())
        throw std::runtime_error("EndOfStream: dds pixel data truncated");
      std::ranges::copy(vec_rest.subspan(st_src, st_len), vec_data.begin() + static_cast<std::ptrdiff_t>(st_dest));
      st_src += st_len;
    } else {
      for (std::size_t st_off = st_dest; st_off < st_dest + st_len;
           st_off += static_cast<std::size_t>(int4_level_stride)) {
        if (st_src + static_cast<std::size_t>(int4_row_bytes) > vec_rest.size())
          throw std::runtime_error("EndOfStream: dds pixel data truncated");
        std::ranges::copy(vec_rest.subspan(st_src, static_cast<std::size_t>(int4_row_bytes)),
                          vec_data.begin() + static_cast<std::ptrdiff_t>(st_off));
        st_src += static_cast<std::size_t>(int4_row_bytes);
      }
    }
  };
  fill_level(0, st_data_len, int4_width, int4_stride);
  for (const auto& mip : vec_mips)
    fill_level(mip.st_data_offset, mip.st_data_len, mip.int4_width, mip.int4_stride);

  if (b_rgb_swapped) {
    const auto swap_level_rgb24 = [&](const MipMapOffset& mip) {
      for (auto int4_y = 0; int4_y < mip.int4_height; int4_y++) {
        const auto st_row_offset =
            mip.st_data_offset + static_cast<std::size_t>(int4_y) * static_cast<std::size_t>(mip.int4_stride);
        for (auto int4_x = 0; int4_x < mip.int4_width; int4_x++) {
          const auto st_i = st_row_offset + static_cast<std::size_t>(int4_x) * 3;
          const auto byte_temp = vec_data[st_i];
          vec_data[st_i] = vec_data[st_i + 2];
          vec_data[st_i + 2] = byte_temp;
        }
      }
    };

    switch (type_format) {
      case PfimFormat::Rgb24:
        swap_level_rgb24(MipMapOffset{int4_width, int4_height, int4_stride, 0, 0});
        for (const auto& mip : vec_mips)
          swap_level_rgb24(mip);
        break;
      case PfimFormat::Rgba32:
        for (std::size_t st_i = 0; st_i + 3 < st_total_len; st_i += 4) {
          const auto byte_temp = vec_data[st_i];
          vec_data[st_i] = vec_data[st_i + 2];
          vec_data[st_i + 2] = byte_temp;
        }
        break;
      case PfimFormat::Rgba16:
        for (std::size_t st_i = 0; st_i + 1 < st_total_len; st_i += 2) {
          const auto uint1_temp = static_cast<std::uint8_t>(std::to_integer<int>(vec_data[st_i]) & 0xF);
          vec_data[st_i] = static_cast<std::byte>((std::to_integer<int>(vec_data[st_i]) & 0xF0) +
                                                  (std::to_integer<int>(vec_data[st_i + 1]) & 0xF));
          vec_data[st_i + 1] =
              static_cast<std::byte>((std::to_integer<int>(vec_data[st_i + 1]) & 0xF0) + uint1_temp);
        }
        break;
      default:
        throw std::runtime_error(std::string("Do not know how to swap ") + NameOf(type_format));
    }
  }

  return DdsImage{std::move(vec_data), int4_width, int4_height, type_format};
}

/// CompressedDds.InMemoryDecode(CompressedDds.cs L131-193)+
/// AllocateMipMaps(L99-128)的统一控制流(D65);块循环逐字。数据布局 =
/// 块对齐 stride(widthBlocks×4 像素)× heightBlocks×4 行,非 4 整除尺寸
/// 的衬垫随之入 Data(上游同形)。
/// CompressedDds.InMemoryDecode (CompressedDds.cs L131-193) +
/// AllocateMipMaps (L99-128) as the unified control flow (D65); the block
/// loops verbatim. The data layout is block-aligned strides (widthBlocks×4
/// pixels) × heightBlocks×4 rows, so non-multiple-of-4 dimensions carry
/// their padding into Data (the same shape as upstream).
DdsImage DecodeCompressed(const DdsHeader& header, std::span<const std::byte> vec_rest, BlockKind type_kind) {
  const auto int4_width = static_cast<std::int32_t>(header.uint4_width);
  const auto int4_height = static_cast<std::int32_t>(header.uint4_height);
  const auto int4_width_blocks = CalcBlocks(int4_width);
  const auto int4_stride_pixels = int4_width_blocks * 4;
  const auto int4_stride = int4_stride_pixels * 4;

  const auto st_len0 =
      static_cast<std::size_t>(CalcBlocks(int4_height)) * 4 * static_cast<std::size_t>(int4_stride);

  std::size_t st_total_len = st_len0;
  auto vec_mips = std::vector<MipMapOffset>{};
  if (header.uint4_mip_map_count > 1) {
    vec_mips.resize(header.uint4_mip_map_count - 1);
    for (std::size_t st_i = 1; st_i < header.uint4_mip_map_count; st_i++) {
      const auto int4_mip_width = std::max(MipDim(header.uint4_width, static_cast<std::int32_t>(st_i)), 1);
      const auto int4_mip_height = std::max(MipDim(header.uint4_height, static_cast<std::int32_t>(st_i)), 1);
      const auto int4_mip_stride = CalcBlocks(int4_mip_width) * 4 * 4;
      const auto st_mip_len = static_cast<std::size_t>(CalcBlocks(int4_mip_height)) * 4 *
                              static_cast<std::size_t>(int4_mip_stride);
      vec_mips[st_i - 1] = MipMapOffset{int4_mip_width, int4_mip_height, int4_mip_stride, st_total_len, st_mip_len};
      st_total_len += st_mip_len;
    }
  }

  auto vec_data = std::vector<std::byte>(st_total_len, std::byte{0});
  std::size_t st_b_index = 0;

  auto int8_pixels_left = static_cast<std::int64_t>(st_total_len);
  std::size_t st_data_index = 0;
  for (std::uint32_t uint4_image_index = 0;
       uint4_image_index < header.uint4_mip_map_count + 1 && int8_pixels_left > 0; uint4_image_index++) {
    auto int4_level_stride = int4_stride;
    auto int4_level_blocks = int4_width_blocks;
    auto int8_index_pixels_left = static_cast<std::int64_t>(CalcBlocks(int4_height)) * 4 *
                                  static_cast<std::int64_t>(int4_stride);

    if (uint4_image_index != 0) {
      const auto int4_mip_width =
          std::max(MipDim(header.uint4_width, static_cast<std::int32_t>(uint4_image_index)), 1);
      const auto int4_mip_height =
          std::max(MipDim(header.uint4_height, static_cast<std::int32_t>(uint4_image_index)), 1);
      int4_level_stride = CalcBlocks(int4_mip_width) * 4 * 4;
      int4_level_blocks = CalcBlocks(int4_mip_width);
      int8_index_pixels_left =
          static_cast<std::int64_t>(CalcBlocks(int4_mip_height)) * 4 * static_cast<std::int64_t>(int4_level_stride);
    }

    const auto int4_level_stride_pixels = int4_level_stride / 4;

    while (int8_index_pixels_left > 0) {
      const auto st_orig_data_index = st_data_index;

      for (std::int32_t int4_i = 0; int4_i < int4_level_blocks; int4_i++) {
        st_b_index = DecodeBlock(type_kind, vec_rest, vec_data, st_b_index, st_data_index,
                                 static_cast<std::uint32_t>(int4_level_stride_pixels));
        st_data_index += 4 * 4;
      }

      const auto int8_filled = static_cast<std::int64_t>(int4_level_stride) * 4;
      int8_pixels_left -= int8_filled;
      int8_index_pixels_left -= int8_filled;

      // 下跳 divSize-1 行的块首。| Jump down to the block-row start
      // divSize-1 rows below.
      st_data_index = st_orig_data_index + static_cast<std::size_t>(int8_filled);
    }
  }

  return DdsImage{std::move(vec_data), int4_width, int4_height, PfimFormat::Rgba32};
}

}  // namespace

bool IsDds(std::span<const std::byte> vec_file) {
  return vec_file.size() >= 4 && ReadLE32(vec_file.data()) == 0x20534444u;
}

DdsFrame::DdsFrame(std::span<const std::byte> vec_file) {
  auto reader_source = SpanReader{vec_file};
  const auto header = ParseDdsHeader(reader_source);
  const auto vec_rest = vec_file.subspan(static_cast<std::size_t>(reader_source.Position()));

  DdsImage image;
  switch (header.type_four_cc) {
    case CompressionAlgorithm::D3DFMT_DXT1: image = DecodeCompressed(header, vec_rest, BlockKind::Dxt1); break;
    case CompressionAlgorithm::D3DFMT_DXT3: image = DecodeCompressed(header, vec_rest, BlockKind::Dxt3); break;
    case CompressionAlgorithm::D3DFMT_DXT5: image = DecodeCompressed(header, vec_rest, BlockKind::Dxt5); break;
    case CompressionAlgorithm::D3DFMT_DXT2:
    case CompressionAlgorithm::D3DFMT_DXT4:
      throw std::runtime_error("Cannot support DXT2 or DXT4");
    case CompressionAlgorithm::None: image = DecodeUncompressed(header, vec_rest); break;
    default:
      // D66:BC4/BC5/ATI/DX10 系未移植 —— 上游可解(BC5 系解为 Rgb24),
      // 此处显式拒绝(COVERAGE 登记;mods 零 dds 资产,oracle 向量覆盖已
      // 移植路径)。
      // D66: the BC4/BC5/ATI/DX10 family is not ported — upstream decodes
      // it (the BC5 family to Rgb24); an explicit rejection here
      // (registered in COVERAGE; the mods carry zero dds assets, the
      // oracle vectors cover the ported paths).
      throw std::runtime_error("FourCC: " + std::to_string(static_cast<std::uint32_t>(header.type_four_cc)) +
                               " not supported.");
  }

  int2_size_ = int2{image.int4_width, image.int4_height};
  vec_data_ = std::move(image.vec_data);

  // SpriteFrameType 指通道字节序 —— 与小端位序反排(DdsLoader.cs L66-68)。
  // SpriteFrameType names the channel byte order — reversed from the
  // little-endian bit order (DdsLoader.cs L66-68).
  switch (image.type_format) {
    case PfimFormat::Rgba32: type_frame_ = gfx::SpriteFrameType::Bgra32; break;
    case PfimFormat::Rgb24: type_frame_ = gfx::SpriteFrameType::Bgr24; break;
    default:
      throw std::runtime_error(std::string("Unhandled ImageFormat ") + NameOf(image.type_format));
  }
}

DdsSprite::DdsSprite(std::span<const std::byte> vec_file) {
  vec_frames_.push_back(std::make_unique<DdsFrame>(vec_file));
}

bool TryParseDds(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsDds(vec_file)) {
    vec_frames.clear();
    return false;
  }

  vec_frames.clear();
  vec_frames.push_back(std::make_unique<DdsFrame>(vec_file));
  return true;
}

}  // namespace ora::fmt
