#pragma once
import std;

#include "core/int2.hpp"

namespace ora::platform {

struct FontGlyph {
  int2 offset{};
  std::int32_t int4_width = 0;
  std::int32_t int4_height = 0;
  float fp4_advance = 0.0f;
  std::vector<std::uint8_t> vec_data;

  [[nodiscard]] bool Empty() const {
    return int4_width == 0 && int4_height == 0 && fp4_advance == 0.0f && vec_data.empty();
  }
};

class IFont {
 public:
  virtual ~IFont() = default;

  virtual FontGlyph CreateGlyph(std::uint32_t uint4_char_code, int int4_size,
                                float fp4_device_scale) = 0;

 protected:
  IFont() = default;
};

}
