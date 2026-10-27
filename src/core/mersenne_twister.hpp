#pragma once
import std;
#include <cassert>

#include "exts_math.hpp"

namespace ora {

class MersenneTwister {
 public:
  std::int32_t Last{0};
  std::int32_t TotalCount{0};

  constexpr explicit MersenneTwister(std::int32_t int4_seed) { Seed(int4_seed); }

  constexpr void Seed(std::int32_t int4_seed) {
    mt_[0] = static_cast<std::uint32_t>(int4_seed);
    for (std::uint32_t uint4_i{1}; uint4_i < kMtSize; uint4_i++)
      mt_[uint4_i] = 1812433253u * (mt_[uint4_i - 1] ^ (mt_[uint4_i - 1] >> 30)) + uint4_i;
    index_ = 0;
    Last = 0;
    TotalCount = 0;
  }

  constexpr std::uint32_t NextUint() {
    if (index_ == 0)
      Generate();

    std::uint32_t uint4_y{mt_[static_cast<std::size_t>(index_)]};
    uint4_y ^= uint4_y >> 11;
    uint4_y ^= (uint4_y << 7) & 2636928640u;
    uint4_y ^= (uint4_y << 15) & 4022730752u;
    uint4_y ^= uint4_y >> 18;

    index_ = (index_ + 1) % 624;
    TotalCount++;

    Last = static_cast<std::int32_t>(static_cast<std::uint64_t>(uint4_y) % 2147483647u);
    return uint4_y;
  }

  constexpr std::uint64_t NextUlong() {
    return static_cast<std::uint64_t>(NextUint()) << 32 | NextUint();
  }

  constexpr std::int32_t Next() {
    NextUint();
    return Last;
  }

  constexpr std::int32_t Next(std::int32_t int4_low, std::int32_t int4_high) {
    assert(int4_high >= int4_low);
    const std::int32_t int4_diff{int4_high - int4_low};
    if (int4_diff <= 1)
      return int4_low;

    return int4_low + Next() % int4_diff;
  }

  constexpr std::int32_t Next(std::int32_t int4_high) { return Next(0, int4_high); }

  float NextFloat() { return std::abs(Next() / static_cast<float>(0x7fffffff)); }

  constexpr std::int32_t PickWeighted(std::span<const std::int32_t> weights_vec) {
    std::uint64_t uint8_total{0};
    for (const std::int32_t int4_weight : weights_vec) {
      assert(int4_weight >= 0);
      uint8_total += static_cast<std::uint64_t>(int4_weight);
    }

    if (uint8_total == 0)
      return Next(0, static_cast<std::int32_t>(weights_vec.size()));

    const std::uint64_t uint8_spin{NextUlong() % uint8_total};
    std::uint64_t uint8_acc{0};
    for (std::size_t size_i{0}; size_i < weights_vec.size(); size_i++) {
      uint8_acc += static_cast<std::uint64_t>(weights_vec[size_i]);
      if (uint8_spin < uint8_acc)
        return static_cast<std::int32_t>(size_i);
    }

    assert(false && "unreachable");
    return 0;
  }

  template <typename T>
  constexpr void ShuffleInPlace(std::span<T> span_buf, std::int32_t int4_start, std::int32_t int4_len) {
    for (std::int32_t int4_i{int4_len}; int4_i > 1; int4_i--) {
      const std::int32_t int4_swap{Next(int4_i)};
      const std::size_t size_hi{static_cast<std::size_t>(int4_start + int4_i - 1)};
      const std::size_t size_lo{static_cast<std::size_t>(int4_start + int4_swap)};
      T val_tmp{std::move(span_buf[size_hi])};
      span_buf[size_hi] = std::move(span_buf[size_lo]);
      span_buf[size_lo] = std::move(val_tmp);
    }
  }

 private:
  static constexpr std::uint32_t kMtSize{624};
  std::array<std::uint32_t, 624> mt_{};
  std::int32_t index_{0};

  constexpr void Generate() {
    for (std::uint32_t uint4_i{0}; uint4_i < kMtSize; uint4_i++) {
      const std::uint32_t uint4_y{(mt_[uint4_i] & 0x80000000u) |
                                  (mt_[(uint4_i + 1) % 624] & 0x7fffffffu)};
      mt_[uint4_i] = mt_[(uint4_i + 397u) % 624u] ^ (uint4_y >> 1);
      if ((uint4_y & 1) == 1)
        mt_[uint4_i] ^= 2567483615u;
    }
  }
};

}
