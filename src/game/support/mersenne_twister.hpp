#pragma once

import std;

namespace ora::game::support {

struct MersenneTwister {
  std::array<std::uint32_t, 624> vec4u_mt{};
  std::int32_t int4_index{0};
  std::int32_t int4_last{0};
  std::int32_t int4_total_count{0};
};

MersenneTwister make_mersenne_twister(std::int32_t int4_seed);

std::uint32_t next_uint(MersenneTwister& mt_rng);
std::uint64_t next_ulong(MersenneTwister& mt_rng);
std::int32_t next(MersenneTwister& mt_rng);
std::int32_t next(MersenneTwister& mt_rng, std::int32_t int4_high);
std::int32_t next(MersenneTwister& mt_rng, std::int32_t int4_low, std::int32_t int4_high);
float next_float(MersenneTwister& mt_rng);
std::int32_t pick_weighted(MersenneTwister& mt_rng, std::span<const std::int32_t> vec4_weights);

template <typename T>
void shuffle_in_place(MersenneTwister& mt_rng, std::span<T> span_items, std::int32_t int4_start, std::int32_t int4_len) {
  for (std::int32_t int4_i{int4_len}; int4_i > 1; --int4_i) {
    std::int32_t int4_swap{next(mt_rng, int4_i)};
    std::swap(span_items[static_cast<std::size_t>(int4_start + int4_i - 1)], span_items[static_cast<std::size_t>(int4_start + int4_swap)]);
  }
}

}
