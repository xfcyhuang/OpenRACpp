// UPSTREAM: OpenRA.Game/Graphics/PlatformInterfaces.cs @1f594e320 L192-203
// (FontGlyph + IFont;Dispose 随 C++ 析构)。FreeTypeFont(freetype_font.hpp)
// 实现该抽象;SpriteFont 经此消费(上游 Platform.CreateFont 的返回型)。
// FontGlyph 自 freetype_font.hpp 上移至此(接口与其载荷同域;避免相互包含)。
// FontGlyph + IFont (PlatformInterfaces.cs L192-203; Dispose arrives as the
// C++ destructor). FreeTypeFont (freetype_font.hpp) implements the
// abstraction; SpriteFont consumes it (the return type of upstream's
// Platform.CreateFont). FontGlyph moves up here from freetype_font.hpp (the
// interface and its payload share a home; avoids a mutual include).
#pragma once
import std;

#include "core/int2.hpp"

namespace ora::platform {

/// 字形(FontGlyph,PlatformInterfaces.cs L197-203;Advance 上游为 float ——
/// 赋值源是 26.6 右移后的整数)。
/// A glyph (FontGlyph, PlatformInterfaces.cs L197-203; Advance is a float
/// upstream — its assignment source is the post-26.6-shift integer).
struct FontGlyph {
  int2 offset{};                // bitmap_left / -bitmap_top
  std::int32_t int4_width = 0;  // metrics.width >> 6
  std::int32_t int4_height = 0; // metrics.height >> 6
  float fp4_advance = 0.0f;     // metrics.horiAdvance >> 6
  std::vector<std::uint8_t> vec_data;  // 行主序位图(每行 width 字节)| row-major bitmap (width bytes per row)

  /// 上游 EmptyGlyph 的等价判定(Data == null / 全零)。
  /// The equivalent of upstream's EmptyGlyph test (Data == null / all-zero).
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

}  // namespace ora::platform
