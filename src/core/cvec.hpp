#pragma once
import std;

#include "exts_math.hpp"
#include "rectangle.hpp"

namespace ora {

struct CVec {
  std::int32_t X{0};
  std::int32_t Y{0};

  constexpr CVec() = default;
  constexpr CVec(std::int32_t int4_x, std::int32_t int4_y) : X{int4_x}, Y{int4_y} {}

  static constexpr CVec Zero() { return CVec{0, 0}; }

  static constexpr CVec Max(CVec v_a, CVec v_b) {
    return CVec{v_a.X > v_b.X ? v_a.X : v_b.X, v_a.Y > v_b.Y ? v_a.Y : v_b.Y};
  }
  static constexpr CVec Min(CVec v_a, CVec v_b) {
    return CVec{v_a.X < v_b.X ? v_a.X : v_b.X, v_a.Y < v_b.Y ? v_a.Y : v_b.Y};
  }
  static constexpr std::int32_t Dot(CVec v_a, CVec v_b) { return v_a.X * v_b.X + v_a.Y * v_b.Y; }

  constexpr CVec Sign() const { return CVec{int2::SignOf(X), int2::SignOf(Y)}; }
  constexpr CVec Abs() const { return CVec{X < 0 ? -X : X, Y < 0 ? -Y : Y}; }
  constexpr std::int32_t LengthSquared() const { return X * X + Y * Y; }
  constexpr std::int32_t Length() const { return ISqrt(LengthSquared()); }

  constexpr std::int32_t Hash() const { return X ^ Y; }

  constexpr CVec Clamp(Rectangle const& rect_r) const {
    return CVec{std::min(rect_r.Right(), std::max(X, rect_r.Left())),
                std::min(rect_r.Bottom(), std::max(Y, rect_r.Top()))};
  }

  static constexpr std::array<CVec, 8> Directions() {
    return {CVec{-1, -1}, CVec{-1, 0}, CVec{-1, 1}, CVec{0, -1},
            CVec{0, 1},   CVec{1, -1}, CVec{1, 0},  CVec{1, 1}};
  }

  friend constexpr CVec operator+(CVec v_a, CVec v_b) { return CVec{v_a.X + v_b.X, v_a.Y + v_b.Y}; }
  friend constexpr CVec operator-(CVec v_a, CVec v_b) { return CVec{v_a.X - v_b.X, v_a.Y - v_b.Y}; }
  friend constexpr CVec operator*(std::int32_t int4_a, CVec v_b) {
    return CVec{int4_a * v_b.X, int4_a * v_b.Y};
  }
  friend constexpr CVec operator*(CVec v_b, std::int32_t int4_a) { return int4_a * v_b; }
  friend constexpr CVec operator/(CVec v_a, std::int32_t int4_b) {
    return CVec{v_a.X / int4_b, v_a.Y / int4_b};
  }
  friend constexpr CVec operator-(CVec v_a) { return CVec{-v_a.X, -v_a.Y}; }
  friend constexpr bool operator==(CVec v_a, CVec v_b) { return v_a.X == v_b.X && v_a.Y == v_b.Y; }
  friend constexpr bool operator!=(CVec v_a, CVec v_b) { return !(v_a == v_b); }
};

}
