#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "core/wangle.hpp"
#include "formats/png.hpp"
#include "gfx/sprite.hpp"
#include "gfx/vertex.hpp"

namespace ora::gfx {

constexpr core::Vector2 operator+(core::Vector2 a, core::Vector2 b) { return {a.X + b.X, a.Y + b.Y}; }
constexpr core::Vector2 operator-(core::Vector2 a, core::Vector2 b) { return {a.X - b.X, a.Y - b.Y}; }
constexpr core::Vector2 operator*(core::Vector2 v, float k) { return {v.X * k, v.Y * k}; }
constexpr core::Vector2 operator*(float k, core::Vector2 v) { return v * k; }
constexpr core::Vector2 operator-(core::Vector2 v) { return {-v.X, -v.Y}; }

constexpr core::Vector3 operator+(core::Vector3 a, core::Vector3 b) { return {a.X + b.X, a.Y + b.Y, a.Z + b.Z}; }
constexpr core::Vector3 operator-(core::Vector3 a, core::Vector3 b) { return {a.X - b.X, a.Y - b.Y, a.Z - b.Z}; }
constexpr core::Vector3 operator*(core::Vector3 v, float k) { return {v.X * k, v.Y * k, v.Z * k}; }
constexpr core::Vector3 operator*(float k, core::Vector3 v) { return v * k; }
constexpr core::Vector3 operator*(core::Vector3 a, core::Vector3 b) { return {a.X * b.X, a.Y * b.Y, a.Z * b.Z}; }
constexpr core::Vector3 operator/(core::Vector3 v, float k) { return v * (1.0f / k); }
constexpr core::Vector3 operator-(core::Vector3 v) { return {-v.X, -v.Y, -v.Z}; }

inline float Length(core::Vector2 v) { return std::sqrt(v.X * v.X + v.Y * v.Y); }

std::vector<std::uint32_t> CreateQuadIndices(std::int32_t int4_quads);

void FastCreateQuad(std::span<Vertex> vec_vertices, core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c,
                    core::Vector3 v_d, const Sprite& sprite_r, int2 int2_samplers, std::int32_t int4_palette_index,
                    core::Vector3 v_tint, float float_alpha, std::int32_t int4_nv);

void FastCreateQuad(std::span<Vertex> vec_vertices, core::Vector3 v_o, const Sprite& sprite_r, int2 int2_samplers,
                    std::int32_t int4_palette_index, std::int32_t int4_nv, core::Vector3 v_size, core::Vector3 v_tint,
                    float float_alpha, float float_rotation = 0.0f);

void FastCopyIntoChannel(const Sprite& sprite_dest, std::span<const std::byte> vec_src, SpriteFrameType kind_src_type,
                         bool b_premultiplied = false);

void FastCopyIntoSprite(const Sprite& sprite_dest, const fmt::Png& png_src);

void RotateQuadInto(std::span<core::Vector3> vec_des, core::Vector3 v_tl, core::Vector3 v_size, float float_rotation);

Rectangle BoundingRectangle(core::Vector3 v_offset, core::Vector3 v_size, float float_rotation);

constexpr std::int32_t NextPowerOf2(std::int32_t int4_v) {
  if (int4_v <= 1)
    return 1;
  std::uint32_t u = static_cast<std::uint32_t>(int4_v - 1);
  u |= u >> 1;
  u |= u >> 2;
  u |= u >> 4;
  u |= u >> 8;
  u |= u >> 16;
  return static_cast<std::int32_t>(u + 1);
}

constexpr bool IsPowerOf2(std::int32_t int4_v) { return int4_v > 0 && (int4_v & (int4_v - 1)) == 0; }

constexpr float Lerp(float fp4_a, float fp4_b, float fp4_t) { return fp4_a + fp4_t * (fp4_b - fp4_a); }

constexpr std::int32_t IndexFacing(WAngle wangle_facing, std::int32_t int4_num_frames) {
  const std::int32_t int4_step = 1024 / int4_num_frames;
  const std::int32_t int4_a = (wangle_facing.Angle + int4_step / 2) & 1023;
  return int4_a / int4_step;
}

constexpr WAngle AngleDiffToStep(WAngle wangle_facing, std::int32_t int4_num_frames) {
  const std::int32_t int4_step = 1024 / int4_num_frames;
  const std::int32_t int4_a = (wangle_facing.Angle + int4_step / 2) & 1023;
  return WAngle{int4_a % int4_step - int4_step / 2};
}

constexpr WAngle GetInterpolatedFacingRotation(WAngle wangle_facing, std::int32_t int4_facings,
                                               std::int32_t int4_interpolated_facings) {
  const std::int32_t int4_step = 1024 / int4_interpolated_facings;
  return WAngle{AngleDiffToStep(wangle_facing, int4_facings).Angle / int4_step * int4_step};
}

constexpr bool PolygonContains(std::span<const int2> vec_polygon, int2 int2_p) {
  const auto winding_test = [](int2 v0, int2 v1, int2 p) {
    return int2::SignOf((v1.X - v0.X) * (p.Y - v0.Y) - (p.X - v0.X) * (v1.Y - v0.Y));
  };

  auto int4_winding_number = 0;
  for (std::size_t int4_i{}; int4_i < vec_polygon.size(); int4_i++) {
    const int2 tv = vec_polygon[int4_i];
    const int2 nv = vec_polygon[(int4_i + 1) % vec_polygon.size()];

    if (tv.Y <= int2_p.Y && nv.Y > int2_p.Y && winding_test(tv, nv, int2_p) > 0)
      int4_winding_number++;
    else if (tv.Y > int2_p.Y && nv.Y <= int2_p.Y && winding_test(tv, nv, int2_p) < 0)
      int4_winding_number--;
  }

  return int4_winding_number != 0;
}

inline core::Color PremultiplyAlpha(core::Color color_c) {
  const std::uint32_t uint4_argb = color_c.ToArgb();
  const std::uint32_t uint4_a = uint4_argb >> 24;

  if (uint4_a == 255)
    return color_c;

  if (uint4_a == 0)
    return core::Color{};

  std::uint32_t uint4_r = (uint4_argb >> 16) & 0xFF;
  std::uint32_t uint4_g = (uint4_argb >> 8) & 0xFF;
  std::uint32_t uint4_b = uint4_argb & 0xFF;

  uint4_r = uint4_r * uint4_a + 128;
  uint4_r = (uint4_r + (uint4_r >> 8)) >> 8;
  uint4_g = uint4_g * uint4_a + 128;
  uint4_g = (uint4_g + (uint4_g >> 8)) >> 8;
  uint4_b = uint4_b * uint4_a + 128;
  uint4_b = (uint4_b + (uint4_b >> 8)) >> 8;

  return core::Color::FromArgbRaw((uint4_a << 24) | (uint4_r << 16) | (uint4_g << 8) | uint4_b);
}

inline core::Vector2 FromAngle(float fp4_angle) {
  return core::Vector2{std::sin(fp4_angle), std::cos(fp4_angle)};
}

}
