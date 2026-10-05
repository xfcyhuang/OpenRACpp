// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpRALoader.cs @b6fc03f L17-102
// 实现文件:TmpRAFrame / IsTmpRA / ParseFrames 逐句照抄;Stream →
// SpanReader。
// Implementation file: TmpRAFrame / IsTmpRA / ParseFrames copied
// statement by statement; Stream → SpanReader.
#include "formats/tmp_ra.hpp"

#include "formats/span_reader.hpp"

namespace ora::fmt {

namespace {

/// TmpRAFrame(L20-40)。
class TmpRAFrame final : public gfx::ISpriteFrame {
 public:
  TmpRAFrame(std::span<const std::byte> vec_data, int2 int2_tile_size) : int2_frame_size_(int2_tile_size) {
    if (!vec_data.empty())
      int2_size_ = int2_tile_size;

    vec_data_.assign_range(vec_data);
  }

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Indexed8; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return core::Vector2{}; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

 private:
  int2 int2_size_{};
  int2 int2_frame_size_{};
  std::vector<std::byte> vec_data_;
};

}  // namespace

bool IsTmpRA(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto int8_start = stream.Position();

  stream.Skip(20);
  const auto uint4_a = stream.ReadUInt32();
  stream.Skip(2);
  const auto uint2_b = stream.ReadUInt16();

  stream.Seek(int8_start);
  return uint4_a == 0 && uint2_b == 0x2C73;
}

namespace {

/// ParseFrames(L63-90)。
std::vector<std::unique_ptr<gfx::ISpriteFrame>> ParseFrames(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto uint2_width = stream.ReadUInt16();
  const auto uint2_height = stream.ReadUInt16();
  const auto int2_size = int2{uint2_width, uint2_height};

  stream.Skip(12);
  const auto uint4_img_start = stream.ReadUInt32();
  stream.Skip(8);
  const auto int4_index_end = stream.ReadInt32();
  stream.Skip(4);
  const auto int4_index_start = stream.ReadInt32();

  stream.Seek(int4_index_start);
  const auto int4_count = int4_index_end - int4_index_start;
  auto vec_tiles = std::vector<std::unique_ptr<gfx::ISpriteFrame>>{};
  vec_tiles.reserve(static_cast<std::size_t>(int4_count));
  for (const auto byte_b : stream.ReadBytes(int4_count)) {
    if (byte_b != std::byte{255}) {
      stream.Seek(uint4_img_start +
                  static_cast<std::int64_t>(static_cast<std::uint8_t>(byte_b)) * uint2_width * uint2_height);
      vec_tiles.push_back(std::make_unique<TmpRAFrame>(stream.ReadBytes(uint2_width * uint2_height), int2_size));
    } else
      vec_tiles.push_back(std::make_unique<TmpRAFrame>(std::span<const std::byte>{}, int2_size));
  }

  return vec_tiles;
}

}  // namespace

bool TryParseTmpRA(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsTmpRA(vec_file))
    return false;

  vec_frames = ParseFrames(vec_file);
  return true;
}

}  // namespace ora::fmt
