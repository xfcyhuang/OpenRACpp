#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsShpD2(std::span<const std::byte> vec_file);

bool TryParseShpD2(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}
