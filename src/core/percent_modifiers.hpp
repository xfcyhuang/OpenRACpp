#pragma once
import std;
#include <cassert>

#define ORA_CASSERT(cond)   do {     if !consteval { assert(cond); }   } while (0)

namespace ora {

constexpr unsigned __int128 Abs128(__int128 int16_value) {
  return int16_value < 0
             ? static_cast<unsigned __int128>(-(int16_value + 1)) + 1
             : static_cast<unsigned __int128>(int16_value);
}

constexpr std::int32_t ApplyPercentageModifiers(
    std::int32_t int4_number, std::span<const std::int32_t> vec_percentages) {
  constexpr std::size_t kMaxChain = 18;
  ORA_CASSERT(vec_percentages.size() <= kMaxChain);

  __int128 int16_numerator{int4_number};
  __int128 int16_denominator{1};

  for (const std::int32_t int4_percent : vec_percentages) {

    ORA_CASSERT(int4_percent == 0 || int16_numerator == 0 ||
           Abs128(int16_numerator) <=
               (static_cast<unsigned __int128>(~static_cast<unsigned __int128>(0)) >> 1) /
                   (int4_percent < 0 ? -static_cast<std::int64_t>(int4_percent)
                                     : static_cast<std::int64_t>(int4_percent)));
    int16_numerator *= int4_percent;
    int16_denominator *= 100;
  }

  const __int128 int16_quotient{
      int16_numerator / int16_denominator};
  ORA_CASSERT(int16_quotient >= std::numeric_limits<std::int32_t>::min() &&
              int16_quotient <= std::numeric_limits<std::int32_t>::max());
  return static_cast<std::int32_t>(int16_quotient);
}

}
