#include "formats/tmp_ts.hpp"

#include "core/rectangle.hpp"
#include "formats/span_reader.hpp"

namespace ora::fmt {

namespace {

class TmpTSFrame;

class TmpTSDepthFrame final : public gfx::ISpriteFrame {
 public:
  explicit TmpTSDepthFrame(const TmpTSFrame& parent) : ptr_parent_(&parent) {}

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Indexed8; }
  int2 Size() const override;
  int2 FrameSize() const override { return Size(); }
  core::Vector2 Offset() const override;
  std::span<const std::byte> Data() const override;
  bool DisableExportPadding() const override { return false; }

 private:
  const TmpTSFrame* ptr_parent_;
};

void UnpackTileData(SpanReader& stream, std::span<std::byte> vec_data, int2 int2_size, Rectangle rect_frame_bounds) {
  auto int4_width = 4;
  for (auto int4_j = 0; int4_j < int2_size.Y; int4_j++) {
    const auto int4_start =
        (int4_j - rect_frame_bounds.Y) * rect_frame_bounds.Width + (int2_size.X - int4_width) / 2 - rect_frame_bounds.X;
    stream.ReadInto(vec_data, static_cast<std::size_t>(int4_start), static_cast<std::size_t>(int4_width));

    int4_width += (int4_j < int2_size.Y / 2 - 1 ? 1 : -1) * 4;
  }
}

class TmpTSFrame final : public gfx::ISpriteFrame {
 public:
  TmpTSFrame(SpanReader& stream, int2 int2_tile_size, std::int32_t int4_u, std::int32_t int4_v) {
    if (stream.Position() != 0) {
      int2_size_ = int2_tile_size;

      stream.Skip(20);

      const auto int4_extra_x = stream.ReadInt32() - (int4_u - int4_v) * int2_tile_size.X / 2;
      const auto int4_extra_y = stream.ReadInt32() - (int4_u + int4_v) * int2_tile_size.Y / 2;
      const auto int4_extra_width = stream.ReadInt32();
      const auto int4_extra_height = stream.ReadInt32();
      const auto uint4_flags = stream.ReadUInt32();

      auto rect_bounds = Rectangle{0, 0, int2_tile_size.X, int2_tile_size.Y};
      if ((uint4_flags & 0x01) != 0) {
        const auto rect_extra_bounds = Rectangle{int4_extra_x, int4_extra_y, int4_extra_width, int4_extra_height};
        rect_bounds = Rectangle::Union(rect_bounds, rect_extra_bounds);

        vec_offset_ = core::Vector2{static_cast<float>(rect_bounds.X + 0.5f * (rect_bounds.Width - int2_tile_size.X)),
                                    static_cast<float>(rect_bounds.Y + 0.5f * (rect_bounds.Height - int2_tile_size.Y))};
        int2_size_ = int2{rect_bounds.Width, rect_bounds.Height};
      }

      stream.Skip(12);

      vec_data_.assign(static_cast<std::size_t>(rect_bounds.Width) * static_cast<std::size_t>(rect_bounds.Height),
                       std::byte{0});
      vec_depth_data_.assign(static_cast<std::size_t>(rect_bounds.Width) * static_cast<std::size_t>(rect_bounds.Height),
                             std::byte{0});

      UnpackTileData(stream, vec_data_, int2_tile_size, rect_bounds);
      UnpackTileData(stream, vec_depth_data_, int2_tile_size, rect_bounds);

      if ((uint4_flags & 0x01) == 0)
        return;

      for (auto int4_j = 0; int4_j < int4_extra_height; int4_j++) {
        const auto int4_start =
            (int4_j + int4_extra_y - rect_bounds.Y) * rect_bounds.Width + int4_extra_x - rect_bounds.X;
        for (auto int4_i = 0; int4_i < int4_extra_width; int4_i++) {
          const auto byte_extra = stream.ReadUInt8();
          if (byte_extra != 0)
            vec_data_.at(static_cast<std::size_t>(int4_start + int4_i)) = static_cast<std::byte>(byte_extra);
        }
      }

      for (auto int4_j = 0; int4_j < int4_extra_height; int4_j++) {
        const auto int4_start =
            (int4_j + int4_extra_y - rect_bounds.Y) * rect_bounds.Width + int4_extra_x - rect_bounds.X;
        for (auto int4_i = 0; int4_i < int4_extra_width; int4_i++) {
          const auto byte_extra = stream.ReadUInt8();

          if (byte_extra < 32)
            vec_depth_data_.at(static_cast<std::size_t>(int4_start + int4_i)) = static_cast<std::byte>(byte_extra);
        }
      }
    } else
      vec_data_.clear();
  }

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Indexed8; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return Size(); }
  core::Vector2 Offset() const override { return vec_offset_; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  std::span<const std::byte> DepthData() const { return vec_depth_data_; }
  bool DisableExportPadding() const override { return false; }

 private:
  friend class TmpTSDepthFrame;

  int2 int2_size_{};
  core::Vector2 vec_offset_{};
  std::vector<std::byte> vec_data_;
  std::vector<std::byte> vec_depth_data_;
};

int2 TmpTSDepthFrame::Size() const { return ptr_parent_->Size(); }
core::Vector2 TmpTSDepthFrame::Offset() const { return ptr_parent_->Offset(); }
std::span<const std::byte> TmpTSDepthFrame::Data() const { return ptr_parent_->DepthData(); }

}

bool IsTmpTS(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto int8_start = stream.Position();
  stream.Skip(8);
  const auto uint4_sx = stream.ReadUInt32();
  const auto uint4_sy = stream.ReadUInt32();

  auto uint4_offset = stream.ReadUInt32();
  while (uint4_offset == 0)
    uint4_offset = stream.ReadUInt32();

  if (uint4_offset > static_cast<std::uint32_t>(stream.Length()) - 52) {
    stream.Seek(int8_start);
    return false;
  }

  stream.Seek(static_cast<std::int64_t>(uint4_offset) + 12);
  const auto uint4_test = stream.ReadUInt32();

  stream.Seek(int8_start);
  return uint4_test == uint4_sx * uint4_sy / 2 + 52;
}

namespace {

std::vector<std::unique_ptr<gfx::ISpriteFrame>> ParseFrames(std::span<const std::byte> vec_file) {
  auto stream = SpanReader{vec_file};
  const auto uint4_template_width = stream.ReadUInt32();
  const auto uint4_template_height = stream.ReadUInt32();
  const auto int4_tile_width = stream.ReadInt32();
  const auto int4_tile_height = stream.ReadInt32();
  const auto int2_size = int2{int4_tile_width, int4_tile_height};
  auto arr_offsets = std::vector<std::uint32_t>(static_cast<std::size_t>(uint4_template_width) *
                                                static_cast<std::size_t>(uint4_template_height));
  for (auto& uint4_offset : arr_offsets)
    uint4_offset = stream.ReadUInt32();

  const auto st_stride = arr_offsets.size();
  auto vec_tiles = std::vector<std::unique_ptr<gfx::ISpriteFrame>>(st_stride * 2);

  for (auto uint4_j = std::uint32_t{0}; uint4_j < uint4_template_height; uint4_j++) {
    for (auto uint4_i = std::uint32_t{0}; uint4_i < uint4_template_width; uint4_i++) {
      const auto st_k = static_cast<std::size_t>(uint4_j) * static_cast<std::size_t>(uint4_template_width) +
                        static_cast<std::size_t>(uint4_i);
      stream.Seek(arr_offsets[st_k]);

      auto frame = std::make_unique<TmpTSFrame>(stream, int2_size, static_cast<std::int32_t>(uint4_i),
                                                static_cast<std::int32_t>(uint4_j));
      vec_tiles[st_k + st_stride] = std::make_unique<TmpTSDepthFrame>(*frame);
      vec_tiles[st_k] = std::move(frame);
    }
  }

  return vec_tiles;
}

}

bool TryParseTmpTS(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsTmpTS(vec_file))
    return false;

  vec_frames = ParseFrames(vec_file);
  return true;
}

}
