#pragma once
import std;

#include "formats/targa.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsShpRemastered(std::span<const std::byte> vec_file);

bool TryParseShpRemastered(std::span<const std::byte> vec_file,
                           std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

class ShpRemasteredSprite {
 public:
  explicit ShpRemasteredSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }

  std::vector<std::unique_ptr<gfx::ISpriteFrame>> TakeFrames() && { return std::move(vec_frames_); }

 private:
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}
