// UPSTREAM: OpenRA.Game/Exts.cs @7d57605 L282-373(ISqrt 家族与 √2 近似)
// 整数平方根:逐位算法,无浮点;三种舍入模式与上游逐一对应。
// 负数输入上游抛 InvalidOperationException,此处以契约断言承接(ORA_PRE 过渡形态)。
#pragma once
import std;
#include <cassert>  // assert 为宏,规范允许的 import/#include 并用正形态

namespace ora {

/// ISqrt 舍入模式(Exts.ISqrtRoundMode)
enum class ISqrtRoundMode : std::uint8_t { Floor, Nearest, Ceiling };

/// 整数平方根 uint32 版(Exts.cs L291-322)。逐位求根,每次两位。
constexpr std::uint32_t ISqrt(std::uint32_t uint4_number,
                              ISqrtRoundMode mode_round = ISqrtRoundMode::Floor) {
  std::uint32_t uint4_divisor{1U << 30};            // 二进制开方的当前项,从最高位起
  std::uint32_t uint4_root{0};                      // 累积的根
  std::uint32_t uint4_remainder{uint4_number};      // 剩余被开方数

  while (uint4_divisor > uint4_number)              // 定位被开方数的最高项
    uint4_divisor >>= 2;

  while (uint4_divisor != 0) {                      // 每迭代求出根的 2 位
    if (uint4_root + uint4_divisor <= uint4_remainder) {
      uint4_remainder -= uint4_root + uint4_divisor;
      uint4_root += 2 * uint4_divisor;
    }

    uint4_root >>= 1;
    uint4_divisor >>= 2;
  }

  if (mode_round == ISqrtRoundMode::Nearest && uint4_remainder > uint4_root)
    uint4_root++;
  else if (mode_round == ISqrtRoundMode::Ceiling && uint4_root * uint4_root < uint4_number)
    uint4_root++;

  return uint4_root;
}

/// 整数平方根 int32 版(Exts.cs L283-289)。负数为契约违规。
constexpr std::int32_t ISqrt(std::int32_t int4_number,
                             ISqrtRoundMode mode_round = ISqrtRoundMode::Floor) {
  assert(int4_number >= 0);  // 上游:负数抛 InvalidOperationException
  return static_cast<std::int32_t>(ISqrt(static_cast<std::uint32_t>(int4_number), mode_round));
}

/// 整数平方根 uint64 版(Exts.cs L332-362)
constexpr std::uint64_t ISqrt(std::uint64_t uint8_number,
                              ISqrtRoundMode mode_round = ISqrtRoundMode::Floor) {
  std::uint64_t uint8_divisor{1ULL << 62};
  std::uint64_t uint8_root{0};
  std::uint64_t uint8_remainder{uint8_number};

  while (uint8_divisor > uint8_number)
    uint8_divisor >>= 2;

  while (uint8_divisor != 0) {
    if (uint8_root + uint8_divisor <= uint8_remainder) {
      uint8_remainder -= uint8_root + uint8_divisor;
      uint8_root += 2 * uint8_divisor;
    }

    uint8_root >>= 1;
    uint8_divisor >>= 2;
  }

  if (mode_round == ISqrtRoundMode::Nearest && uint8_remainder > uint8_root)
    uint8_root++;
  else if (mode_round == ISqrtRoundMode::Ceiling && uint8_root * uint8_root < uint8_number)
    uint8_root++;

  return uint8_root;
}

/// 整数平方根 int64 版(Exts.cs L324-329)
constexpr std::int64_t ISqrt(std::int64_t int8_number,
                             ISqrtRoundMode mode_round = ISqrtRoundMode::Floor) {
  assert(int8_number >= 0);
  return static_cast<std::int64_t>(ISqrt(static_cast<std::uint64_t>(int8_number), mode_round));
}

/// 乘 √2 近似(Exts.cs L365-368)。上游参数为 short,提升 int 后乘除。
constexpr std::int32_t MultiplyBySqrtTwo(std::int16_t int2_number) {
  return int2_number * 46341 / 32768;
}

/// 乘 √2/2 近似(Exts.cs L370-373)。long 中间量防溢出。
constexpr std::int32_t MultiplyBySqrtTwoOverTwo(std::int32_t int4_number) {
  return static_cast<std::int32_t>(static_cast<std::int64_t>(int4_number) * 23170 / 32768);
}

}  // namespace ora
