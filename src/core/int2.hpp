#pragma once
import std;

#include "core/exts_math.hpp"
#include "core/vector_n.hpp"

namespace ora {

struct Rectangle;

struct int2 {
  std::int32_t X{0};
  std::int32_t Y{0};

  constexpr int2() = default;
  constexpr int2(std::int32_t int4_x, std::int32_t int4_y) : X{int4_x}, Y{int4_y} {}

  static constexpr int2 Zero() { return int2{0, 0}; }

  static constexpr std::int32_t SignOf(std::int32_t int4_v) { return (int4_v > 0) - (int4_v < 0); }

  constexpr int2 Sign() const { return int2{SignOf(X), SignOf(Y)}; }
  constexpr int2 Abs() const { return int2{X < 0 ? -X : X, Y < 0 ? -Y : Y}; }
  constexpr std::int32_t LengthSquared() const { return X * X + Y * Y; }
  constexpr std::int32_t Length() const { return ISqrt(LengthSquared()); }

  constexpr int2 WithX(std::int32_t int4_new_x) const { return int2{int4_new_x, Y}; }
  constexpr int2 WithY(std::int32_t int4_new_y) const { return int2{X, int4_new_y}; }

  static constexpr int2 Max(int2 v_a, int2 v_b) {
    return int2{v_a.X > v_b.X ? v_a.X : v_b.X, v_a.Y > v_b.Y ? v_a.Y : v_b.Y};
  }
  static constexpr int2 Min(int2 v_a, int2 v_b) {
    return int2{v_a.X < v_b.X ? v_a.X : v_b.X, v_a.Y < v_b.Y ? v_a.Y : v_b.Y};
  }

  static constexpr std::int32_t Lerp(std::int32_t int4_a, std::int32_t int4_b,
                                     std::int32_t int4_mul, std::int32_t int4_div) {
    return int4_a + (int4_b - int4_a) * int4_mul / int4_div;
  }

  static constexpr int2 Lerp(int2 v_a, int2 v_b, std::int32_t int4_mul, std::int32_t int4_div) {
    return v_a + (v_b - v_a) * int4_mul / int4_div;
  }

  static constexpr std::int32_t Dot(int2 v_a, int2 v_b) { return v_a.X * v_b.X + v_a.Y * v_b.Y; }

  static constexpr std::uint32_t Swap(std::uint32_t uint4_orig) {
    return ((uint4_orig & 0xff000000u) >> 24) | ((uint4_orig & 0x00ff0000u) >> 8) |
           ((uint4_orig & 0x0000ff00u) << 8) | ((uint4_orig & 0x000000ffu) << 24);
  }

  constexpr int2 Clamp(Rectangle const& rect_r) const;

  constexpr core::Vector2 ToVector2() const {
    return core::Vector2{static_cast<float>(X), static_cast<float>(Y)};
  }

  static constexpr int2 FromVector(core::Vector2 v_vec) {
    return int2{static_cast<std::int32_t>(v_vec.X), static_cast<std::int32_t>(v_vec.Y)};
  }

  friend constexpr int2 operator+(int2 v_a, int2 v_b) { return int2{v_a.X + v_b.X, v_a.Y + v_b.Y}; }
  friend constexpr int2 operator-(int2 v_a, int2 v_b) { return int2{v_a.X - v_b.X, v_a.Y - v_b.Y}; }
  friend constexpr int2 operator*(std::int32_t int4_a, int2 v_b) {
    return int2{int4_a * v_b.X, int4_a * v_b.Y};
  }
  friend constexpr int2 operator*(int2 v_b, std::int32_t int4_a) { return int4_a * v_b; }
  friend constexpr int2 operator/(int2 v_a, std::int32_t int4_b) {
    return int2{v_a.X / int4_b, v_a.Y / int4_b};
  }
  friend constexpr int2 operator-(int2 v_a) { return int2{-v_a.X, -v_a.Y}; }
  friend constexpr bool operator==(int2 v_a, int2 v_b) { return v_a.X == v_b.X && v_a.Y == v_b.Y; }
  friend constexpr bool operator!=(int2 v_a, int2 v_b) { return !(v_a == v_b); }
};

}
