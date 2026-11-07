#include "ima_adpcm.hpp"

namespace ora::mods::common {

namespace {

constexpr std::array<std::int32_t, 8> kIndexAdjust{-1, -1, -1, -1, 2, 4, 6, 8};

constexpr std::array<std::int32_t, 89> kStepTable{
    7,    8,    9,    10,   11,   12,   13,    14,    16,    17,   19,   21,   23,
    25,   28,   31,   34,   37,   41,   45,    50,    55,    60,   66,   73,   80,
    88,   97,   107,  118,  130,  143,  157,   173,   190,   209,  230,  253,  279,
    307,  337,  371,  408,  449,  494,  544,   598,   658,   724,  796,  876,  963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878,  2066,  2272,  2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484,  7132,  7845,  8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

}

std::int16_t decode_ima_adpcm_sample(std::uint8_t u1_b, std::int32_t& int4_index,
                                     std::int32_t& int4_current) {
  const bool b_sign{(u1_b & 8u) != 0u};
  const std::int32_t int4_b{static_cast<std::int32_t>(u1_b & 7u)};

  std::int32_t int4_delta{kStepTable[static_cast<std::size_t>(int4_index)] * int4_b / 4 +
                          kStepTable[static_cast<std::size_t>(int4_index)] / 8};
  if (b_sign) int4_delta = -int4_delta;

  int4_current += int4_delta;
  if (int4_current > 32767) int4_current = 32767;
  if (int4_current < -32768) int4_current = -32768;

  int4_index += kIndexAdjust[static_cast<std::size_t>(int4_b)];
  if (int4_index < 0) int4_index = 0;
  if (int4_index > 88) int4_index = 88;

  return static_cast<std::int16_t>(int4_current);
}

void load_ima_adpcm_block(std::span<const std::uint8_t> span_raw, std::int32_t& int4_index,
                          std::int32_t& int4_current, std::span<std::uint8_t> span_output) {
  std::size_t sz_offset{0};
  for (std::size_t sz_i{0}; sz_i < span_raw.size(); ++sz_i) {
    const std::uint8_t u1_b{span_raw[sz_i]};

    std::int16_t int4_t{decode_ima_adpcm_sample(u1_b, int4_index, int4_current)};
    span_output[sz_offset++] = static_cast<std::uint8_t>(int4_t);
    span_output[sz_offset++] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(int4_t) >> 8);

    int4_t = decode_ima_adpcm_sample(static_cast<std::uint8_t>(u1_b >> 4), int4_index, int4_current);
    span_output[sz_offset++] = static_cast<std::uint8_t>(int4_t);
    span_output[sz_offset++] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(int4_t) >> 8);
  }
}

}
