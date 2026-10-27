#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsTmpTD(std::span<const std::byte> vec_file);

bool TryParseTmpTD(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}
