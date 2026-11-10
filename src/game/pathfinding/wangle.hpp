#pragma once

import std;

namespace ora::game::pathfinding {

struct WPos {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_z{};
};

struct WVec {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_z{};
};

struct WRot {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_z{};
  std::int32_t int4_w{};
};

struct RotMatrix {
  std::int32_t vec4_m[16]{};
};

constexpr std::int32_t wangle_wrap(std::int32_t int4_angle) { return int4_angle & 1023; }

[[nodiscard]] std::int32_t wangle_lerp(std::int32_t int4_a, std::int32_t int4_b,
                                       std::int32_t int4_mul, std::int32_t int4_div);
[[nodiscard]] std::int32_t wangle_cos(std::int32_t int4_angle);
[[nodiscard]] std::int32_t wangle_sin(std::int32_t int4_angle);
[[nodiscard]] std::int32_t wangle_tan(std::int32_t int4_angle);
[[nodiscard]] std::int32_t wangle_arctan(std::int32_t int4_y, std::int32_t int4_x);
[[nodiscard]] std::int32_t tick_facing(std::int32_t int4_facing, std::int32_t int4_desired,
                                       std::int32_t int4_step);
[[nodiscard]] std::int64_t isqrt64(std::int64_t int8_number);

[[nodiscard]] WVec wvec_sub(WPos pos_a, WPos pos_b);
[[nodiscard]] std::int64_t wvec_length_squared(WVec vec);
[[nodiscard]] std::int64_t wvec_horizontal_length_squared(WVec vec);
[[nodiscard]] std::int32_t wvec_horizontal_length(WVec vec);
[[nodiscard]] std::int32_t wvec_yaw(WVec vec);
[[nodiscard]] WPos wpos_lerp(WPos pos_a, WPos pos_b, std::int32_t int4_mul, std::int32_t int4_div);
[[nodiscard]] std::int32_t int2_lerp(std::int32_t int4_a, std::int32_t int4_b, std::int32_t int4_mul,
                                     std::int32_t int4_div);
[[nodiscard]] WRot wrot_from_yaw(std::int32_t int4_yaw);
[[nodiscard]] RotMatrix wrot_as_matrix(WRot rot);
[[nodiscard]] WVec wvec_rotate_matrix(WVec vec, const RotMatrix& mtx);
[[nodiscard]] WVec wvec_rotate_yaw(WVec vec, std::int32_t int4_yaw);

}
