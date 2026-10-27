#pragma once
import std;

#include "wdist.hpp"
#include "wvec.hpp"

namespace ora {

struct WPos {
  std::int32_t X{0};
  std::int32_t Y{0};
  std::int32_t Z{0};

  constexpr WPos() = default;
  constexpr WPos(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z)
      : X{int4_x}, Y{int4_y}, Z{int4_z} {}
  constexpr WPos(WDist d_x, WDist d_y, WDist d_z)
      : X{d_x.Length}, Y{d_y.Length}, Z{d_z.Length} {}

  static constexpr WPos Zero() { return WPos{0, 0, 0}; }

  constexpr explicit operator WVec() const { return WVec{X, Y, Z}; }

  static constexpr WPos Lerp(WPos p_a, WPos p_b, std::int32_t int4_mul, std::int32_t int4_div) {
    return p_a + (p_b - p_a) * int4_mul / int4_div;
  }

  static constexpr WPos Lerp(WPos p_a, WPos p_b, std::int64_t int8_mul, std::int64_t int8_div) {
    const std::int32_t int4_x{
        static_cast<std::int32_t>(p_a.X + (static_cast<std::int64_t>(p_b.X) - p_a.X) * int8_mul / int8_div)};
    const std::int32_t int4_y{
        static_cast<std::int32_t>(p_a.Y + (static_cast<std::int64_t>(p_b.Y) - p_a.Y) * int8_mul / int8_div)};
    const std::int32_t int4_z{
        static_cast<std::int32_t>(p_a.Z + (static_cast<std::int64_t>(p_b.Z) - p_a.Z) * int8_mul / int8_div)};
    return WPos{int4_x, int4_y, int4_z};
  }

  static constexpr WPos LerpQuadratic(WPos p_a, WPos p_b, WAngle pitch_ang,
                                      std::int32_t int4_mul, std::int32_t int4_div) {
    const WPos pos_ret{Lerp(p_a, p_b, int4_mul, int4_div)};
    if (pitch_ang.Angle == 0)
      return pos_ret;

    const std::int32_t int4_len{(p_b - p_a).Length()};
    const std::int32_t int4_tan{pitch_ang.Tan()};

    const __int128 int16_num{static_cast<__int128>(int4_len) * int4_tan * int4_mul * (int4_div - int4_mul)};
    const __int128 int16_den{static_cast<__int128>(1024 * int4_div * int4_div)};

    const __int128 int16_q{(int16_num + static_cast<__int128>(pos_ret.Z) * int16_den) / int16_den};
    const std::int32_t int4_clamped{
        int16_q < std::numeric_limits<std::int32_t>::min()
            ? std::numeric_limits<std::int32_t>::min()
            : (int16_q > std::numeric_limits<std::int32_t>::max()
                   ? std::numeric_limits<std::int32_t>::max()
                   : static_cast<std::int32_t>(int16_q))};

    return WPos{pos_ret.X, pos_ret.Y, int4_clamped};
  }

  constexpr std::int32_t Hash() const { return X ^ Y ^ Z; }

  friend constexpr WPos operator+(WPos p_a, WVec v_b) {
    return WPos{p_a.X + v_b.X, p_a.Y + v_b.Y, p_a.Z + v_b.Z};
  }
  friend constexpr WPos operator-(WPos p_a, WVec v_b) {
    return WPos{p_a.X - v_b.X, p_a.Y - v_b.Y, p_a.Z - v_b.Z};
  }
  friend constexpr WVec operator-(WPos p_a, WPos p_b) {
    return WVec{p_a.X - p_b.X, p_a.Y - p_b.Y, p_a.Z - p_b.Z};
  }
  friend constexpr bool operator==(WPos p_a, WPos p_b) {
    return p_a.X == p_b.X && p_a.Y == p_b.Y && p_a.Z == p_b.Z;
  }
  friend constexpr bool operator!=(WPos p_a, WPos p_b) { return !(p_a == p_b); }
};

inline WVec::WVec(WPos const& pos_p) : X{pos_p.X}, Y{pos_p.Y}, Z{pos_p.Z} {}

inline WPos Average(std::span<const WPos> span_src) {
  std::int32_t int4_length{0};
  std::int64_t int8_x{0};
  std::int64_t int8_y{0};
  std::int64_t int8_z{0};
  for (const WPos& pos_p : span_src) {
    int4_length++;
    int8_x += pos_p.X;
    int8_y += pos_p.Y;
    int8_z += pos_p.Z;
  }

  if (int4_length == 0)
    return WPos::Zero();

  int8_x /= int4_length;
  int8_y /= int4_length;
  int8_z /= int4_length;

  return WPos{static_cast<std::int32_t>(int8_x), static_cast<std::int32_t>(int8_y),
              static_cast<std::int32_t>(int8_z)};
}

}
