#pragma once
import std;

#include "core/int2.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

class IHitShape {
 public:
  virtual ~IHitShape() = default;
  virtual WDist OuterRadius() const = 0;
  virtual WDist DistanceFromEdge(const WVec& v) const = 0;
  virtual WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                                 const WRot& orientation) const = 0;
  virtual void Initialize() = 0;
};

class CircleShape final : public IHitShape {
 public:
  CircleShape() = default;
  explicit CircleShape(WDist radius)
      : Radius{radius} {}

  WDist Radius{426};
  int VerticalTopOffset = 0;
  int VerticalBottomOffset = 0;

  WDist OuterRadius() const override { return Radius; }

  void Initialize() override;

  WDist DistanceFromEdge(const WVec& v) const override;
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;
};

class CapsuleShape final : public IHitShape {
 public:
  CapsuleShape() = default;
  CapsuleShape(int2 a, int2 b, WDist radius)
      : PointA{a}, PointB{b}, Radius{radius} {}

  int2 PointA;
  int2 PointB;
  WDist Radius{426};
  int VerticalTopOffset = 0;
  int VerticalBottomOffset = 0;

  WDist OuterRadius() const override { return outer_radius_; }

  void Initialize() override;

  WDist DistanceFromEdge(const WVec& v) const override;
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;

 private:
  int2 ab_;
  int ab_len_sq_ = 0;
  WDist outer_radius_;
};

class PolygonShape final : public IHitShape {
 public:
  PolygonShape() = default;
  explicit PolygonShape(std::vector<int2> points)
      : Points{std::move(points)} {}

  std::vector<int2> Points;
  int VerticalTopOffset = 0;
  int VerticalBottomOffset = 0;
  WAngle LocalYaw{0};

  WDist OuterRadius() const override { return outer_radius_; }

  void Initialize() override;

  WDist DistanceFromEdge(const WVec& v) const override;
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;

 private:

  static int DistanceSquaredFromLineSegment(int2 c, int2 a, int2 b, int ab2);

  WDist outer_radius_;
  std::vector<WVec> vec_combat_overlay_verts_top_;
  std::vector<WVec> vec_combat_overlay_verts_bottom_;
  std::vector<int> vec_squares_;
};

class RectangleShape final : public IHitShape {
 public:
  RectangleShape() = default;
  RectangleShape(int2 tl, int2 br)
      : TopLeft{tl}, BottomRight{br} {}

  int2 TopLeft;
  int2 BottomRight;
  int VerticalTopOffset = 0;
  int VerticalBottomOffset = 0;
  WAngle LocalYaw{0};

  WDist OuterRadius() const override { return outer_radius_; }

  void Initialize() override;

  WDist DistanceFromEdge(const WVec& v) const override;
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;

 private:
  int2 quadrant_size_;
  int2 center_;
  WDist outer_radius_;
};

std::unique_ptr<IHitShape> ParseHitShape(const meta::RecordObject& rec_type);

}
