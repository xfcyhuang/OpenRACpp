#pragma once
import std;

#include "gfx/sequence_set.hpp"

namespace ora::mods {

std::unique_ptr<gfx::ISpriteSequenceLoader> MakeSequenceLoader(const std::string& str_format);

}
