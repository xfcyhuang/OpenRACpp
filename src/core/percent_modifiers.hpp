// UPSTREAM: OpenRA.Mods.Common/Util.cs @b6fc03f L203-211(ApplyPercentageModifiers)
// 百分比修正链的精确十进制语义:上游以 C# decimal(128 位软十进制)逐项乘 p/100m,
// 最后 (int) 向零截断。100=2²×5² 保证 p/100 恒为有限小数;实践值域(number ≤ 10 位、
// 百分比 ≤ 4 位、链长 ≤ 4,见 Armament/Health/Mobile 等 22 个调用点)内 decimal 乘法
// 永不触发 28 位有效数字舍入,故该函数等价于"精确有理数 number·Πpᵢ/100^k + 向零截断"。
// 此处以 __int128 累积分母 100^k 的分子精确复刻;理论极端值域(分子超 __int128、
// 链长 > 18、商超 int32)上游抛 OverflowException,此处以契约断言承接,实践不可达。
// 调用面差异:参数由 IEnumerable<int>(上游 LINQ 链 + 每调用 ToArray 分配)改为
// span 直传 —— 上游 22 处调用点(Armament.MaxRange/MovementSpeedForCell/伤害/
// 视野等)全部同步路径,见 docs/OPTIMIZATION_TRACKER.md OPT-A1。
// Exact decimal semantics of the percentage-modifier chain: upstream multiplies
// p/100m item by item in C# decimal (128-bit soft decimal) and finally truncates
// with (int) towards zero. Since 100 = 2²×5², p/100 is always a finite decimal,
// and within practical ranges (number ≤ 10 digits, percentages ≤ 4 digits,
// chain length ≤ 4 — see the 22 call sites in Armament/Health/Mobile etc.) the
// decimal products never hit the 28-significant-digit rounding, so the function
// equals "exact rational number·Πpᵢ/100^k truncated towards zero". Here an
// __int128 numerator over the fixed denominator 100^k reproduces it exactly;
// the theoretical extreme range (numerator beyond __int128, chain > 18, quotient
// beyond int32 — upstream throws OverflowException) is caught by contract
// assertions and is unreachable in practice. The parameter is a plain span
// instead of IEnumerable<int> (upstream: LINQ chains + per-call ToArray
// allocations); all 22 call sites are on the synced path. See
// docs/OPTIMIZATION_TRACKER.md OPT-A1.
#pragma once
import std;
#include <cassert>  // assert 为宏,规范允许的 import/#include 并用正形态 | assert is a macro; the spec-sanctioned mixed form

/// 常量求值安全的契约断言:编译期求值路径退化为无操作(溢出/超界契约由
/// 运行期断言与测试/黄金对拍覆盖),运行期等价 assert。
/// Constant-evaluation-safe contract assertion: the compile-time evaluation
/// path degrades to a no-op (the overflow/range contracts are covered by the
/// runtime assertions plus tests/golden differentials); at runtime it is plain assert.
#define ORA_CASSERT(cond)   do {     if !consteval { assert(cond); }   } while (0)

namespace ora {

/// |v| 的无符号绝对值(INT128_MIN 安全:-fwrapv 下 -(v+1)+1 不回绕)
/// Unsigned absolute value of v (INT128_MIN-safe: -(v+1)+1 never wraps under -fwrapv)
constexpr unsigned __int128 Abs128(__int128 int16_value) {
  return int16_value < 0
             ? static_cast<unsigned __int128>(-(int16_value + 1)) + 1
             : static_cast<unsigned __int128>(int16_value);
}

/// 百分比修正链(Util.cs L203-211)。分子 __int128 精确累积,整除即 (int)decimal 的
/// 向零截断。等价域论证见文件头;黄金对拍(边界舍入用例)登记于 OPT-A1 验收项。
/// The percentage-modifier chain (Util.cs L203-211). The __int128 numerator
/// accumulates exactly and the integer division is the (int)decimal truncation
/// towards zero. See the file header for the equivalence-domain argument; the
/// golden differential (boundary-rounding cases) is registered as the OPT-A1
/// acceptance item.
constexpr std::int32_t ApplyPercentageModifiers(
    std::int32_t int4_number, std::span<const std::int32_t> vec_percentages) {
  constexpr std::size_t kMaxChain = 18;  // 100^18 < 2^127,分母不溢出 | 100^18 < 2^127 keeps the denominator in range
  ORA_CASSERT(vec_percentages.size() <= kMaxChain);

  __int128 int16_numerator{int4_number};  // 精确有理数的分子 | numerator of the exact rational
  __int128 int16_denominator{1};          // 恒为 100^k | always 100^k

  for (const std::int32_t int4_percent : vec_percentages) {
    // 乘前溢出预检:超 __int128 即上游 decimal OverflowException 域(实践不可达)
    // Pre-multiply overflow check: beyond __int128 is the upstream decimal
    // OverflowException domain (unreachable in practice).
    ORA_CASSERT(int4_percent == 0 || int16_numerator == 0 ||
           Abs128(int16_numerator) <=
               (static_cast<unsigned __int128>(~static_cast<unsigned __int128>(0)) >> 1) /
                   (int4_percent < 0 ? -static_cast<std::int64_t>(int4_percent)
                                     : static_cast<std::int64_t>(int4_percent)));
    int16_numerator *= int4_percent;
    int16_denominator *= 100;
  }

  const __int128 int16_quotient{
      int16_numerator / int16_denominator};  // C++ 整除 = 向零截断 =(int)decimal
  ORA_CASSERT(int16_quotient >= std::numeric_limits<std::int32_t>::min() &&
              int16_quotient <= std::numeric_limits<std::int32_t>::max());
  return static_cast<std::int32_t>(int16_quotient);
}

}  // namespace ora
