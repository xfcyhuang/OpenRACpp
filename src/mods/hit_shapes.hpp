// UPSTREAM: OpenRA.Mods.Common/HitShapes/IHitShape.cs @b6fc03f L17-28 +
//          Circle.cs L17-67 + Capsule.cs L17-121 + Polygon.cs L21-130 +
//          Rectangle.cs L20-146(逐语义重写;RenderDebugOverlay 渲染面随
//          Phase 6)
//          IHitShape.cs L17-28 + Circle.cs L17-67 + Capsule.cs L17-121 +
//          Polygon.cs L21-130 + Rectangle.cs L20-146 (verbatim-semantics
//          rewrites; the RenderDebugOverlay render face lands with Phase
//          6).
//
// 机制对照 / Mechanism mapping:
//  - 上游 Game.CreateObject<IHitShape>(shape + "Shape") + FieldLoader.Load
//    → 值袋嵌套记录(loaders.cpp 的 LoadShape 已物化)+ 本文件的按名解析
//    (Initialize() 在解析尾调用 —— 上游 LoadShape L157 同点)
//    Upstream's Game.CreateObject<IHitShape>(shape + "Shape") +
//    FieldLoader.Load → the value-bag nested record (materialized by
//    loaders.cpp's LoadShape) + the by-name parse here (Initialize() runs
//    at the parse tail — the same point as upstream's LoadShape L157).
//  - Polygon 的 Points.PolygonContains(Exts)→ core/polygon.hpp 的同面
//    Polygon's Points.PolygonContains (Exts) → the same face in
//    core/polygon.hpp.
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

/// IHitShape.cs L17-28(RenderDebugOverlay 渲染面随 Phase 6)
/// IHitShape.cs L17-28 (the RenderDebugOverlay render face lands with
/// Phase 6).
class IHitShape {
 public:
  virtual ~IHitShape() = default;
  virtual WDist OuterRadius() const = 0;
  virtual WDist DistanceFromEdge(const WVec& v) const = 0;
  virtual WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                                 const WRot& orientation) const = 0;
  virtual void Initialize() = 0;
};

/// Circle.cs L17-67
class CircleShape final : public IHitShape {
 public:
  CircleShape() = default;
  explicit CircleShape(WDist radius)
      : Radius{radius} {}

  WDist Radius{426};              // L24([FieldLoader.Require])
  int VerticalTopOffset = 0;      // L27
  int VerticalBottomOffset = 0;   // L30

  WDist OuterRadius() const override { return Radius; }  // L22

  void Initialize() override;     // L38-42

  WDist DistanceFromEdge(const WVec& v) const override;  // L44-46
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;  // L48-60
};

/// Capsule.cs L17-121
class CapsuleShape final : public IHitShape {
 public:
  CapsuleShape() = default;
  CapsuleShape(int2 a, int2 b, WDist radius)
      : PointA{a}, PointB{b}, Radius{radius} {}

  int2 PointA;                    // L26([FieldLoader.Require])
  int2 PointB;                    // L29([FieldLoader.Require])
  WDist Radius{426};              // L31
  int VerticalTopOffset = 0;      // L34
  int VerticalBottomOffset = 0;   // L37

  WDist OuterRadius() const override { return outer_radius_; }  // L23

  void Initialize() override;     // L51-63

  WDist DistanceFromEdge(const WVec& v) const override;  // L65-83
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;  // L85-94

 private:
  int2 ab_;            // L39
  int ab_len_sq_ = 0;  // L40
  WDist outer_radius_;
};

/// Polygon.cs L21-130
class PolygonShape final : public IHitShape {
 public:
  PolygonShape() = default;
  explicit PolygonShape(std::vector<int2> points)
      : Points{std::move(points)} {}

  std::vector<int2> Points;       // L27([FieldLoader.Require])
  int VerticalTopOffset = 0;      // L31
  int VerticalBottomOffset = 0;   // L34
  WAngle LocalYaw{0};             // L38

  WDist OuterRadius() const override { return outer_radius_; }  // L24

  void Initialize() override;     // L48-60

  WDist DistanceFromEdge(const WVec& v) const override;  // L86-102
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;  // L104-115

 private:
  /// L62-84:DistanceSquaredFromLineSegment(c 到线段 ab 的平方距离;ab2 =
  /// |ab|²)
  static int DistanceSquaredFromLineSegment(int2 c, int2 a, int2 b, int ab2);

  WDist outer_radius_;
  std::vector<WVec> vec_combat_overlay_verts_top_;     // L40
  std::vector<WVec> vec_combat_overlay_verts_bottom_;  // L41
  std::vector<int> vec_squares_;                       // L42
};

/// Rectangle.cs L20-146
class RectangleShape final : public IHitShape {
 public:
  RectangleShape() = default;
  RectangleShape(int2 tl, int2 br)
      : TopLeft{tl}, BottomRight{br} {}

  int2 TopLeft;                   // L27([FieldLoader.Require])
  int2 BottomRight;               // L30([FieldLoader.Require])
  int VerticalTopOffset = 0;      // L33
  int VerticalBottomOffset = 0;   // L36
  WAngle LocalYaw{0};             // L40

  WDist OuterRadius() const override { return outer_radius_; }  // L23

  void Initialize() override;     // L58-105

  WDist DistanceFromEdge(const WVec& v) const override;  // L107-114
  WDist DistanceFromEdge(const WPos& pos, const WPos& origin,
                         const WRot& orientation) const override;  // L116-127

 private:
  int2 quadrant_size_;  // L42
  int2 center_;         // L43
  WDist outer_radius_;
};

/// LoadShape 产物解析:嵌套记录 → 具体形状(Initialize() 已调用;未注册
/// 记录名 → nullptr,上游 CreateObject null 路径)
/// The LoadShape product parse: the nested record → the concrete shape
/// (Initialize() already run; an unregistered record name → nullptr,
/// upstream's CreateObject null path).
std::unique_ptr<IHitShape> ParseHitShape(const meta::RecordObject& rec_type);

}  // namespace ora::mods
