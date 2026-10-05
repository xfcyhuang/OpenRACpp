// UPSTREAM: OpenRA.Game/WDist.cs @b6fc03f L24-115(除 Lua 脚本绑定接口)
// 一维世界距离:1024 单位 = 1 cell。字段/方法名保留 C# 原名以保持审计对照。
// FromPDF 依赖 MersenneTwister,故本文件 include mersenne_twister.hpp。
// ToString/TryParse 供数据加载链(FieldLoader)使用,数字转换走 std::from/to_chars。
// 1D world distance: 1024 units = 1 cell. Field/method names keep the original C# names for audit cross-reference.
// FromPDF depends on MersenneTwister, so this file includes mersenne_twister.hpp.
// ToString/TryParse serve the data-loading chain (FieldLoader); numeric conversion goes through std::from/to_chars.
#pragma once
import std;
#include <cassert>

#include "mersenne_twister.hpp"

namespace ora {

/// 一维世界距离(WDist.cs L24)
/// 1D world distance (WDist.cs L24)
struct WDist {
  std::int32_t Length{0};  // 定点长度,1024 == 1 cell(保留上游字段名) | Fixed-point length, 1024 == 1 cell (upstream field name preserved)

  constexpr WDist() = default;
  constexpr explicit WDist(std::int32_t int4_r) : Length{int4_r} {}

  static constexpr WDist Zero() { return WDist{0}; }
  static constexpr WDist MaxValue() { return WDist{std::numeric_limits<std::int32_t>::max()}; }
  static constexpr WDist FromCells(std::int32_t int4_cells) { return WDist{1024 * int4_cells}; }

  constexpr std::int64_t LengthSquared() const {
    return static_cast<std::int64_t>(Length) * Length;
  }

  /// N 样本概率密度采样(WDist.cs L56-60):N 次 [-1024,1024) 采样求均值,
  /// N↑ 时近似高斯;1=矩形分布,2=三角分布。注意上游 Sum() 为 int 域求和(可回绕)
  /// N-sample probability density sampling (WDist.cs L56-60): mean of N draws from [-1024,1024);
  /// approaches Gaussian as N grows; 1 = rectangular distribution, 2 = triangular. Note upstream Sum() adds in the int domain (may wrap)
  static constexpr WDist FromPDF(MersenneTwister& rng_r, std::int32_t int4_samples) {
    std::int32_t int4_sum{0};
    for (std::int32_t int4_i{0}; int4_i < int4_samples; int4_i++)
      int4_sum += rng_r.Next(-1024, 1024);
    return WDist{int4_sum / int4_samples};
  }

  /// 解析 "NcM"/"N"/"NcM cM" 类文本(WDist.cs L62-94):按首个 c/C 拆分整数与小数部分,
  /// 负号传导到小数部分。std::from_chars 无 locale 无异常,契合确定性要求。
  /// Parses texts of the form "NcM"/"N"/"NcM cM" (WDist.cs L62-94): splits integer and fractional parts at the first c/C,
  /// with the negative sign propagated to the fractional part. std::from_chars is locale- and exception-free, fitting determinism requirements.
  static std::expected<WDist, bool> TryParse(std::string_view str_s) {
    if (str_s.empty())
      return std::unexpected{false};

    const std::size_t size_split{str_s.find_first_of("cC")};
    std::int32_t int4_cell{0};
    std::int32_t int4_subcell{0};

    if (size_split == std::string_view::npos) {
      // 单段:仅小数部分
      // Single segment: fractional part only
      if (!ParseInt(str_s, int4_subcell))
        return std::unexpected{false};
    } else {
      // 两段:cell 部分 + 小数部分
      // Two segments: cell part + fractional part
      if (!ParseInt(str_s.substr(0, size_split), int4_cell))
        return std::unexpected{false};
      if (!ParseInt(str_s.substr(size_split + 1), int4_subcell))
        return std::unexpected{false};
    }

    if (int4_cell < 0)  // 符号传导到小数部分(WDist.cs L89-90) | Sign propagated to the fractional part (WDist.cs L89-90)
      int4_subcell = -int4_subcell;

    return WDist{1024 * int4_cell + int4_subcell};
  }

  /// C# CompareTo(WDist)(WDist.cs L108):返回 -1/0/1;直接比较避免差值回绕误判
  /// C# CompareTo(WDist) (WDist.cs L108): returns -1/0/1; direct comparison avoids misjudgment from difference wraparound
  constexpr std::int32_t CompareTo(WDist other) const {
    return Length < other.Length ? -1 : (Length == other.Length ? 0 : 1);
  }

  /// "XcY" 文本形式(WDist.cs L110-115)
  /// "XcY" text form (WDist.cs L110-115)
  std::string ToString() const {
    const std::int32_t int4_abs{Length < 0 ? -Length : Length};
    std::string str_out{Length < 0 ? "-" : ""};
    str_out += std::format("{}c{}", int4_abs / 1024, int4_abs % 1024);
    return str_out;
  }

  /// 上游 GetHashCode(WDist.cs L96)
  /// Upstream GetHashCode (WDist.cs L96)
  constexpr std::int32_t Hash() const { return Length; }

  friend constexpr WDist operator+(WDist d_a, WDist d_b) { return WDist{d_a.Length + d_b.Length}; }
  friend constexpr WDist operator-(WDist d_a, WDist d_b) { return WDist{d_a.Length - d_b.Length}; }
  friend constexpr WDist operator-(WDist d_a) { return WDist{-d_a.Length}; }
  friend constexpr WDist operator/(WDist d_a, std::int32_t int4_b) { return WDist{d_a.Length / int4_b}; }
  friend constexpr WDist operator*(WDist d_a, std::int32_t int4_b) { return WDist{d_a.Length * int4_b}; }
  friend constexpr WDist operator*(std::int32_t int4_a, WDist d_b) { return WDist{int4_a * d_b.Length}; }
  friend constexpr bool operator<(WDist d_a, WDist d_b) { return d_a.Length < d_b.Length; }
  friend constexpr bool operator>(WDist d_a, WDist d_b) { return d_a.Length > d_b.Length; }
  friend constexpr bool operator<=(WDist d_a, WDist d_b) { return d_a.Length <= d_b.Length; }
  friend constexpr bool operator>=(WDist d_a, WDist d_b) { return d_a.Length >= d_b.Length; }
  friend constexpr bool operator==(WDist d_a, WDist d_b) { return d_a.Length == d_b.Length; }
  friend constexpr bool operator!=(WDist d_a, WDist d_b) { return !(d_a == d_b); }

 private:
  /// Exts.TryParseInt32Invariant 等价:from_chars 解析带可选符号的十进制 int
  /// Equivalent of Exts.TryParseInt32Invariant: from_chars parses a decimal int with an optional sign
  static bool ParseInt(std::string_view str_s, std::int32_t& int4_out) {
    if (str_s.empty())
      return false;
    const char* ptr_first{str_s.data()};
    const char* ptr_last{str_s.data() + str_s.size()};
    const auto [ptr_next, err_ec]{std::from_chars(ptr_first, ptr_last, int4_out)};
    return err_ec == std::errc{} && ptr_next == ptr_last;
  }
};

}  // namespace ora
