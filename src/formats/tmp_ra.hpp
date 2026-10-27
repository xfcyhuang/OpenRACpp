#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsTmpRA(std::span<const std::byte> vec_file);

bool TryParseTmpRA(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}
