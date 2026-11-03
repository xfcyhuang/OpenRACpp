#include "game/support/mersenne_twister.hpp"

namespace ora::game::support {

namespace {

constexpr std::uint32_t kMtN{624};
constexpr std::uint32_t kMtM{397};
constexpr std::uint32_t kInitMult{1812433253};
constexpr std::uint32_t kTemperB{0x9d2c5680};
constexpr std::uint32_t kTemperC{0xefc60000};
constexpr std::uint32_t kMatrixA{0x9908b0df};
constexpr std::int32_t kIntMax{2147483647};

[[noreturn]] void fail_unreachable(std::string_view str_what) {
  std::println("mersenne_twister: {}", str_what);
  std::abort();
}

void generate(MersenneTwister& mt_rng) {
  for (std::uint32_t u4_i{0}; u4_i < kMtN; ++u4_i) {
    std::uint32_t u4_y{(mt_rng.vec4u_mt[u4_i] & 0x80000000u) | (mt_rng.vec4u_mt[(u4_i + 1) % kMtN] & 0x7fffffffu)};
    mt_rng.vec4u_mt[u4_i] = mt_rng.vec4u_mt[(u4_i + kMtM) % kMtN] ^ (u4_y >> 1);
    if ((u4_y & 1) == 1)
      mt_rng.vec4u_mt[u4_i] ^= kMatrixA;
  }
}

}

MersenneTwister make_mersenne_twister(std::int32_t int4_seed) {
  MersenneTwister mt_rng{};
  mt_rng.vec4u_mt[0] = static_cast<std::uint32_t>(int4_seed);
  for (std::uint32_t u4_i{1}; u4_i < kMtN; ++u4_i) {
    std::uint32_t u4_prev{mt_rng.vec4u_mt[u4_i - 1]};
    mt_rng.vec4u_mt[u4_i] = kInitMult * (u4_prev ^ (u4_prev >> 30)) + u4_i;
  }
  return mt_rng;
}

std::uint32_t next_uint(MersenneTwister& mt_rng) {
  if (mt_rng.int4_index == 0)
    generate(mt_rng);

  std::uint32_t u4_y{mt_rng.vec4u_mt[static_cast<std::uint32_t>(mt_rng.int4_index)]};
  u4_y ^= u4_y >> 11;
  u4_y ^= (u4_y << 7) & kTemperB;
  u4_y ^= (u4_y << 15) & kTemperC;
  u4_y ^= u4_y >> 18;

  mt_rng.int4_index = static_cast<std::int32_t>((static_cast<std::uint32_t>(mt_rng.int4_index) + 1) % kMtN);
  ++mt_rng.int4_total_count;
  mt_rng.int4_last = static_cast<std::int32_t>(u4_y % static_cast<std::uint32_t>(kIntMax));
  return u4_y;
}

std::uint64_t next_ulong(MersenneTwister& mt_rng) {
  std::uint64_t u8_hi{static_cast<std::uint64_t>(next_uint(mt_rng))};
  std::uint64_t u8_lo{static_cast<std::uint64_t>(next_uint(mt_rng))};
  return u8_hi << 32 | u8_lo;
}

std::int32_t next(MersenneTwister& mt_rng) {
  next_uint(mt_rng);
  return mt_rng.int4_last;
}

std::int32_t next(MersenneTwister& mt_rng, std::int32_t int4_high) {
  return next(mt_rng, 0, int4_high);
}

std::int32_t next(MersenneTwister& mt_rng, std::int32_t int4_low, std::int32_t int4_high) {
  if (int4_high < int4_low)
    fail_unreachable("next: high < low");
  std::int32_t int4_diff{int4_high - int4_low};
  if (int4_diff <= 1)
    return int4_low;
  return int4_low + next(mt_rng) % int4_diff;
}

float next_float(MersenneTwister& mt_rng) {
  float fp4_value{static_cast<float>(next(mt_rng)) / static_cast<float>(kIntMax)};
  return std::fabs(fp4_value);
}

std::int32_t pick_weighted(MersenneTwister& mt_rng, std::span<const std::int32_t> vec4_weights) {
  std::uint64_t u8_total{0};
  for (std::int32_t int4_weight : vec4_weights) {
    if (int4_weight < 0)
      fail_unreachable("pick_weighted: negative weight");
    u8_total += static_cast<std::uint64_t>(int4_weight);
  }
  if (u8_total == 0)
    return next(mt_rng, 0, static_cast<std::int32_t>(vec4_weights.size()));

  std::uint64_t u8_spin{next_ulong(mt_rng) % u8_total};
  std::uint64_t u8_acc{0};
  for (std::int32_t int4_index{0}; int4_index < static_cast<std::int32_t>(vec4_weights.size()); ++int4_index) {
    u8_acc += static_cast<std::uint64_t>(vec4_weights[static_cast<std::size_t>(int4_index)]);
    if (u8_spin < u8_acc)
      return int4_index;
  }
  fail_unreachable("pick_weighted: unreachable");
}

}
