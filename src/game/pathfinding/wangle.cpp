#include "game/pathfinding/wangle.hpp"

namespace ora::game::pathfinding {

namespace {

constexpr std::int16_t kCosineTable[257] = {
    1024, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1022, 1022, 1022, 1021,
    1021, 1020, 1020, 1019, 1019, 1018, 1017, 1017, 1016, 1015, 1014, 1013,
    1012, 1011, 1010, 1009, 1008, 1007, 1006, 1005, 1004, 1003, 1001, 1000,
    999, 997, 996, 994, 993, 991, 990, 988, 986, 985, 983, 981,
    979, 978, 976, 974, 972, 970, 968, 966, 964, 962, 959, 957,
    955, 953, 950, 948, 946, 943, 941, 938, 936, 933, 930, 928,
    925, 922, 920, 917, 914, 911, 908, 906, 903, 900, 897, 894,
    890, 887, 884, 881, 878, 875, 871, 868, 865, 861, 858, 854,
    851, 847, 844, 840, 837, 833, 829, 826, 822, 818, 814, 811,
    807, 803, 799, 795, 791, 787, 783, 779, 775, 771, 767, 762,
    758, 754, 750, 745, 741, 737, 732, 728, 724, 719, 715, 710,
    706, 701, 696, 692, 687, 683, 678, 673, 668, 664, 659, 654,
    649, 644, 639, 634, 629, 625, 620, 615, 609, 604, 599, 594,
    589, 584, 579, 574, 568, 563, 558, 553, 547, 542, 537, 531,
    526, 521, 515, 510, 504, 499, 493, 488, 482, 477, 471, 466,
    460, 454, 449, 443, 437, 432, 426, 420, 414, 409, 403, 397,
    391, 386, 380, 374, 368, 362, 356, 350, 344, 339, 333, 327,
    321, 315, 309, 303, 297, 291, 285, 279, 273, 267, 260, 254,
    248, 242, 236, 230, 224, 218, 212, 205, 199, 193, 187, 181,
    175, 168, 162, 156, 150, 144, 137, 131, 125, 119, 112, 106,
    100, 94, 87, 81, 75, 69, 62, 56, 50, 43, 37, 31,
    25, 18, 12, 6, 0,
};

constexpr std::int32_t kTanTable[257] = {
    0, 6, 12, 18, 25, 31, 37, 44, 50, 56, 62, 69,
    75, 81, 88, 94, 100, 107, 113, 119, 126, 132, 139, 145,
    151, 158, 164, 171, 177, 184, 190, 197, 203, 210, 216, 223,
    229, 236, 243, 249, 256, 263, 269, 276, 283, 290, 296, 303,
    310, 317, 324, 331, 338, 345, 352, 359, 366, 373, 380, 387,
    395, 402, 409, 416, 424, 431, 438, 446, 453, 461, 469, 476,
    484, 492, 499, 507, 515, 523, 531, 539, 547, 555, 563, 571,
    580, 588, 596, 605, 613, 622, 630, 639, 648, 657, 666, 675,
    684, 693, 702, 711, 721, 730, 740, 749, 759, 769, 779, 789,
    799, 809, 819, 829, 840, 850, 861, 872, 883, 894, 905, 916,
    928, 939, 951, 963, 974, 986, 999, 1011, 1023, 1036, 1049, 1062,
    1075, 1088, 1102, 1115, 1129, 1143, 1158, 1172, 1187, 1201, 1216, 1232,
    1247, 1263, 1279, 1295, 1312, 1328, 1345, 1363, 1380, 1398, 1416, 1435,
    1453, 1473, 1492, 1512, 1532, 1553, 1574, 1595, 1617, 1639, 1661, 1684,
    1708, 1732, 1756, 1782, 1807, 1833, 1860, 1887, 1915, 1944, 1973, 2003,
    2034, 2065, 2098, 2131, 2165, 2199, 2235, 2272, 2310, 2348, 2388, 2429,
    2472, 2515, 2560, 2606, 2654, 2703, 2754, 2807, 2861, 2918, 2976, 3036,
    3099, 3164, 3232, 3302, 3375, 3451, 3531, 3613, 3700, 3790, 3885, 3984,
    4088, 4197, 4311, 4432, 4560, 4694, 4836, 4987, 5147, 5318, 5499, 5693,
    5901, 6124, 6364, 6622, 6903, 7207, 7539, 7902, 8302, 8743, 9233, 9781,
    10396, 11094, 11891, 12810, 13882, 15148, 16667, 18524, 20843, 23826, 27801, 33366,
    41713, 55622, 83438, 166883, 2147483647,
};

}

std::int32_t wangle_lerp(std::int32_t int4_a, std::int32_t int4_b, std::int32_t int4_mul,
                         std::int32_t int4_div) {
  std::int32_t int4_diff{int4_b - int4_a};
  const std::int32_t int4_mask1{(511 - int4_diff) >> 31};
  const std::int32_t int4_mask2{(int4_diff + 512) >> 31};
  int4_diff += (int4_mask1 & -1024) | (int4_mask2 & 1024);
  return wangle_wrap(int4_a + int4_diff * int4_mul / int4_div);
}

std::int32_t wangle_cos(std::int32_t int4_angle_raw) {
  const std::int32_t int4_angle{wangle_wrap(int4_angle_raw)};
  const std::int32_t int4_q_index{int4_angle & 511};
  const std::int32_t int4_mirrored{256 - int4_q_index};
  const std::int32_t int4_mask{int4_mirrored >> 31};
  const std::int32_t int4_final_index{256 - ((int4_mirrored ^ int4_mask) - int4_mask)};
  const std::int32_t int4_sign_bit{static_cast<std::int32_t>(
      static_cast<std::uint32_t>(int4_angle + 256) >> 9) & 1};
  const std::int32_t int4_sign{1 - (int4_sign_bit << 1)};
  return int4_sign * kCosineTable[int4_final_index];
}

std::int32_t wangle_sin(std::int32_t int4_angle) {
  return wangle_cos(int4_angle - 256);
}

std::int32_t wangle_tan(std::int32_t int4_angle_raw) {

  const std::int32_t int4_angle{int4_angle_raw & 511};
  const std::int32_t int4_shifted{int4_angle - 257};
  const std::int32_t int4_mask{int4_shifted >> 31};
  const std::int32_t int4_delta{int4_angle - 256};
  const std::int32_t int4_triangle{(int4_delta ^ (int4_delta >> 31)) - (int4_delta >> 31)};
  const std::int32_t int4_final_index{256 - int4_triangle};
  const std::int32_t int4_sign{-1 - (int4_mask << 1)};
  return int4_sign * kTanTable[static_cast<std::size_t>(int4_final_index)];
}

std::int32_t wangle_arctan(std::int32_t int4_y, std::int32_t int4_x) {
  if (int4_y == 0) return wangle_wrap(int4_x >= 0 ? 0 : 512);
  if (int4_x == 0) return wangle_wrap(int4_y > 0 ? 256 : 768);

  const std::int64_t int8_ay{int4_y < 0 ? -static_cast<std::int64_t>(int4_y)
                                        : static_cast<std::int64_t>(int4_y)};
  const std::int64_t int8_ax{int4_x < 0 ? -static_cast<std::int64_t>(int4_x)
                                        : static_cast<std::int64_t>(int4_x)};

  if (int8_ay >= int8_ax * 167) return wangle_wrap(int4_y > 0 ? 256 : 768);

  const std::int32_t int4_target{static_cast<std::int32_t>((int8_ay << 10) / int8_ax)};

  std::int32_t int4_index{0};
  int4_index |= kTanTable[int4_index | 128] <= int4_target ? 128 : 0;
  int4_index |= kTanTable[int4_index | 64] <= int4_target ? 64 : 0;
  int4_index |= kTanTable[int4_index | 32] <= int4_target ? 32 : 0;
  int4_index |= kTanTable[int4_index | 16] <= int4_target ? 16 : 0;
  int4_index |= kTanTable[int4_index | 8] <= int4_target ? 8 : 0;
  int4_index |= kTanTable[int4_index | 4] <= int4_target ? 4 : 0;
  int4_index |= kTanTable[int4_index | 2] <= int4_target ? 2 : 0;
  int4_index |= kTanTable[int4_index | 1] <= int4_target ? 1 : 0;

  const std::int32_t int4_val{kTanTable[int4_index]};
  const std::int32_t int4_next_val{kTanTable[int4_index + 1]};
  int4_index += int4_target - int4_val > int4_next_val - int4_target ? 1 : 0;

  const std::int32_t int4_x_neg_result{512 + (int4_y < 0 ? int4_index : -int4_index)};
  const std::int32_t int4_x_pos_result{int4_y < 0 ? 1024 - int4_index : int4_index};
  return wangle_wrap(int4_x < 0 ? int4_x_neg_result : int4_x_pos_result);
}

std::int32_t tick_facing(std::int32_t int4_facing, std::int32_t int4_desired,
                         std::int32_t int4_step) {
  const std::int32_t int4_left_turn{wangle_wrap(int4_facing - int4_desired)};
  if (int4_left_turn < int4_step) return wangle_wrap(int4_desired);

  const std::int32_t int4_right_turn{wangle_wrap(int4_desired - int4_facing)};
  if (int4_right_turn < int4_step) return wangle_wrap(int4_desired);

  return wangle_wrap(int4_right_turn < int4_left_turn ? int4_facing + int4_step
                                                      : int4_facing - int4_step);
}

std::int64_t isqrt64(std::int64_t int8_number) {
  const std::uint64_t u8_number{static_cast<std::uint64_t>(int8_number)};
  std::uint64_t u8_divisor{1ULL << 62};
  std::uint64_t u8_root{0};
  std::uint64_t u8_remainder{u8_number};

  while (u8_divisor > u8_number) u8_divisor >>= 2;

  while (u8_divisor != 0) {
    if (u8_root + u8_divisor <= u8_remainder) {
      u8_remainder -= u8_root + u8_divisor;
      u8_root += 2 * u8_divisor;
    }
    u8_root >>= 1;
    u8_divisor >>= 2;
  }

  return static_cast<std::int64_t>(u8_root);
}

WVec wvec_sub(WPos pos_a, WPos pos_b) {
  return WVec{pos_a.int4_x - pos_b.int4_x, pos_a.int4_y - pos_b.int4_y,
              pos_a.int4_z - pos_b.int4_z};
}

std::int64_t wvec_length_squared(WVec vec) {
  return static_cast<std::int64_t>(vec.int4_x) * vec.int4_x +
         static_cast<std::int64_t>(vec.int4_y) * vec.int4_y +
         static_cast<std::int64_t>(vec.int4_z) * vec.int4_z;
}

std::int64_t wvec_horizontal_length_squared(WVec vec) {
  return static_cast<std::int64_t>(vec.int4_x) * vec.int4_x +
         static_cast<std::int64_t>(vec.int4_y) * vec.int4_y;
}

std::int32_t wvec_horizontal_length(WVec vec) {
  return static_cast<std::int32_t>(isqrt64(wvec_horizontal_length_squared(vec)));
}

std::int32_t wvec_yaw(WVec vec) {
  if (wvec_length_squared(vec) == 0) return 0;
  return wangle_wrap(wangle_arctan(-vec.int4_y, vec.int4_x) - 256);
}

WPos wpos_lerp(WPos pos_a, WPos pos_b, std::int32_t int4_mul, std::int32_t int4_div) {
  return WPos{pos_a.int4_x + (pos_b.int4_x - pos_a.int4_x) * int4_mul / int4_div,
              pos_a.int4_y + (pos_b.int4_y - pos_a.int4_y) * int4_mul / int4_div,
              pos_a.int4_z + (pos_b.int4_z - pos_a.int4_z) * int4_mul / int4_div};
}

std::int32_t int2_lerp(std::int32_t int4_a, std::int32_t int4_b, std::int32_t int4_mul,
                       std::int32_t int4_div) {
  return int4_a + (int4_b - int4_a) * int4_mul / int4_div;
}

WRot wrot_from_yaw(std::int32_t int4_yaw) {
  const std::int32_t int4_qy{wangle_wrap(-int4_yaw / 2)};
  const std::int32_t int4_cr{1024};
  const std::int32_t int4_sr{0};
  const std::int32_t int4_cp{1024};
  const std::int32_t int4_sp{0};
  const std::int64_t int8_cy{wangle_cos(int4_qy)};
  const std::int64_t int8_sy{wangle_sin(int4_qy)};

  WRot rot;
  rot.int4_x = static_cast<std::int32_t>(
      (int4_sr * int4_cp * int8_cy - int4_cr * int4_sp * int8_sy) / 1048576);
  rot.int4_y = static_cast<std::int32_t>(
      (int4_cr * int4_sp * int8_cy + int4_sr * int4_cp * int8_sy) / 1048576);
  rot.int4_z = static_cast<std::int32_t>(
      (int4_cr * int4_cp * int8_sy - int4_sr * int4_sp * int8_cy) / 1048576);
  rot.int4_w = static_cast<std::int32_t>(
      (int4_cr * int4_cp * int8_cy + int4_sr * int4_sp * int8_sy) / 1048576);
  return rot;
}

RotMatrix wrot_as_matrix(WRot rot) {
  const std::int64_t int8_lsq{static_cast<std::int64_t>(rot.int4_x) * rot.int4_x +
                              static_cast<std::int64_t>(rot.int4_y) * rot.int4_y +
                              static_cast<std::int64_t>(rot.int4_z) * rot.int4_z +
                              static_cast<std::int64_t>(rot.int4_w) * rot.int4_w};
  const std::int64_t int8_x{rot.int4_x};
  const std::int64_t int8_y{rot.int4_y};
  const std::int64_t int8_z{rot.int4_z};
  const std::int64_t int8_w{rot.int4_w};

  RotMatrix mtx;
  mtx.vec4_m[0] = static_cast<std::int32_t>(int8_lsq - 2 * (int8_y * int8_y + int8_z * int8_z));
  mtx.vec4_m[1] = static_cast<std::int32_t>(2 * (int8_x * int8_y + int8_z * int8_w));
  mtx.vec4_m[2] = static_cast<std::int32_t>(2 * (int8_x * int8_z - int8_y * int8_w));
  mtx.vec4_m[3] = 0;
  mtx.vec4_m[4] = static_cast<std::int32_t>(2 * (int8_x * int8_y - int8_z * int8_w));
  mtx.vec4_m[5] = static_cast<std::int32_t>(int8_lsq - 2 * (int8_x * int8_x + int8_z * int8_z));
  mtx.vec4_m[6] = static_cast<std::int32_t>(2 * (int8_y * int8_z + int8_x * int8_w));
  mtx.vec4_m[7] = 0;
  mtx.vec4_m[8] = static_cast<std::int32_t>(2 * (int8_x * int8_z + int8_y * int8_w));
  mtx.vec4_m[9] = static_cast<std::int32_t>(2 * (int8_y * int8_z - int8_x * int8_w));
  mtx.vec4_m[10] = static_cast<std::int32_t>(int8_lsq - 2 * (int8_x * int8_x + int8_y * int8_y));
  mtx.vec4_m[11] = 0;
  mtx.vec4_m[12] = 0;
  mtx.vec4_m[13] = 0;
  mtx.vec4_m[14] = 0;
  mtx.vec4_m[15] = static_cast<std::int32_t>(int8_lsq);
  return mtx;
}

WVec wvec_rotate_matrix(WVec vec, const RotMatrix& mtx) {
  const std::int64_t int8_x{vec.int4_x};
  const std::int64_t int8_y{vec.int4_y};
  const std::int64_t int8_z{vec.int4_z};
  return WVec{
      static_cast<std::int32_t>((int8_x * mtx.vec4_m[0] + int8_y * mtx.vec4_m[4] +
                                 int8_z * mtx.vec4_m[8]) /
                                mtx.vec4_m[15]),
      static_cast<std::int32_t>((int8_x * mtx.vec4_m[1] + int8_y * mtx.vec4_m[5] +
                                 int8_z * mtx.vec4_m[9]) /
                                mtx.vec4_m[15]),
      static_cast<std::int32_t>((int8_x * mtx.vec4_m[2] + int8_y * mtx.vec4_m[6] +
                                 int8_z * mtx.vec4_m[10]) /
                                mtx.vec4_m[15])};
}

WVec wvec_rotate_yaw(WVec vec, std::int32_t int4_yaw) {
  return wvec_rotate_matrix(vec, wrot_as_matrix(wrot_from_yaw(int4_yaw)));
}

}
