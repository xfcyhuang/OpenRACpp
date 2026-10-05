// UPSTREAM: OpenRA.Game/Primitives/int2.cs @b6fc03f L20-94(全类型)
// 二维整数向量。字段/方法名保留 C# 原名(X/Y/Sign/Abs/…)以保持逐行审计对照,
// 2D integer vector. Field/method names keep the C# originals (X/Y/Sign/Abs/...) for line-by-line audit cross-reference;
// 因此公有字段不适用 cpp26.md 的变量前缀规范;局部变量仍按规范带类型前缀。
// therefore public fields are exempt from the cpp26.md variable prefix convention; local variables still carry type prefixes per the convention.
// 上游 FromVector/ToVector2/ToVector3(System.Numerics 浮点)不移植——仅渲染域使用,后续按需。
// Upstream FromVector/ToVector2/ToVector3 (System.Numerics floats) are not ported — rendering domain only, add later as needed.
// Clamp(Rectangle) 因与 Rectangle 相互依赖,实现在 rectangle.hpp。
// Clamp(Rectangle) is mutually dependent with Rectangle, so it is implemented in rectangle.hpp.
#pragma once
import std;

#include "core/exts_math.hpp"  // Length() 用 ISqrt(原先仅靠包含序偶然可见)| Length() needs ISqrt (previously visible only by include order)

namespace ora {

struct Rectangle;  // 前置声明:Clamp 参数为 const 引用,声明处无需完整类型 | Forward declaration: Clamp takes a const reference, so no complete type is needed here

/// 二维整数向量(int2.cs L20)。值语义,全部运算 constexpr。
/// 2D integer vector (int2.cs L20). Value semantics; all operations are constexpr.
struct int2 {
  std::int32_t X{0};  // 横坐标(保留上游字段名) | Horizontal coordinate (upstream field name kept)
  std::int32_t Y{0};  // 纵坐标 | Vertical coordinate

  constexpr int2() = default;
  constexpr int2(std::int32_t int4_x, std::int32_t int4_y) : X{int4_x}, Y{int4_y} {}

  static constexpr int2 Zero() { return int2{0, 0}; }

  /// Math.Sign 的整数化:(v>0)-(v<0)
  /// Integer version of Math.Sign: (v>0)-(v<0)
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

  /// 标量线性插值(int2.cs L77-80):a + (b-a)*mul/div(int 域,回绕语义依赖 -fwrapv)
  /// Scalar linear interpolation (int2.cs L77-80): a + (b-a)*mul/div (int domain; wraparound semantics relies on -fwrapv)
  static constexpr std::int32_t Lerp(std::int32_t int4_a, std::int32_t int4_b,
                                     std::int32_t int4_mul, std::int32_t int4_div) {
    return int4_a + (int4_b - int4_a) * int4_mul / int4_div;
  }

  /// 向量线性插值(int2.cs L82-85)
  /// Vector linear interpolation (int2.cs L82-85)
  static constexpr int2 Lerp(int2 v_a, int2 v_b, std::int32_t int4_mul, std::int32_t int4_div) {
    return v_a + (v_b - v_a) * int4_mul / int4_div;
  }

  static constexpr std::int32_t Dot(int2 v_a, int2 v_b) { return v_a.X * v_b.X + v_a.Y * v_b.Y; }

  /// 翻转 uint32 字节序(int2.cs L72-75)
  /// Reverse the byte order of a uint32 (int2.cs L72-75)
  static constexpr std::uint32_t Swap(std::uint32_t uint4_orig) {
    return ((uint4_orig & 0xff000000u) >> 24) | ((uint4_orig & 0x00ff0000u) >> 8) |
           ((uint4_orig & 0x0000ff00u) << 8) | ((uint4_orig & 0x000000ffu) << 24);
  }

  constexpr int2 Clamp(Rectangle const& rect_r) const;  // 定义在 rectangle.hpp | Defined in rectangle.hpp

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

}  // namespace ora
