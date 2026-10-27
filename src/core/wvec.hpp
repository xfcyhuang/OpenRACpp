#pragma once
import std;

#include "exts_math.hpp"
#include "int32_matrix4x4.hpp"
#include "mersenne_twister.hpp"
#include "wdist.hpp"
#include "wangle.hpp"

namespace ora {

struct WRot;
struct WPos;

struct WVec {
  std::int32_t X{0};
  std::int32_t Y{0};
  std::int32_t Z{0};

  constexpr WVec() = default;
  constexpr WVec(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z)
      : X{int4_x}, Y{int4_y}, Z{int4_z} {}
  explicit WVec(WPos const& pos_p);
  constexpr WVec(WDist d_x, WDist d_y, WDist d_z)
      : X{d_x.Length}, Y{d_y.Length}, Z{d_z.Length} {}

  static constexpr WVec Zero() { return WVec{0, 0, 0}; }

  constexpr std::int64_t LengthSquared() const {
    return static_cast<std::int64_t>(X) * X + static_cast<std::int64_t>(Y) * Y +
           static_cast<std::int64_t>(Z) * Z;
  }
  constexpr std::int32_t Length() const {
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

  constexpr WAngle Yaw() const {
    if (LengthSquared() == 0)
      return WAngle{0};

    return WAngle::ArcTan(-Y, X) - WAngle{256};
  }

  WVec Rotate(WRot const& rot_r) const;

  constexpr WVec Rotate(Int32Matrix4x4 const& mtx_m) const {
    const std::int64_t int8_lx{X};
    const std::int64_t int8_ly{Y};
    const std::int64_t int8_lz{Z};
    return WVec{
        static_cast<std::int32_t>((int8_lx * mtx_m.M11 + int8_ly * mtx_m.M21 + int8_lz * mtx_m.M31) / mtx_m.M44),
        static_cast<std::int32_t>((int8_lx * mtx_m.M12 + int8_ly * mtx_m.M22 + int8_lz * mtx_m.M32) / mtx_m.M44),
        static_cast<std::int32_t>((int8_lx * mtx_m.M13 + int8_ly * mtx_m.M23 + int8_lz * mtx_m.M33) / mtx_m.M44)};
  }

  static constexpr WVec Lerp(WVec v_a, WVec v_b, std::int32_t int4_mul, std::int32_t int4_div) {
    return v_a + (v_b - v_a) * int4_mul / int4_div;
  }

  static constexpr WVec LerpQuadratic(WVec v_a, WVec v_b, WAngle pitch_ang,
                                      std::int32_t int4_mul, std::int32_t int4_div) {
    const WVec vec_ret{Lerp(v_a, v_b, int4_mul, int4_div)};
    if (pitch_ang.Angle == 0)
      return vec_ret;

    const std::int32_t int4_len{(v_b - v_a).Length()};
    const std::int32_t int4_tan{pitch_ang.Tan()};

    const __int128 int16_num{static_cast<__int128>(int4_len) * int4_tan * int4_mul * (int4_div - int4_mul)};
    const __int128 int16_den{static_cast<__int128>(1024 * int4_div * int4_div)};
    const std::int32_t int4_offset{static_cast<std::int32_t>(int16_num / int16_den)};

    return WVec{vec_ret.X, vec_ret.Y, vec_ret.Z + int4_offset};
  }

  static constexpr WVec FromPDF(MersenneTwister& rng_r, std::int32_t int4_samples) {
    return WVec{WDist::FromPDF(rng_r, int4_samples), WDist::FromPDF(rng_r, int4_samples),
                WDist{0}};
  }

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

}
