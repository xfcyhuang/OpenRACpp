// UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/ShpTSLoader.cs @7d57605 L17-163
// 实现文件:ShpTSFrame 头解析与三种扫描线格式、IsShpTS 判定循环、
// ParseFrames 逐句照抄;Stream → SpanReader。
// Implementation file: the ShpTSFrame header parse with its three
// scanline formats, the IsShpTS probe loop, and ParseFrames copied
// statement by statement; Stream → SpanReader.
#include "formats/shp_ts.hpp"

#include "formats/rle_zeros.hpp"
#include "formats/span_reader.hpp"

namespace ora::fmt {

namespace {

/// ShpTSFrame(L20-102)。
class ShpTSFrame final : public gfx::ISpriteFrame {
 public:
  ShpTSFrame(SpanReader& stream, int2 int2_frame_size) : int2_frame_size_(int2_frame_size) {
    const auto uint2_x = stream.ReadUInt16();
    const auto uint2_y = stream.ReadUInt16();
    const auto uint2_width = stream.ReadUInt16();
    const auto uint2_height = stream.ReadUInt16();

    // 宽高取偶避免半整数偏移 | pad the dimensions to an even number to
    // avoid issues with half-integer offsets
    auto int4_data_width = static_cast<std::int32_t>(uint2_width);
    auto int4_data_height = static_cast<std::int32_t>(uint2_height);
    if (int4_data_width % 2 == 1)
      int4_data_width++;

    if (int4_data_height % 2 == 1)
      int4_data_height++;

    vec_offset_ = core::Vector2{static_cast<float>(uint2_x + (int4_data_width - int2_frame_size.X) / 2),
                                static_cast<float>(uint2_y + (int4_data_height - int2_frame_size.Y) / 2)};
    int2_size_ = int2{int4_data_width, int4_data_height};

    uint1_format_ = stream.ReadUInt8();
    stream.Skip(11);
    uint4_file_offset_ = stream.ReadUInt32();

    if (uint4_file_offset_ == 0)
      return;

    // 边解析帧数据边前进(返回前跳回头表位置;上游注释)。
    // Parse the frame data as we go (but remember to jump back to the
    // header before returning! — upstream comment).
    const auto int8_start = stream.Position();
    stream.Seek(uint4_file_offset_);

    vec_data_.assign(static_cast<std::size_t>(int4_data_width) * static_cast<std::size_t>(int4_data_height),
                     std::byte{0});

    if (uint1_format_ == 3) {
      // Format 3:RLE-zero 压缩扫描线 | RLE-zero compressed scanlines
      for (auto int4_j = 0; int4_j < uint2_height; int4_j++) {
        const auto int4_length = stream.ReadUInt16() - 2;
        rle_zeros::DecodeInto(stream.ReadBytes(int4_length), vec_data_,
                              static_cast<std::size_t>(int4_data_width) * static_cast<std::size_t>(int4_j));
      }
    } else {
      // Format 2:未压缩长度前缀扫描线;Format 1/0:未压缩整宽行。
      // Format 2: uncompressed length-prefixed scanlines; Formats 1/0:
      // an uncompressed full-width row.
      const auto int4_length = uint1_format_ == 2 ? stream.ReadUInt16() - 2 : static_cast<std::int32_t>(uint2_width);
      for (auto int4_j = 0; int4_j < uint2_height; int4_j++)
        stream.ReadInto(vec_data_, static_cast<std::size_t>(int4_data_width) * static_cast<std::size_t>(int4_j),
                        static_cast<std::size_t>(int4_length));
    }

    stream.Seek(int8_start);
  }

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Indexed8; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return vec_offset_; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

 private:
  int2 int2_size_{};
  int2 int2_frame_size_{};
  core::Vector2 vec_offset_{};
  std::uint8_t uint1_format_ = 0;
  std::uint32_t uint4_file_offset_ = 0;
  std::vector<std::byte> vec_data_;
};

}  // namespace

bool IsShpTS(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto int8_start = stream.Position();

  // 首 word 为零 | the first word is zero
  if (stream.ReadUInt16() != 0) {
    stream.Seek(int8_start);
    return false;
  }

  // 帧数合理性检查 | sanity-check the image count
  stream.Skip(4);
  const auto uint2_image_count = stream.ReadUInt16();
  if (stream.Position() + 24 * static_cast<std::int64_t>(uint2_image_count) > stream.Length()) {
    stream.Seek(int8_start);
    return false;
  }

  // 检查图像尺寸与压缩格式标志;部分文件含伪帧,循环直到找到有效帧。
  // Check the image size and compression type flag; some files define
  // bogus frames, so loop until we find a valid one.
  stream.Skip(4);
  std::uint16_t uint2_w, uint2_h, uint2_f = 0;
  std::uint8_t uint1_type;
  do {
    uint2_w = stream.ReadUInt16();
    uint2_h = stream.ReadUInt16();
    uint1_type = stream.ReadUInt8();

    // 零尺寸帧总带非零 type | zero-sized frames always define a non-zero type
    if ((uint2_w == 0 || uint2_h == 0) && uint1_type == 0) {
      // 上游此路径不复位 stream(逐字照抄;C++ 侧每个判定从文件头独立
      // 起读,链式影响不可观测 —— COVERAGE 登记)。
      // Upstream does not restore the stream on this path (copied
      // verbatim; every C++ probe starts fresh at offset 0, making the
      // chained effect unobservable — registered in COVERAGE).
      return false;
    }

    stream.Skip(19);
  } while (uint2_w == 0 && uint2_h == 0 && ++uint2_f < uint2_image_count);

  stream.Seek(int8_start);
  return uint2_f == uint2_image_count || uint1_type < 4;
}

namespace {

/// ParseFrames(L142-153)。
std::vector<std::unique_ptr<gfx::ISpriteFrame>> ParseFrames(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};

  stream.ReadUInt16();
  const auto uint2_width = stream.ReadUInt16();
  const auto uint2_height = stream.ReadUInt16();
  const auto int2_size = int2{uint2_width, uint2_height};
  const auto uint2_frame_count = stream.ReadUInt16();

  auto vec_frames = std::vector<std::unique_ptr<gfx::ISpriteFrame>>{};
  vec_frames.reserve(uint2_frame_count);
  for (auto int4_i = 0; int4_i < uint2_frame_count; int4_i++)
    vec_frames.push_back(std::make_unique<ShpTSFrame>(stream, int2_size));

  return vec_frames;
}

}  // namespace

bool TryParseShpTS(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsShpTS(vec_file))
    return false;

  vec_frames = ParseFrames(vec_file);
  return true;
}

}  // namespace ora::fmt
