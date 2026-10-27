#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsTmpTS(std::span<const std::byte> vec_file);

bool TryParseTmpTS(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}
