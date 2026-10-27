#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"

namespace ora::gfx {

enum class TextureChannel : std::uint8_t {
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
  RGBA = 4,
};

enum class BlendMode : std::uint8_t {
  None,
  Alpha,
  Additive,
  Subtractive,
  Multiply,
  Multiplicative,
  DoubleMultiplicative,
  LowAdditive,
  Screen,
  Translucent,
};

enum class SpriteFrameType : std::uint8_t {
  Indexed8,
  Bgra32,
  Bgr24,
  Rgba32,
  Rgb24,
};

enum class SheetType : std::uint8_t {
  Indexed = 1,
  BGRA = 4,
};

class Sheet;

struct Sprite {
  Rectangle Bounds;
  Sheet* ptr_sheet = nullptr;
  BlendMode kind_blend = BlendMode::Alpha;
  TextureChannel kind_channel = TextureChannel::Red;
  float float_z_ramp = 0.0f;
  core::Vector3 vec_size{};
  core::Vector3 vec_offset{};

  float float_top = 0.0f, float_left = 0.0f, float_bottom = 0.0f, float_right = 0.0f;

  bool b_secondary = false;
  Sheet* ptr_secondary_sheet = nullptr;
  Rectangle SecondaryBounds;
  TextureChannel kind_secondary_channel = TextureChannel::Red;
  float float_secondary_top = 0.0f, float_secondary_left = 0.0f;
  float float_secondary_bottom = 0.0f, float_secondary_right = 0.0f;

  constexpr Sprite() = default;

  Sprite(Sheet& sheet, Rectangle bounds, TextureChannel channel, float scale = 1.0f);
  Sprite(Sheet& sheet, Rectangle bounds, float z_ramp, core::Vector3 offset, TextureChannel channel,
         BlendMode blend_mode = BlendMode::Alpha, float scale = 1.0f);
};

struct SpriteWithSecondaryData : Sprite {

  SpriteWithSecondaryData(const Sprite& sprite, Sheet& secondary_sheet, Rectangle secondary_bounds,
                          TextureChannel secondary_channel);
};

}
