// UPSTREAM: NONE —— Pfim v0.11.3 逐语义移植(targa/ 四件 + Targa.cs 骨架);
// 上游 OpenRA 消费面 TgaLoader.cs @7d57605。适配汇总见 targa.hpp 头注。
// 忠实保留的上游怪癖:TopLeft-RLE 行紧排(不按 stride 跳衬垫)、
// 15bpp 的 PixelDepthBytes=1 截断、色图应用 newLen = depthBytes×DataLen
// (非宽高积)。缓冲流/快路径统一(D65)。
// UPSTREAM: NONE — the verbatim-semantics port of Pfim v0.11.3 (the four
// targa/ files + the Targa.cs skeleton); the OpenRA consumer face is
// TgaLoader.cs @7d57605. See targa.hpp's header note. Upstream quirks kept
// faithfully: the TopLeft-RLE tight row packing (no stride skips), the
// PixelDepthBytes=1 truncation at 15bpp, and the color-map application's
// newLen = depthBytes×DataLen (not width×height). Buffered/fast paths
// unified (D65).
#include "formats/targa.hpp"

namespace ora::fmt {
namespace {

/// TargaImageType(TargaHeader.cs L18-58)。| TargaImageType (TargaHeader.cs
/// L18-58).
enum class TargaImageType : std::uint8_t {
  NoData = 0,
  UncompressedColorMap = 1,
  UncompressedTrueColor = 2,
  UncompressedBW = 3,
  RunLengthColorMap = 9,
  RunLengthTrueColor = 10,
  RunLengthBW = 11,
};

/// Pfim.ImageFormat 名(仅异常文本与帧映射用;ImageFormat.cs)。
/// The Pfim.ImageFormat names (for exception texts and the frame mapping
/// only; ImageFormat.cs).
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

/// TargaHeader.cs L109-176 的解析产物(仅 OpenRA 可观测量)。
/// The parse product of TargaHeader.cs L109-176 (only the OpenRA-visible
/// quantities).
struct TargaHeader {
  std::uint8_t uint1_id_length = 0;
  bool b_has_color_map = false;
  TargaImageType type_image = TargaImageType::NoData;
  std::int16_t int2_color_map_origin = 0;
  std::int16_t int2_color_map_length = 0;
  std::int16_t int2_color_map_depth_bits = 0;
  std::int16_t int2_x_origin = 0;
  std::int16_t int2_y_origin = 0;
  std::int16_t int2_width = 0;
  std::int16_t int2_height = 0;
  std::uint8_t uint1_pixel_depth_bits = 0;
  std::uint8_t uint1_orientation = 0;  // 描述字节 >>4 &3 | descriptor >>4 &3.

  std::vector<std::byte> vec_color_map;

  std::uint8_t ColorMapDepthBytes() const {
    return static_cast<std::uint8_t>(int2_color_map_depth_bits / 8);
  }
  std::uint8_t PixelDepthBytes() const { return static_cast<std::uint8_t>(uint1_pixel_depth_bits / 8); }
  bool IsCompressed() const {
    return type_image == TargaImageType::RunLengthTrueColor || type_image == TargaImageType::RunLengthColorMap ||
           type_image == TargaImageType::RunLengthBW;
  }
};

/// Util.Stride(Util.cs L241-253):4 字节对齐 + 两个 ArgumentException 消息。
/// Util.Stride (Util.cs L241-253): 4-byte alignment + the two
/// ArgumentException messages.
std::int32_t PfimStride(std::int32_t int4_width, std::int32_t int4_pixel_depth) {
  if (int4_width <= 0)
    throw std::runtime_error("Width must be greater than zero");
  if (int4_pixel_depth <= 0)
    throw std::runtime_error("Pixel depth must be greater than zero");

  const auto int4_bytes_per_pixel = (int4_pixel_depth + 7) / 8;

  // Windows GDI+ requires that the stride be a multiple of four.
  return 4 * ((int4_width * int4_bytes_per_pixel + 3) / 4);
}

/// TargaHeader(Stream, config)(TargaHeader.cs L104-107 + L109-176):
/// 18 字节头 + Image ID + 色图读取。
/// TargaHeader(Stream, config) (TargaHeader.cs L104-107 + L109-176): the
/// 18-byte header + the image ID + the color-map read.
TargaHeader ParseTargaHeader(SpanReader& reader_source) {
  auto header = TargaHeader{};

  const auto span_buf = reader_source.ReadBytes(18);
  const auto b_at = [&span_buf](std::size_t st_i) { return std::to_integer<std::uint8_t>(span_buf[st_i]); };

  header.uint1_id_length = b_at(0);
  header.b_has_color_map = b_at(1) == 1;
  header.type_image = static_cast<TargaImageType>(b_at(2));
  switch (b_at(2)) {
    case 0: case 1: case 2: case 3: case 9: case 10: case 11: break;  // Enum.IsDefined
    default: throw std::runtime_error("Detected invalid targa image");
  }

  header.int2_color_map_origin = static_cast<std::int16_t>(b_at(3) | (b_at(4) << 8));
  header.int2_color_map_length = static_cast<std::int16_t>(b_at(5) | (b_at(6) << 8));
  header.int2_color_map_depth_bits = b_at(7);
  header.int2_x_origin = static_cast<std::int16_t>(b_at(8) | (b_at(9) << 8));
  header.int2_y_origin = static_cast<std::int16_t>(b_at(10) | (b_at(11) << 8));
  header.int2_width = static_cast<std::int16_t>(b_at(12) | (b_at(13) << 8));
  header.int2_height = static_cast<std::int16_t>(b_at(14) | (b_at(15) << 8));
  header.uint1_pixel_depth_bits = b_at(16);
  header.uint1_orientation = (b_at(17) >> 4) & 3;

  if (header.uint1_id_length != 0)
    reader_source.Skip(header.uint1_id_length);  // ImageId(Unicode 文本,消费面不可观)| the ImageId (Unicode text; unobservable downstream)

  if (header.b_has_color_map) {
    if (header.int2_color_map_origin > header.int2_color_map_length)
      throw std::runtime_error("Color map origin was detected to exceed color map length");

    const auto int4_map_bytes = header.ColorMapDepthBytes();
    header.vec_color_map.assign(static_cast<std::size_t>(header.int2_color_map_length) * int4_map_bytes, std::byte{0});
    const auto span_map = reader_source.ReadBytes(
        static_cast<std::int64_t>(header.int2_color_map_length - header.int2_color_map_origin) * int4_map_bytes);
    std::ranges::copy(span_map, header.vec_color_map.begin() +
                                    static_cast<std::ptrdiff_t>(header.int2_color_map_origin) * int4_map_bytes);
  }

  return header;
}

/// CompressedTarga.RunLength / RunLength3(CompressedTarga.cs L236-305):
/// 行内 RLE 包展开(1/2/3/4 字节像素的模式重复;3 = RGB 三字节循环)。
/// CompressedTarga.RunLength / RunLength3 (CompressedTarga.cs L236-305):
/// the in-row RLE packet expansion (pattern repeats for 1/2/3/4-byte
/// pixels; 3 = the RGB three-byte cycle).
void ExpandRun(std::span<std::byte> vec_dest, std::size_t st_dest, std::span<const std::byte> vec_src,
               std::size_t st_src, std::int32_t int4_color_depth_bytes, std::int32_t int4_run) {
  switch (int4_color_depth_bytes) {
    case 3: {
      const auto uint1_c0 = std::to_integer<std::uint8_t>(vec_src[st_src]);
      const auto uint1_c1 = std::to_integer<std::uint8_t>(vec_src[st_src + 1]);
      const auto uint1_c2 = std::to_integer<std::uint8_t>(vec_src[st_src + 2]);
      for (auto int4_i = 0; int4_i < int4_run; int4_i++) {
        vec_dest[st_dest++] = static_cast<std::byte>(uint1_c0);
        vec_dest[st_dest++] = static_cast<std::byte>(uint1_c1);
        vec_dest[st_dest++] = static_cast<std::byte>(uint1_c2);
      }
      break;
    }
    case 4: {
      const auto span_pattern = vec_src.subspan(st_src, 4);
      for (auto int4_i = 0; int4_i < int4_run; int4_i++)
        for (auto int4_j = 0; int4_j < 4; int4_j++)
          vec_dest[st_dest + static_cast<std::size_t>(int4_i) * 4 + static_cast<std::size_t>(int4_j)] =
              span_pattern[static_cast<std::size_t>(int4_j)];
      break;
    }
    case 1: {
      const auto byte_fill = vec_src[st_src];
      for (auto int4_i = 0; int4_i < int4_run; int4_i++)
        vec_dest[st_dest + static_cast<std::size_t>(int4_i)] = byte_fill;
      break;
    }
    case 2: {
      const auto span_pattern = vec_src.subspan(st_src, 2);
      for (auto int4_i = 0; int4_i < int4_run; int4_i++)
        for (auto int4_j = 0; int4_j < 2; int4_j++)
          vec_dest[st_dest + static_cast<std::size_t>(int4_i) * 2 + static_cast<std::size_t>(int4_j)] =
              span_pattern[static_cast<std::size_t>(int4_j)];
      break;
    }
    default:
      break;
  }
}

/// 解码产物:Targa(Targa.cs)的可观测量。| The decode product: the
/// observable quantities of Targa (Targa.cs).
struct TargaImage {
  std::vector<std::byte> vec_data;  // Data 属性全数组 | the full Data array
  std::int32_t int4_width = 0;
  std::int32_t int4_height = 0;
  std::uint8_t uint1_pixel_depth_bits = 0;

  PfimFormat Format() const {
    switch (uint1_pixel_depth_bits) {
      case 8: return PfimFormat::Rgb8;
      case 16: return PfimFormat::R5g5b5;
      case 24: return PfimFormat::Rgb24;
      case 32: return PfimFormat::Rgba32;
      default: throw std::runtime_error("Unrecognized pixel depth: " + std::to_string(uint1_pixel_depth_bits));
    }
  }
};

/// DecodeTarga(Targa.cs L61-100)后置的 ApplyColorMap(Targa.cs L102-152):
/// 仅色图类型图像;newLen = depthBytes × DataLen 的上游怪癖保留。
/// The ApplyColorMap run after DecodeTarga (Targa.cs L102-152): color-map
/// image types only; the upstream newLen = depthBytes × DataLen quirk kept.
void ApplyColorMap(TargaImage& image, TargaHeader& header) {
  if (!header.b_has_color_map ||
      (header.type_image != TargaImageType::RunLengthColorMap && header.type_image != TargaImageType::UncompressedColorMap))
    return;

  const auto int4_map_depth_bytes = header.ColorMapDepthBytes();
  const auto int4_old_stride = PfimStride(image.int4_width, header.uint1_pixel_depth_bits);
  const auto int4_new_stride = PfimStride(image.int4_width, int4_map_depth_bytes * 8);
  const auto uint8_new_len = static_cast<std::size_t>(int4_map_depth_bytes) * image.vec_data.size();
  auto vec_new = std::vector<std::byte>(uint8_new_len, std::byte{0});

  switch (header.int2_color_map_depth_bits) {
    case 16:
    case 24:
    case 32:
      for (auto int4_i = 0; int4_i < image.int4_height; int4_i++) {
        const auto st_data_offset = static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_old_stride);
        const auto st_new_offset = static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_new_stride);
        for (auto int4_j = 0; int4_j < image.int4_width; int4_j++) {
          const auto st_map_index = std::to_integer<std::size_t>(
                                        image.vec_data[st_data_offset + static_cast<std::size_t>(int4_j)]) *
                                    static_cast<std::size_t>(int4_map_depth_bytes);
          for (auto int4_k = 0; int4_k < int4_map_depth_bytes; int4_k++)
            vec_new[st_new_offset + static_cast<std::size_t>(int4_j) * static_cast<std::size_t>(int4_map_depth_bytes) +
                    static_cast<std::size_t>(int4_k)] =
                header.vec_color_map[st_map_index + static_cast<std::size_t>(int4_k)];
        }
      }
      break;
    default:
      throw std::runtime_error("Unrecognized color map depth " + std::to_string(header.int2_color_map_depth_bits));
  }

  image.vec_data = std::move(vec_new);
  header.uint1_pixel_depth_bits = static_cast<std::uint8_t>(header.int2_color_map_depth_bits);
  header.vec_color_map.clear();
  header.int2_color_map_length = 0;
  header.b_has_color_map = false;
  header.int2_color_map_depth_bits = 0;
}

/// DecodeTarga(Targa.cs L61-100):定向分派 + 压缩/非压缩解码。
/// DecodeTarga (Targa.cs L61-100): the orientation dispatch + the
/// compressed/uncompressed decode.
TargaImage DecodeTarga(std::span<const std::byte> vec_file) {
  auto reader_source = SpanReader{vec_file};
  auto header = ParseTargaHeader(reader_source);

  const auto int4_width = header.int2_width;
  const auto int4_height = header.int2_height;
  const auto int4_stride = PfimStride(int4_width, header.uint1_pixel_depth_bits);
  const auto int4_len = int4_height * int4_stride;
  if (int4_len <= 0)
    throw std::runtime_error("OverflowException");  // 上游 Rent[negative] 的等价抛点 | the equivalent of upstream's Rent[negative]

  auto vec_data = std::vector<std::byte>(static_cast<std::size_t>(int4_len), std::byte{0});
  const auto int4_bytes_per_pixel = header.PixelDepthBytes();
  const auto int4_row_bytes = int4_width * int4_bytes_per_pixel;
  const auto span_src = vec_file.subspan(static_cast<std::size_t>(reader_source.Position()));
  std::size_t st_src = 0;

  const auto read_src = [&](std::size_t st_count) {
    if (st_src + st_count > span_src.size())
      throw std::runtime_error("EndOfStream: targa pixel data truncated");  // 上游快路径越界/短读的等价抛点 | the equivalent of upstream's fast-path overread
    const auto span_out = span_src.subspan(st_src, st_count);
    st_src += st_count;
    return span_out;
  };

  // TargaOrientation(TargaHeader.cs L73-107):BottomLeft/BottomRight/
  // TopRight 同走自下而上(BottomLeft);TopLeft 自上而下。RLE 的 TopLeft
  // 行紧排怪癖见头注。
  // TargaOrientation (TargaHeader.cs L73-107): BottomLeft/BottomRight/
  // TopRight all take the bottom-up path (BottomLeft); TopLeft fills
  // top-down. The TopLeft-RLE tight-packing quirk is in the header note.
  const auto b_bottom_up = header.uint1_orientation != 2;

  if (!header.IsCompressed()) {
    // UncompressedTarga(UncompressedTarga.cs):行序拷贝,行宽 = W×bpp。
    // UncompressedTarga (UncompressedTarga.cs): sequential row copies at
    // row width W×bpp.
    for (auto int4_row = 0; int4_row < int4_height; int4_row++) {
      const auto int4_dest_row = b_bottom_up ? int4_height - 1 - int4_row : int4_row;
      const auto span_row = read_src(static_cast<std::size_t>(int4_row_bytes));
      std::ranges::copy(span_row, vec_data.begin() + static_cast<std::ptrdiff_t>(int4_dest_row) * int4_stride);
    }
  } else {
    // CompressedTarga(CompressedTarga.cs):BottomLeft 系按 stride 落行;
    // TopLeft 系上游的紧排循环(dataIndex 不按 stride 复位)照抄。
    // CompressedTarga (CompressedTarga.cs): the BottomLeft family lands
    // rows on stride; the TopLeft family reproduces upstream's tight loop
    // (dataIndex never realigns to stride).
    auto int4_data_index = b_bottom_up ? int4_len - int4_stride : 0;
    for (auto int4_row = 0; int4_row < int4_height; int4_row++) {
      auto int4_col = 0;
      do {
        const auto uint1_packet = std::to_integer<std::uint8_t>(read_src(1).front());
        if ((uint1_packet & 128) != 0) {
          const auto int4_run = uint1_packet - 127;
          ExpandRun(vec_data, static_cast<std::size_t>(int4_data_index), span_src, st_src, int4_bytes_per_pixel,
                    int4_run);
          st_src += static_cast<std::size_t>(int4_bytes_per_pixel);
          int4_data_index += int4_run * int4_bytes_per_pixel;
          int4_col += int4_run;
        } else {
          const auto int4_pixels = uint1_packet + 1;
          const auto int4_bytes = int4_pixels * int4_bytes_per_pixel;
          const auto span_packet = read_src(static_cast<std::size_t>(int4_bytes));
          std::ranges::copy(span_packet,
                            vec_data.begin() + static_cast<std::ptrdiff_t>(int4_data_index));
          int4_data_index += int4_bytes;
          int4_col += int4_pixels;
        }
      } while (int4_col < int4_width);

      if (b_bottom_up)
        int4_data_index -= int4_bytes_per_pixel * int4_width + int4_stride;
    }
  }

  auto image = TargaImage{std::move(vec_data), int4_width, int4_height, header.uint1_pixel_depth_bits};
  ApplyColorMap(image, header);
  return image;
}

}  // namespace

bool IsTga(std::span<const std::byte> vec_file) {
  if (vec_file.size() < 9)
    return false;  // 上游短流 Read 抛 EndOfStream → TryParseSprite 未捕获;此处按探 false 处理的等价域(夹具均 ≥18B)| upstream's short-stream Read throws past the probe; fixtures are always ≥18B

  const auto b_at = [&vec_file](std::size_t st_i) { return std::to_integer<std::uint8_t>(vec_file[st_i]); };

  // Require true-color images(L29)。
  if (b_at(1) != 0)  // colorMapType
    return false;
  if (b_at(2) != 2)  // imageType
    return false;

  const std::uint32_t uint4_color_map_offset_and_size =
      b_at(3) | (b_at(4) << 8) | (b_at(5) << 16) | (static_cast<std::uint32_t>(b_at(6)) << 24);
  if (uint4_color_map_offset_and_size != 0)
    return false;

  if (b_at(7) != 0)  // colorMapBits
    return false;

  return true;
}

TgaFrame::TgaFrame(std::span<const std::byte> vec_file) {
  const auto image = DecodeTarga(vec_file);

  int2_size_ = int2{image.int4_width, image.int4_height};
  int2_frame_size_ = int2_size_;
  vec_data_ = std::move(image.vec_data);

  // 帧通道序按位反排的注释语义(TgaLoader.cs L93):Pfim Rgba32 字节序
  // 即 BGRABgra32。| The reversed-channel-order comment semantics
  // (TgaLoader.cs L93): Pfim's Rgba32 byte order is BGRA → Bgra32.
  switch (image.Format()) {
    case PfimFormat::Rgba32: type_frame_ = gfx::SpriteFrameType::Bgra32; break;
    case PfimFormat::Rgb24: type_frame_ = gfx::SpriteFrameType::Bgr24; break;
    default:
      throw std::runtime_error(std::string("Unhandled ImageFormat ") + NameOf(image.Format()));
  }
}

TgaFrame::TgaFrame(std::span<const std::byte> vec_file, int2 int2_frame_size, Rectangle rect_frame_window)
    : TgaFrame(vec_file) {
  int2_frame_size_ = int2_frame_size;
  vector2_offset_ = core::Vector2{
      0.5f * static_cast<float>(rect_frame_window.Left() + rect_frame_window.Right() - int2_frame_size_.X),
      0.5f * static_cast<float>(rect_frame_window.Top() + rect_frame_window.Bottom() - int2_frame_size_.Y)};
}

TgaSprite::TgaSprite(std::span<const std::byte> vec_file) {
  vec_frames_.push_back(std::make_unique<TgaFrame>(vec_file));
}

bool TryParseTga(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsTga(vec_file)) {
    vec_frames.clear();
    return false;
  }

  vec_frames.clear();
  vec_frames.push_back(std::make_unique<TgaFrame>(vec_file));
  return true;
}

}  // namespace ora::fmt
