#include "formats/shp_ts.hpp"

#include "formats/rle_zeros.hpp"
#include "formats/span_reader.hpp"

namespace ora::fmt {

namespace {

class ShpTSFrame final : public gfx::ISpriteFrame {
 public:
  ShpTSFrame(SpanReader& stream, int2 int2_frame_size) : int2_frame_size_(int2_frame_size) {
    const auto uint2_x = stream.ReadUInt16();
    const auto uint2_y = stream.ReadUInt16();
    const auto uint2_width = stream.ReadUInt16();
    const auto uint2_height = stream.ReadUInt16();

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

    const auto int8_start = stream.Position();
    stream.Seek(uint4_file_offset_);

    vec_data_.assign(static_cast<std::size_t>(int4_data_width) * static_cast<std::size_t>(int4_data_height),
                     std::byte{0});

    if (uint1_format_ == 3) {

      for (auto int4_j = 0; int4_j < uint2_height; int4_j++) {
        const auto int4_length = stream.ReadUInt16() - 2;
        rle_zeros::DecodeInto(stream.ReadBytes(int4_length), vec_data_,
                              static_cast<std::size_t>(int4_data_width) * static_cast<std::size_t>(int4_j));
      }
    } else {

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

}

bool IsShpTS(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto int8_start = stream.Position();

  if (stream.ReadUInt16() != 0) {
    stream.Seek(int8_start);
    return false;
  }

  stream.Skip(4);
  const auto uint2_image_count = stream.ReadUInt16();
  if (stream.Position() + 24 * static_cast<std::int64_t>(uint2_image_count) > stream.Length()) {
    stream.Seek(int8_start);
    return false;
  }

  stream.Skip(4);
  std::uint16_t uint2_w, uint2_h, uint2_f = 0;
  std::uint8_t uint1_type;
  do {
    uint2_w = stream.ReadUInt16();
    uint2_h = stream.ReadUInt16();
    uint1_type = stream.ReadUInt8();

    if ((uint2_w == 0 || uint2_h == 0) && uint1_type == 0) {

      return false;
    }

    stream.Skip(19);
  } while (uint2_w == 0 && uint2_h == 0 && ++uint2_f < uint2_image_count);

  stream.Seek(int8_start);
  return uint2_f == uint2_image_count || uint1_type < 4;
}

namespace {

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

}

bool TryParseShpTS(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsShpTS(vec_file))
    return false;

  vec_frames = ParseFrames(vec_file);
  return true;
}

}
