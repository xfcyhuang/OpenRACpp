// UPSTREAM: OpenRA.Game/WVec.cs @b6fc03f L20-109(除 Lua 脚本绑定接口)
// 三维世界向量:全整数定点,1024 单位 = 1 cell。
// LerpQuadratic 上游用 decimal(96 位十进制整数)防溢出;此处以 __int128 精确整数除法
// 复刻,商向零截断与 decimal 一致——理论差异仅当商的小数展开连续 12+ 个 9 并触发
// decimal 第 28 位进位时出现(golden 对拍覆盖)。
// Rotate(WRot) 因与 WRot 相互依赖,声明于此、定义在 wrot.hpp。
// 3D world vector: all-integer fixed-point, 1024 units = 1 cell.
// Upstream LerpQuadratic guards against overflow with decimal (96-bit decimal integer); here it is
// reproduced with exact __int128 integer division — the quotient truncates toward zero exactly as decimal
// does; the only theoretical divergence appears when the quotient's fractional expansion has 12+ consecutive
// 9s that trigger a carry into decimal's 28th digit (covered by golden differential testing).
// Rotate(WRot) and WRot are mutually dependent, so it is declared here and defined in wrot.hpp.
#pragma once
import std;

#include "exts_math.hpp"
#include "int32_matrix4x4.hpp"
#include "mersenne_twister.hpp"
#include "wdist.hpp"
#include "wangle.hpp"

namespace ora {

struct WRot;  // 前置声明:Rotate(WRot) 参数为 const 引用 | Forward declaration: Rotate(WRot) takes its parameter by const reference
struct WPos;  // 前置声明:显式转换构造的参数 | Forward declaration: parameter of the explicit conversion constructor

/// 三维世界向量(WVec.cs L20)
/// 3D world vector (WVec.cs L20)
struct WVec {
  std::int32_t X{0};  // 定点横坐标,1024 == 1 cell(保留上游字段名) | Fixed-point X coordinate, 1024 == 1 cell (upstream field name preserved)
  std::int32_t Y{0};  // 定点纵坐标 | Fixed-point Y coordinate
  std::int32_t Z{0};  // 定点高度 | Fixed-point height (Z)

  constexpr WVec() = default;
  constexpr WVec(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z)
      : X{int4_x}, Y{int4_y}, Z{int4_z} {}
  explicit WVec(WPos const& pos_p);  // 显式转换(WVec.cs 无此构造,对应 explicit operator,定义在 wpos.hpp) | Explicit conversion (no such constructor in WVec.cs; mirrors the explicit operator, defined in wpos.hpp)
  constexpr WVec(WDist d_x, WDist d_y, WDist d_z)  // (WDist.cs L27)取 Length 分量 | (WDist.cs L27) takes the Length components
      : X{d_x.Length}, Y{d_y.Length}, Z{d_z.Length} {}

  static constexpr WVec Zero() { return WVec{0, 0, 0}; }

  constexpr std::int64_t LengthSquared() const {  // long 中间量防 int 平方溢出 | long intermediates prevent int square overflow
    return static_cast<std::int64_t>(X) * X + static_cast<std::int64_t>(Y) * Y +
           static_cast<std::int64_t>(Z) * Z;
  }
  constexpr std::int32_t Length() const {  // (int)ISqrt:极端值截断语义照抄上游 | (int)ISqrt: extreme-value truncation semantics copied verbatim from upstream
    return static_cast<std::int32_t>(ISqrt(LengthSquared()));
  }
  constexpr std::int64_t HorizontalLengthSquared() const {
    return static_cast<std::int64_t>(X) * X + static_cast<std::int64_t>(Y) * Y;
  }
  constexpr std::int32_t HorizontalLength() const {
    return static_cast<std::int32_t>(ISqrt(HorizontalLengthSquared()));
  }
  constexpr std::int64_t VerticalLengthSquared() const {
    return static_cast<std::int64_t>(Z) * Z;
  }
  constexpr std::int32_t VerticalLength() const {
    return static_cast<std::int32_t>(ISqrt(VerticalLengthSquared()));
  }

  static constexpr std::int32_t Dot(WVec v_a, WVec v_b) {
    return v_a.X * v_b.X + v_a.Y * v_b.Y + v_a.Z * v_b.Z;
  }

  /// 朝向角(WVec.cs L66-76):OpenRA 定义北为 -Y
  /// Yaw angle (WVec.cs L66-76): OpenRA defines north as -Y
  constexpr WAngle Yaw() const {
    if (LengthSquared() == 0)
      return WAngle{0};

    return WAngle::ArcTan(-Y, X) - WAngle{256};
  }

  WVec Rotate(WRot const& rot_r) const;  // 定义在 wrot.hpp(依赖 WRot 完整类型) | Defined in wrot.hpp (needs the complete WRot type)

  /// 定点矩阵旋转(WVec.cs L55-64):long 中间量,整除 M44(1024 == 1.0)
  /// Fixed-point matrix rotation (WVec.cs L55-64): long intermediates, integer division by M44 (1024 == 1.0)
  constexpr WVec Rotate(Int32Matrix4x4 const& mtx_m) const {
    const std::int64_t int8_lx{X};
    const std::int64_t int8_ly{Y};
    const std::int64_t int8_lz{Z};
    return WVec{
        static_cast<std::int32_t>((int8_lx * mtx_m.M11 + int8_ly * mtx_m.M21 + int8_lz * mtx_m.M31) / mtx_m.M44),
        static_cast<std::int32_t>((int8_lx * mtx_m.M12 + int8_ly * mtx_m.M22 + int8_lz * mtx_m.M32) / mtx_m.M44),
        static_cast<std::int32_t>((int8_lx * mtx_m.M13 + int8_ly * mtx_m.M23 + int8_lz * mtx_m.M33) / mtx_m.M44)};
  }

  /// 线性插值(WVec.cs L78)
  /// Linear interpolation (WVec.cs L78)
  static constexpr WVec Lerp(WVec v_a, WVec v_b, std::int32_t int4_mul, std::int32_t int4_div) {
    return v_a + (v_b - v_a) * int4_mul / int4_div;
  }

  /// 抛物线插值(WVec.cs L80-92):offset 先截断为 int 再加到 Z(ret.Z + offset 为 int 域回绕加法)。
  /// 分母 1024*div*div 在上游是 int 域乘法(可回绕),须回绕后再进 __int128
  /// Quadratic interpolation (WVec.cs L80-92): offset is truncated to int first, then added to Z (ret.Z + offset is an int-domain wraparound addition).
  /// The denominator 1024*div*div is an int-domain multiplication upstream (may wrap); it must wrap before entering __int128
  static constexpr WVec LerpQuadratic(WVec v_a, WVec v_b, WAngle pitch_ang,
                                      std::int32_t int4_mul, std::int32_t int4_div) {
    const WVec vec_ret{Lerp(v_a, v_b, int4_mul, int4_div)};
    if (pitch_ang.Angle == 0)
      return vec_ret;

    const std::int32_t int4_len{(v_b - v_a).Length()};
    const std::int32_t int4_tan{pitch_ang.Tan()};

    const __int128 int16_num{static_cast<__int128>(int4_len) * int4_tan * int4_mul * (int4_div - int4_mul)};
    const __int128 int16_den{static_cast<__int128>(1024 * int4_div * int4_div)};  // int 域回绕后转 128 位 | int-domain wraparound applied before widening to 128 bits
    const std::int32_t int4_offset{static_cast<std::int32_t>(int16_num / int16_den)};  // 向零截断 | Truncates toward zero

    return WVec{vec_ret.X, vec_ret.Y, vec_ret.Z + int4_offset};
  }

  /// N 样本概率密度采样(WVec.cs L99-102):X/Y 各做一次 WDist::FromPDF,Z 恒 0
  /// N-sample probability density sampling (WVec.cs L99-102): one WDist::FromPDF each for X/Y, Z always 0
  static constexpr WVec FromPDF(MersenneTwister& rng_r, std::int32_t int4_samples) {
    return WVec{WDist::FromPDF(rng_r, int4_samples), WDist::FromPDF(rng_r, int4_samples),
                WDist{0}};
  }

  /// 上游 GetHashCode(WVec.cs L104)
  /// Upstream GetHashCode (WVec.cs L104)
  constexpr std::int32_t Hash() const { return X ^ Y ^ Z; }

  friend constexpr WVec operator+(WVec v_a, WVec v_b) {
    return WVec{v_a.X + v_b.X, v_a.Y + v_b.Y, v_a.Z + v_b.Z};
  }
  friend constexpr WVec operator-(WVec v_a, WVec v_b) {
    return WVec{v_a.X - v_b.X, v_a.Y - v_b.Y, v_a.Z - v_b.Z};
  }
  friend constexpr WVec operator-(WVec v_a) { return WVec{-v_a.X, -v_a.Y, -v_a.Z}; }
  friend constexpr WVec operator/(WVec v_a, std::int32_t int4_b) {
    return WVec{v_a.X / int4_b, v_a.Y / int4_b, v_a.Z / int4_b};
  }
  friend constexpr WVec operator*(std::int32_t int4_a, WVec v_b) {
    return WVec{int4_a * v_b.X, int4_a * v_b.Y, int4_a * v_b.Z};
  }
  friend constexpr WVec operator*(WVec v_a, std::int32_t int4_b) { return int4_b * v_a; }
  friend constexpr bool operator==(WVec v_a, WVec v_b) {
    return v_a.X == v_b.X && v_a.Y == v_b.Y && v_a.Z == v_b.Z;
  }
  friend constexpr bool operator!=(WVec v_a, WVec v_b) { return !(v_a == v_b); }
};

}  // namespace ora
