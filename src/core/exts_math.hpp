#pragma once
import std;
#include <cassert>

namespace ora {

enum class ISqrtRoundMode : std::uint8_t { Floor, Nearest, Ceiling };

constexpr std::uint32_t ISqrt(std::uint32_t uint4_number,
                              ISqrtRoundMode mode_round = ISqrtRoundMode::Floor) {
  std::uint32_t uint4_divisor{1U << 30};
  std::uint32_t uint4_root{0};
  std::uint32_t uint4_remainder{uint4_number};

  while (uint4_divisor > uint4_number)
    uint4_divisor >>= 2;

  while (uint4_divisor != 0) {
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

constexpr std::int32_t ISqrt(std::int32_t int4_number,
                             ISqrtRoundMode mode_round = ISqrtRoundMode::Floor) {
  assert(int4_number >= 0);
  return static_cast<std::int32_t>(ISqrt(static_cast<std::uint32_t>(int4_number), mode_round));
}

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

constexpr std::int64_t ISqrt(std::int64_t int8_number,
                             ISqrtRoundMode mode_round = ISqrtRoundMode::Floor) {
  assert(int8_number >= 0);
  return static_cast<std::int64_t>(ISqrt(static_cast<std::uint64_t>(int8_number), mode_round));
}

constexpr std::int32_t MultiplyBySqrtTwo(std::int16_t int2_number) {
  return int2_number * 46341 / 32768;
}

constexpr std::int32_t MultiplyBySqrtTwoOverTwo(std::int32_t int4_number) {
  return static_cast<std::int32_t>(static_cast<std::int64_t>(int4_number) * 23170 / 32768);
}

}
