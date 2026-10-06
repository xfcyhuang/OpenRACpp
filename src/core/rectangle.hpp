// UPSTREAM: OpenRA.Game/Primitives/Rectangle.cs @b6fc03f L17-129(除 Size 依赖部分)
// 轴对齐整数矩形。字段/方法名保留 C# 原名以保持审计对照。
// Axis-aligned integer rectangle. Field/method names keep the C# originals for audit cross-reference.
// 未移植:Location/Size 属性与 int2,Size 构造(依赖 Size 类型,后续图形阶段补);
// Not ported: Location/Size properties and the int2,Size constructor (depend on the Size type, to be added in the later graphics stage);
//        Clamp(Vector2) 随第十四批 Viewport 落地(浮点,渲染域)。
//        Clamp(Vector2) lands with the fourteenth batch's Viewport (float, rendering domain).
//        int2::Clamp(Rectangle) 的定义在本文件尾部。
//        int2::Clamp(Rectangle) is defined at the end of this file.
#pragma once
import std;

#include "int2.hpp"
#include "vector_n.hpp"

namespace ora {

/// 轴对齐整数矩形(Rectangle.cs L17)
/// Axis-aligned integer rectangle (Rectangle.cs L17)
struct Rectangle {
  std::int32_t X{0};       // 左上角横坐标(保留上游字段名) | Top-left horizontal coordinate (upstream field name kept)
  std::int32_t Y{0};       // 左上角纵坐标 | Top-left vertical coordinate
  std::int32_t Width{0};   // 宽度 | Width
  std::int32_t Height{0};  // 高度 | Height

  constexpr Rectangle() = default;
  constexpr Rectangle(std::int32_t int4_x, std::int32_t int4_y,
                      std::int32_t int4_width, std::int32_t int4_height)
      : X{int4_x}, Y{int4_y}, Width{int4_width}, Height{int4_height} {}

  static constexpr Rectangle Empty() { return Rectangle{}; }

  /// 由左/上/右/下边界构造(Rectangle.cs L28-31)
  /// Construct from left/top/right/bottom edges (Rectangle.cs L28-31)
  static constexpr Rectangle FromLTRB(std::int32_t int4_left, std::int32_t int4_top,
                                      std::int32_t int4_right, std::int32_t int4_bottom) {
    return Rectangle{int4_left, int4_top, int4_right - int4_left, int4_bottom - int4_top};
  }

  /// 两矩形并集包围盒(Rectangle.cs L33-36)
  /// Bounding box of the union of two rectangles (Rectangle.cs L33-36)
  static constexpr Rectangle Union(Rectangle rect_a, Rectangle rect_b) {
    return FromLTRB(std::min(rect_a.Left(), rect_b.Left()), std::min(rect_a.Top(), rect_b.Top()),
                    std::max(rect_a.Right(), rect_b.Right()), std::max(rect_a.Bottom(), rect_b.Bottom()));
  }

  /// 两矩形交集;无相交返回 Empty(Rectangle.cs L118-122)
  /// Intersection of two rectangles; returns Empty when disjoint (Rectangle.cs L118-122)
  static constexpr Rectangle Intersect(Rectangle rect_a, Rectangle rect_b) {
    if (!rect_a.IntersectsWithInclusive(rect_b))
      return Empty();

    return FromLTRB(std::max(rect_a.Left(), rect_b.Left()), std::max(rect_a.Top(), rect_b.Top()),
                    std::min(rect_a.Right(), rect_b.Right()), std::min(rect_a.Bottom(), rect_b.Bottom()));
  }

  constexpr std::int32_t Left() const { return X; }      // 左边界 = X(Rectangle.cs L62) | Left edge = X (Rectangle.cs L62)
  constexpr std::int32_t Right() const { return X + Width; }   // 右边界 = X + Width(L62) | Right edge = X + Width (L62)
  constexpr std::int32_t Top() const { return Y; }       // 上边界 = Y(L63) | Top edge = Y (L63)
  constexpr std::int32_t Bottom() const { return Y + Height; } // 下边界 = Y + Height(L63) | Bottom edge = Y + Height (L63)
  constexpr bool IsEmpty() const { return X == 0 && Y == 0 && Width == 0 && Height == 0; }

  constexpr int2 TopLeft() const { return int2{X, Y}; }
  constexpr int2 TopRight() const { return int2{X + Width, Y}; }
  constexpr int2 BottomLeft() const { return int2{X, Y + Height}; }
  constexpr int2 BottomRight() const { return int2{X + Width, Y + Height}; }

  /// 开区间右下 Contains(Rectangle.cs L74-77):x ∈ [Left, Right), y ∈ [Top, Bottom)
  /// Contains with exclusive Right/Bottom (Rectangle.cs L74-77): x ∈ [Left, Right), y ∈ [Top, Bottom)
  constexpr bool Contains(std::int32_t int4_x, std::int32_t int4_y) const {
    return int4_x >= Left() && int4_x < Right() && int4_y >= Top() && int4_y < Bottom();
  }
  constexpr bool Contains(int2 pt_v) const { return Contains(pt_v.X, pt_v.Y); }
  constexpr bool Contains(Rectangle const& rect_r) const {  // L124-127:rect == Intersect(this, rect)
    return rect_r == Intersect(*this, rect_r);
  }

  /// 开区间相交测试(Rectangle.cs L107-110)
  /// Exclusive-interval intersection test (Rectangle.cs L107-110)
  constexpr bool IntersectsWith(Rectangle const& rect_r) const {
    return Left() < rect_r.Right() && Right() > rect_r.Left() &&
           Top() < rect_r.Bottom() && Bottom() > rect_r.Top();
  }

  /// 闭区间相交测试(上游 private,Intersect 内部用,Rectangle.cs L112-115)
  /// Inclusive-interval intersection test (private upstream, used internally by Intersect, Rectangle.cs L112-115)
  constexpr bool IntersectsWithInclusive(Rectangle const& rect_r) const {
    return Left() <= rect_r.Right() && Right() >= rect_r.Left() &&
           Top() <= rect_r.Bottom() && Bottom() >= rect_r.Top();
  }

  /// Clamp(Vector2)(Rectangle.cs L92-95):Min(Right, Max(v, Left)) 的浮点形态
  /// (Vector2.Clamp 的分量序;边界为整数值转 float)。
  /// Clamp(Vector2) (Rectangle.cs L92-95): the float form of
  /// Min(Right, Max(v, Left)) (component-wise Vector2.Clamp; the edges are
  /// integer values widened to float).
  constexpr core::Vector2 Clamp(core::Vector2 vec_value) const {
    return core::Vector2{std::min(static_cast<float>(Right()), std::max(vec_value.X, static_cast<float>(Left()))),
                         std::min(static_cast<float>(Bottom()), std::max(vec_value.Y, static_cast<float>(Top())))};
  }

  /// 上游 GetHashCode(L102-104):Height + Width ^ X + Y(C# 运算优先级:先 + 后 ^)
  /// Upstream GetHashCode (L102-104): Height + Width ^ X + Y (C# operator precedence: + binds before ^)
  constexpr std::int32_t Hash() const { return Height + Width ^ X + Y; }

  friend constexpr Rectangle operator*(std::int32_t int4_a, Rectangle rect_b) {  // L128
    return Rectangle{int4_a * rect_b.X, int4_a * rect_b.Y, int4_a * rect_b.Width, int4_a * rect_b.Height};
  }
  friend constexpr bool operator==(Rectangle rect_a, Rectangle rect_b) {  // L40-45
    return rect_a.X == rect_b.X && rect_a.Y == rect_b.Y &&
           rect_a.Width == rect_b.Width && rect_a.Height == rect_b.Height;
  }
  friend constexpr bool operator!=(Rectangle rect_a, Rectangle rect_b) { return !(rect_a == rect_b); }
};

/// int2.Clamp(Rectangle)(int2.cs L87-91):Min(Right, Max(v, Left))
/// int2.Clamp(Rectangle) (int2.cs L87-91): Min(Right, Max(v, Left))
constexpr int2 int2::Clamp(Rectangle const& rect_r) const {
  return int2{std::min(rect_r.Right(), std::max(X, rect_r.Left())),
              std::min(rect_r.Bottom(), std::max(Y, rect_r.Top()))};
}

}  // namespace ora
