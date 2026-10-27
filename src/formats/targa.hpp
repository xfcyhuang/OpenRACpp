#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "formats/span_reader.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsTga(std::span<const std::byte> vec_file);

bool TryParseTga(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

class TgaFrame final : public gfx::ISpriteFrame {
 public:

  TgaFrame() = default;

  explicit TgaFrame(std::span<const std::byte> vec_file);

  TgaFrame(std::span<const std::byte> vec_file, int2 int2_frame_size, Rectangle rect_frame_window);

  gfx::SpriteFrameType Type() const override { return type_frame_; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return vector2_offset_; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

 private:

  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Indexed8;
  int2 int2_size_{};
  int2 int2_frame_size_{};
  core::Vector2 vector2_offset_{};
  std::vector<std::byte> vec_data_;
};

class TgaSprite {
 public:
  explicit TgaSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }

 private:
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}
