#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

bool IsShpTS(std::span<const std::byte> vec_file);

bool TryParseShpTS(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}
