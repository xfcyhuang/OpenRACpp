#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"

namespace ora {

inline std::int32_t WindingDirectionTest(int2 v0, int2 v1, int2 p) {
  const std::int32_t v = (v1.X - v0.X) * (p.Y - v0.Y) -
                         (p.X - v0.X) * (v1.Y - v0.Y);
  return (v > 0) - (v < 0);
}

inline bool PolygonContains(std::span<const int2> polygon, int2 p) {
  std::int32_t winding_number = 0;

  for (std::size_t i = 0; i < polygon.size(); i++) {
    const int2 tv = polygon[i];
    const int2 nv = polygon[(i + 1) % polygon.size()];

    if (tv.Y <= p.Y && nv.Y > p.Y && WindingDirectionTest(tv, nv, p) > 0)
      winding_number++;
    else if (tv.Y > p.Y && nv.Y <= p.Y &&
             WindingDirectionTest(tv, nv, p) < 0)
      winding_number--;
  }

  return winding_number != 0;
}

inline bool LinesIntersect(int2 a, int2 b, int2 c, int2 d) {

  return WindingDirectionTest(c, d, a) != WindingDirectionTest(c, d, b) &&
         WindingDirectionTest(a, b, c) != WindingDirectionTest(a, b, d);
}

struct Polygon {
  Rectangle BoundingRect{Rectangle::FromLTRB(0, 0, 0, 0)};
  std::vector<int2> Vertices{};
  bool is_rectangle_{true};

  static Polygon Empty() { return Polygon{Rectangle::FromLTRB(0, 0, 0, 0)}; }

  explicit Polygon(Rectangle bounds)
      : BoundingRect{bounds},
        is_rectangle_{true} {
    Vertices = {bounds.TopLeft(), bounds.BottomLeft(), bounds.BottomRight(),
                bounds.TopRight()};
  }

  explicit Polygon(std::vector<int2> vertices)
      : Vertices{std::move(vertices)} {
    if (!Vertices.empty()) {
      std::int32_t left = std::numeric_limits<std::int32_t>::max();
      std::int32_t right = std::numeric_limits<std::int32_t>::min();
      std::int32_t top = std::numeric_limits<std::int32_t>::max();
      std::int32_t bottom = std::numeric_limits<std::int32_t>::min();
      for (const int2& p : Vertices) {
        left = std::min(left, p.X);
        right = std::max(right, p.X);
        top = std::min(top, p.Y);
        bottom = std::max(bottom, p.Y);
      }

      BoundingRect = Rectangle::FromLTRB(left, top, right, bottom);
      is_rectangle_ = false;
    } else {
      is_rectangle_ = true;
      BoundingRect = Rectangle::FromLTRB(0, 0, 0, 0);
      Vertices.assign(4, int2{0, 0});
    }
  }

  bool IsEmpty() const { return BoundingRect.IsEmpty(); }

  bool Contains(int2 xy) const {
    return is_rectangle_ ? BoundingRect.Contains(xy)
                         : ora::PolygonContains(Vertices, xy);
  }

  bool IntersectsWith(Rectangle rect) const {
    const bool intersects_bounding_rect =
        BoundingRect.Left() < rect.Right() && BoundingRect.Right() > rect.Left() &&
        BoundingRect.Top() < rect.Bottom() && BoundingRect.Bottom() > rect.Top();
    if (is_rectangle_)
      return intersects_bounding_rect;

    if (!intersects_bounding_rect)
      return false;

    if ((rect.Left() <= BoundingRect.Left() &&
         rect.Right() >= BoundingRect.Right()) ||
        (rect.Top() <= BoundingRect.Top() &&
         rect.Bottom() >= BoundingRect.Bottom()))
      return true;

    if (ora::PolygonContains(Vertices, rect.TopLeft()) ||
        ora::PolygonContains(Vertices, rect.TopRight()) ||
        ora::PolygonContains(Vertices, rect.BottomLeft()) ||
        ora::PolygonContains(Vertices, rect.BottomRight()))
      return true;

    for (const int2& v : Vertices)
      if (rect.Contains(v))
        return true;

    const int2 rect_vertices[4] = {rect.TopLeft(), rect.BottomLeft(),
                                   rect.BottomRight(), rect.TopRight()};

    for (std::size_t i = 0; i < Vertices.size(); i++)
      for (int j = 0; j < 4; j++)
        if (ora::LinesIntersect(Vertices[i], Vertices[(i + 1) % Vertices.size()],
                                rect_vertices[j],
                                rect_vertices[(j + 1) % 4]))
          return true;

    return false;
  }

  std::int32_t Hash() const {
    std::int32_t code = BoundingRect.Hash();
    for (const int2& v : Vertices)
      code = ((code << 5) + code) ^ (v.X ^ v.Y);
    return code;
  }

  friend bool operator==(const Polygon& a, const Polygon& b) {
    return a.BoundingRect == b.BoundingRect && a.Vertices == b.Vertices;
  }
};

}
