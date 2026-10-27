#pragma once
import std;

#include "int2.hpp"
#include "vector_n.hpp"

namespace ora {

struct Rectangle {
  std::int32_t X{0};
  std::int32_t Y{0};
  std::int32_t Width{0};
  std::int32_t Height{0};

  constexpr Rectangle() = default;
  constexpr Rectangle(std::int32_t int4_x, std::int32_t int4_y,
                      std::int32_t int4_width, std::int32_t int4_height)
      : X{int4_x}, Y{int4_y}, Width{int4_width}, Height{int4_height} {}

  static constexpr Rectangle Empty() { return Rectangle{}; }

  static constexpr Rectangle FromLTRB(std::int32_t int4_left, std::int32_t int4_top,
                                      std::int32_t int4_right, std::int32_t int4_bottom) {
    return Rectangle{int4_left, int4_top, int4_right - int4_left, int4_bottom - int4_top};
  }

  static constexpr Rectangle Union(Rectangle rect_a, Rectangle rect_b) {
    return FromLTRB(std::min(rect_a.Left(), rect_b.Left()), std::min(rect_a.Top(), rect_b.Top()),
                    std::max(rect_a.Right(), rect_b.Right()), std::max(rect_a.Bottom(), rect_b.Bottom()));
  }

  static constexpr Rectangle Intersect(Rectangle rect_a, Rectangle rect_b) {
    if (!rect_a.IntersectsWithInclusive(rect_b))
      return Empty();

    return FromLTRB(std::max(rect_a.Left(), rect_b.Left()), std::max(rect_a.Top(), rect_b.Top()),
                    std::min(rect_a.Right(), rect_b.Right()), std::min(rect_a.Bottom(), rect_b.Bottom()));
  }

  constexpr std::int32_t Left() const { return X; }
  constexpr std::int32_t Right() const { return X + Width; }
  constexpr std::int32_t Top() const { return Y; }
  constexpr std::int32_t Bottom() const { return Y + Height; }
  constexpr bool IsEmpty() const { return X == 0 && Y == 0 && Width == 0 && Height == 0; }

  constexpr int2 TopLeft() const { return int2{X, Y}; }
  constexpr int2 TopRight() const { return int2{X + Width, Y}; }
  constexpr int2 BottomLeft() const { return int2{X, Y + Height}; }
  constexpr int2 BottomRight() const { return int2{X + Width, Y + Height}; }

  constexpr bool Contains(std::int32_t int4_x, std::int32_t int4_y) const {
    return int4_x >= Left() && int4_x < Right() && int4_y >= Top() && int4_y < Bottom();
  }
  constexpr bool Contains(int2 pt_v) const { return Contains(pt_v.X, pt_v.Y); }
  constexpr bool Contains(Rectangle const& rect_r) const {
    return rect_r == Intersect(*this, rect_r);
  }

  constexpr bool IntersectsWith(Rectangle const& rect_r) const {
    return Left() < rect_r.Right() && Right() > rect_r.Left() &&
           Top() < rect_r.Bottom() && Bottom() > rect_r.Top();
  }

  constexpr bool IntersectsWithInclusive(Rectangle const& rect_r) const {
    return Left() <= rect_r.Right() && Right() >= rect_r.Left() &&
           Top() <= rect_r.Bottom() && Bottom() >= rect_r.Top();
  }

  constexpr core::Vector2 Clamp(core::Vector2 vec_value) const {
    return core::Vector2{std::min(static_cast<float>(Right()), std::max(vec_value.X, static_cast<float>(Left()))),
                         std::min(static_cast<float>(Bottom()), std::max(vec_value.Y, static_cast<float>(Top())))};
  }

  constexpr std::int32_t Hash() const { return Height + Width ^ X + Y; }

  friend constexpr Rectangle operator*(std::int32_t int4_a, Rectangle rect_b) {
    return Rectangle{int4_a * rect_b.X, int4_a * rect_b.Y, int4_a * rect_b.Width, int4_a * rect_b.Height};
  }
  friend constexpr bool operator==(Rectangle rect_a, Rectangle rect_b) {
    return rect_a.X == rect_b.X && rect_a.Y == rect_b.Y &&
           rect_a.Width == rect_b.Width && rect_a.Height == rect_b.Height;
  }
  friend constexpr bool operator!=(Rectangle rect_a, Rectangle rect_b) { return !(rect_a == rect_b); }
};

constexpr int2 int2::Clamp(Rectangle const& rect_r) const {
  return int2{std::min(rect_r.Right(), std::max(X, rect_r.Left())),
              std::min(rect_r.Bottom(), std::max(Y, rect_r.Top()))};
}

}
