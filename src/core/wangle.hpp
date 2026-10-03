// UPSTREAM: OpenRA.Game/WAngle.cs @7d57605 L24-269(除 Lua 脚本绑定接口)
// 一维定点角:1024 单位 = 360°。整数三角学:查表 + 无分支位技巧,
// 所有分支化写法(掩码取绝对值、瀑布二分)逐行照抄上游,任何"化简"都可能破坏对拍。
// 表数据照抄上游 L224-269(CosineTable: short[257],TanTable: int[257])。
// RendererRadians/RendererDegrees 为浮点,仅渲染域。
// 1D fixed-point angle: 1024 units = 360°. Integer trigonometry: table lookup + branchless bit tricks;
// every branch-shaped formulation (mask-based absolute value, waterfall binary search) is copied line by line
// from upstream — any "simplification" could break differential testing.
// Table data copied verbatim from upstream L224-269 (CosineTable: short[257], TanTable: int[257]).
// RendererRadians/RendererDegrees are floating-point, rendering domain only.
#pragma once
import std;
#include <cassert>

namespace ora {

/// 一维定点角(WAngle.cs L24)
/// 1D fixed-point angle (WAngle.cs L24)
struct WAngle {
  std::int32_t Angle{0};  // 角度值,恒在 [0,1024)(保留上游字段名) | Angle value, always in [0,1024) (upstream field name preserved)

  constexpr WAngle() = default;
  // 位掩码同时处理回绕与负数(WAngle.cs L32-33);explicit 防 int 隐式转角
  // The bit mask handles both wraparound and negatives (WAngle.cs L32-33); explicit prevents implicit int-to-angle conversion
  constexpr explicit WAngle(std::int32_t int4_a) : Angle{int4_a & 1023} {}

  static constexpr WAngle Zero() { return WAngle{0}; }
  static constexpr WAngle FromFacing(std::int32_t int4_facing) { return WAngle{int4_facing * 4}; }
  static constexpr WAngle FromDegrees(std::int32_t int4_degrees) {
    return WAngle{int4_degrees * 1024 / 360};
  }

  constexpr std::int32_t AngleSquared() const { return Angle * Angle; }
  constexpr std::int32_t Facing() const { return Angle / 4; }

  /// 上游 GetHashCode(WAngle.cs L51):即 Angle 本身
  /// Upstream GetHashCode (WAngle.cs L51): just Angle itself
  constexpr std::int32_t Hash() const { return Angle; }

  /// 正弦(WAngle.cs L59-62):sin(a) = cos(a - 90°)
  /// Sine (WAngle.cs L59-62): sin(a) = cos(a - 90°)
  constexpr std::int32_t Sin() const { return WAngle{Angle - 256}.Cos(); }

  /// 余弦(WAngle.cs L64-86):对称映射 1024 圆到 0-256 象限索引,查表 + 无分支符号
  /// Cosine (WAngle.cs L64-86): symmetrically maps the 1024-unit circle to a 0-256 quadrant index, table lookup + branchless sign
  constexpr std::int32_t Cos() const {
    const std::int32_t int4_angle{Angle};

    const std::int32_t int4_q{int4_angle & 511};              // 0-511 → 0-256 镜像三角波 | 0-511 → 0-256 mirrored triangle wave
    const std::int32_t int4_mirrored{256 - int4_q};
    const std::int32_t int4_mask{int4_mirrored >> 31};        // 算术右移取符号掩码 | Arithmetic right shift yields the sign mask
    const std::int32_t int4_final{256 - ((int4_mirrored ^ int4_mask) - int4_mask)};  // 无分支 abs | Branchless abs

    // 相移 90°(256 单位)使负半球对齐到第 9 位;位值 0→+1、1→-1 无分支映射
    // A 90° phase shift (256 units) aligns the negative hemisphere to bit 9; bit value 0→+1, 1→-1, branchless mapping
    const std::int32_t int4_sign_bit{static_cast<std::int32_t>(
        static_cast<std::uint32_t>(int4_angle + 256) >> 9) & 1};
    const std::int32_t int4_sign{1 - (int4_sign_bit << 1)};

    return int4_sign * CosineTable()[static_cast<std::uint32_t>(int4_final)];
  }

  /// 正切(WAngle.cs L88-106)
  /// Tangent (WAngle.cs L88-106)
  constexpr std::int32_t Tan() const {
    const std::int32_t int4_angle{Angle & 511};

    const std::int32_t int4_shifted{int4_angle - 257};  // +257 保持 90° 渐近线(256)为正 | +257 keeps the 90° asymptote (256) positive
    const std::int32_t int4_mask{int4_shifted >> 31};

    // 无分支 abs(angle-256):0-511 → 256-0-255 三角波
    // Branchless abs(angle-256): 0-511 → 256-0-255 triangle wave
    const std::int32_t int4_t{int4_angle - 256};
    const std::int32_t int4_triangle{(int4_t ^ (int4_t >> 31)) - (int4_t >> 31)};
    const std::int32_t int4_final{256 - int4_triangle};

    const std::int32_t int4_sign{-1 - (int4_mask << 1)};  // 位值 -1→+1、0→-1 | Bit value -1→+1, 0→-1

    return int4_sign * TanTable()[static_cast<std::uint32_t>(int4_final)];
  }

  /// 圆周最短路径插值(WAngle.cs L108-121):跨 1024 回绕取最短弧
  /// Shortest-path-on-circle interpolation (WAngle.cs L108-121): takes the shortest arc across the 1024 wraparound
  static constexpr WAngle Lerp(WAngle a_ang, WAngle b_ang, std::int32_t int4_mul, std::int32_t int4_div) {
    const std::int32_t int4_start{a_ang.Angle};
    std::int32_t int4_diff{b_ang.Angle - int4_start};

    const std::int32_t int4_mask1{(511 - int4_diff) >> 31};
    const std::int32_t int4_mask2{(int4_diff + 512) >> 31};
    int4_diff += (int4_mask1 & -1024) | (int4_mask2 & 1024);

    return WAngle{int4_start + int4_diff * int4_mul / int4_div};
  }

  /// 反正弦(WAngle.cs L123-135);d ∈ [-1024,1024],越界为契约违规
  /// Arcsine (WAngle.cs L123-135); d ∈ [-1024,1024], out of range is a contract violation
  static constexpr WAngle ArcSin(std::int32_t int4_d) {
    assert(static_cast<std::uint32_t>(int4_d + 1024) <= 2048);  // 无符号技巧范围检查 | Unsigned-trick range check

    const std::int32_t int4_index{ClosestCosineIndex(int4_d < 0 ? -int4_d : int4_d)};
    const std::int32_t int4_sign{int4_d >> 31};

    // 正 → Q1(0-256),负 → Q4(768-1024)
    // Positive → Q1 (0-256), negative → Q4 (768-1024)
    return WAngle{(int4_sign & (768 + int4_index)) | (~int4_sign & (256 - int4_index))};
  }

  /// 反余弦(WAngle.cs L137-147);d ∈ [-1024,1024]
  /// Arccosine (WAngle.cs L137-147); d ∈ [-1024,1024]
  static constexpr WAngle ArcCos(std::int32_t int4_d) {
    assert(static_cast<std::uint32_t>(int4_d + 1024) <= 2048);

    const std::int32_t int4_index{ClosestCosineIndex(int4_d < 0 ? -int4_d : int4_d)};
    const std::int32_t int4_sign{int4_d >> 31};

    return WAngle{(int4_sign & (512 - int4_index)) | (~int4_sign & int4_index)};
  }

  /// 反正切(WAngle.cs L174-216):查正切表的瀑布二分 + 象限映射
  /// Arctangent (WAngle.cs L174-216): waterfall binary search over the tangent table + quadrant mapping
  static constexpr WAngle ArcTan(std::int32_t int4_y, std::int32_t int4_x) {
    if (int4_y == 0)
      return WAngle{int4_x >= 0 ? 0 : 512};

    if (int4_x == 0)
      return WAngle{int4_y > 0 ? 256 : 768};

    const std::int64_t int8_ay{int4_y < 0 ? -static_cast<std::int64_t>(int4_y)
                                          : static_cast<std::int64_t>(int4_y)};
    const std::int64_t int8_ax{int4_x < 0 ? -static_cast<std::int64_t>(int4_x)
                                          : static_cast<std::int64_t>(int4_x)};

    // 比值超出正切表精度极限(~89.6°)时直接返回 90°
    // When the ratio exceeds the tangent table's precision limit (~89.6°), return 90° directly
    if (int8_ay >= int8_ax * 167)
      return WAngle{int4_y > 0 ? 256 : 768};

    // 一次移位 + 一次除法把比值放大为整数比较目标(免乘法二分)
    // One shift + one division scales the ratio into an integer comparison target (multiplication-free binary search)
    const std::int32_t int4_target{
        static_cast<std::int32_t>((int8_ay << 10) / int8_ax)};

    const auto& table_vec{TanTable()};
    std::int32_t int4_index{0};

    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 128)] <= int4_target) ? 128 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 64)] <= int4_target) ? 64 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 32)] <= int4_target) ? 32 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 16)] <= int4_target) ? 16 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 8)] <= int4_target) ? 8 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 4)] <= int4_target) ? 4 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 2)] <= int4_target) ? 2 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 1)] <= int4_target) ? 1 : 0;

    const std::int32_t int4_val{table_vec[static_cast<std::uint32_t>(int4_index)]};
    const std::int32_t int4_next{table_vec[static_cast<std::uint32_t>(int4_index + 1)]};
    int4_index += (int4_target - int4_val > int4_next - int4_target) ? 1 : 0;

    // 参考角映射回正确的笛卡尔象限
    // Map the reference angle back to the correct Cartesian quadrant
    const std::int32_t int4_neg_x{512 + (int4_y < 0 ? int4_index : -int4_index)};
    const std::int32_t int4_pos_x{int4_y < 0 ? 1024 - int4_index : int4_index};

    return WAngle{int4_x < 0 ? int4_neg_x : int4_pos_x};
  }

  /// 弧度(仅渲染域,浮点)(WAngle.cs L219)
  /// Radians (rendering domain only, floating-point) (WAngle.cs L219)
  float RendererRadians() const { return static_cast<float>(Angle * 3.14159265358979323846 / 512.0); }
  /// 度数(仅渲染域)(WAngle.cs L220)
  /// Degrees (rendering domain only) (WAngle.cs L220)
  float RendererDegrees() const { return Angle * 0.3515625f; }

  friend constexpr WAngle operator+(WAngle a_ang, WAngle b_ang) { return WAngle{a_ang.Angle + b_ang.Angle}; }
  friend constexpr WAngle operator-(WAngle a_ang, WAngle b_ang) { return WAngle{a_ang.Angle - b_ang.Angle}; }
  friend constexpr WAngle operator-(WAngle a_ang) { return WAngle{-a_ang.Angle}; }
  friend constexpr WAngle operator*(WAngle a_ang, std::int32_t int4_b) { return WAngle{a_ang.Angle * int4_b}; }
  friend constexpr WAngle operator*(std::int32_t int4_a, WAngle b_ang) { return WAngle{int4_a * b_ang.Angle}; }
  friend constexpr WAngle operator/(WAngle a_ang, std::int32_t int4_b) { return WAngle{a_ang.Angle / int4_b}; }
  /// WAngle / WAngle → 商(WAngle.cs L46)
  /// WAngle / WAngle → quotient (WAngle.cs L46)
  friend constexpr std::int32_t operator/(WAngle a_ang, WAngle b_ang) { return a_ang.Angle / b_ang.Angle; }
  friend constexpr bool operator==(WAngle a_ang, WAngle b_ang) { return a_ang.Angle == b_ang.Angle; }
  friend constexpr bool operator!=(WAngle a_ang, WAngle b_ang) { return !(a_ang == b_ang); }

 private:
  /// 余弦表查最近索引(WAngle.cs L151-172):瀑布二分取下界后选最近邻
  /// Closest index into the cosine table (WAngle.cs L151-172): waterfall binary search for the lower bound, then nearest neighbor
  static constexpr std::int32_t ClosestCosineIndex(std::int32_t int4_value) {
    const auto& table_vec{CosineTable()};
    std::int32_t int4_index{0};

    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 128)] > int4_value) ? 128 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 64)] > int4_value) ? 64 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 32)] > int4_value) ? 32 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 16)] > int4_value) ? 16 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 8)] > int4_value) ? 8 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 4)] > int4_value) ? 4 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 2)] > int4_value) ? 2 : 0;
    int4_index |= (table_vec[static_cast<std::uint32_t>(int4_index | 1)] > int4_value) ? 1 : 0;

    const std::int32_t int4_val0{table_vec[static_cast<std::uint32_t>(int4_index)]};
    const std::int32_t int4_idx1{int4_index + 1 < 256 ? int4_index + 1 : 256};
    const std::int32_t int4_val1{table_vec[static_cast<std::uint32_t>(int4_idx1)]};

    return (int4_val0 - int4_value > int4_value - int4_val1) ? int4_index + 1 : int4_index;
  }

  /// 余弦查找表(WAngle.cs L224-245):cos(2πk/1024)·1024,k ∈ [0,256]
  /// Cosine lookup table (WAngle.cs L224-245): cos(2πk/1024)·1024, k ∈ [0,256]
  static constexpr std::array<short, 257> CosineTable() {
    return {
        1024, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1022, 1022, 1022, 1021,
        1021, 1020, 1020, 1019, 1019, 1018, 1017, 1017, 1016, 1015, 1014, 1013,
        1012, 1011, 1010, 1009, 1008, 1007, 1006, 1005, 1004, 1003, 1001, 1000,
        999, 997, 996, 994, 993, 991, 990, 988, 986, 985, 983, 981, 979, 978,
        976, 974, 972, 970, 968, 966, 964, 962, 959, 957, 955, 953, 950, 948,
        946, 943, 941, 938, 936, 933, 930, 928, 925, 922, 920, 917, 914, 911,
        908, 906, 903, 900, 897, 894, 890, 887, 884, 881, 878, 875, 871, 868,
        865, 861, 858, 854, 851, 847, 844, 840, 837, 833, 829, 826, 822, 818,
        814, 811, 807, 803, 799, 795, 791, 787, 783, 779, 775, 771, 767, 762,
        758, 754, 750, 745, 741, 737, 732, 728, 724, 719, 715, 710, 706, 701,
        696, 692, 687, 683, 678, 673, 668, 664, 659, 654, 649, 644, 639, 634,
        629, 625, 620, 615, 609, 604, 599, 594, 589, 584, 579, 574, 568, 563,
        558, 553, 547, 542, 537, 531, 526, 521, 515, 510, 504, 499, 493, 488,
        482, 477, 471, 466, 460, 454, 449, 443, 437, 432, 426, 420, 414, 409,
        403, 397, 391, 386, 380, 374, 368, 362, 356, 350, 344, 339, 333, 327,
        321, 315, 309, 303, 297, 291, 285, 279, 273, 267, 260, 254, 248, 242,
        236, 230, 224, 218, 212, 205, 199, 193, 187, 181, 175, 168, 162, 156,
        150, 144, 137, 131, 125, 119, 112, 106, 100, 94, 87, 81, 75, 69, 62,
        56, 50, 43, 37, 31, 25, 18, 12, 6, 0};
  }

  /// 正切查找表(WAngle.cs L247-269):tan(2πk/1024)·1024,k ∈ [0,256]
  /// Tangent lookup table (WAngle.cs L247-269): tan(2πk/1024)·1024, k ∈ [0,256]
  static constexpr std::array<std::int32_t, 257> TanTable() {
    return {
        0, 6, 12, 18, 25, 31, 37, 44, 50, 56, 62, 69, 75, 81, 88, 94, 100, 107,
        113, 119, 126, 132, 139, 145, 151, 158, 164, 171, 177, 184, 190, 197,
        203, 210, 216, 223, 229, 236, 243, 249, 256, 263, 269, 276, 283, 290,
        296, 303, 310, 317, 324, 331, 338, 345, 352, 359, 366, 373, 380, 387,
        395, 402, 409, 416, 424, 431, 438, 446, 453, 461, 469, 476, 484, 492,
        499, 507, 515, 523, 531, 539, 547, 555, 563, 571, 580, 588, 596, 605,
        613, 622, 630, 639, 648, 657, 666, 675, 684, 693, 702, 711, 721, 730,
        740, 749, 759, 769, 779, 789, 799, 809, 819, 829, 840, 850, 861, 872,
        883, 894, 905, 916, 928, 939, 951, 963, 974, 986, 999, 1011, 1023, 1036,
        1049, 1062, 1075, 1088, 1102, 1115, 1129, 1143, 1158, 1172, 1187, 1201,
        1216, 1232, 1247, 1263, 1279, 1295, 1312, 1328, 1345, 1363, 1380, 1398,
        1416, 1435, 1453, 1473, 1492, 1512, 1532, 1553, 1574, 1595, 1617, 1639,
        1661, 1684, 1708, 1732, 1756, 1782, 1807, 1833, 1860, 1887, 1915, 1944,
        1973, 2003, 2034, 2065, 2098, 2131, 2165, 2199, 2235, 2272, 2310, 2348,
        2388, 2429, 2472, 2515, 2560, 2606, 2654, 2703, 2754, 2807, 2861, 2918,
        2976, 3036, 3099, 3164, 3232, 3302, 3375, 3451, 3531, 3613, 3700, 3790,
        3885, 3984, 4088, 4197, 4311, 4432, 4560, 4694, 4836, 4987, 5147, 5318,
        5499, 5693, 5901, 6124, 6364, 6622, 6903, 7207, 7539, 7902, 8302, 8743,
        9233, 9781, 10396, 11094, 11891, 12810, 13882, 15148, 16667, 18524, 20843,
        23826, 27801, 33366, 41713, 55622, 83438, 166883, 2147483647};
  }
};

}  // namespace ora
