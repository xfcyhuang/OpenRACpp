#pragma once
import std;

#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsDds(std::span<const std::byte> vec_file);

bool TryParseDds(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

class DdsFrame final : public gfx::ISpriteFrame {
 public:
  explicit DdsFrame(std::span<const std::byte> vec_file);

  gfx::SpriteFrameType Type() const override { return type_frame_; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_size_; }
  core::Vector2 Offset() const override { return core::Vector2{0.0f, 0.0f}; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

 private:
  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Bgra32;
  int2 int2_size_{};
  std::vector<std::byte> vec_data_;
};

class DdsSprite {
 public:
  explicit DdsSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }

 private:
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}
