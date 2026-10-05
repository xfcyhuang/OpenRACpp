// UPSTREAM: OpenRA.Mods.Common/FileFormats/ImaAdpcmReader.cs @b6fc03f
// L18-31(表)与 L33-85(两函数):逐句照抄;byte[]/Span → span。
// [UPSTREAM continued] ImaAdpcmReader.cs L18-31 (the tables) and L33-85 (both
// functions): copied statement by statement; byte[]/Span → span.
#include "formats/ima_adpcm.hpp"

namespace ora::fmt::ima_adpcm {
namespace {

// IndexAdjust(L18)/StepTable(L19-31)逐值。
// IndexAdjust (L18) / StepTable (L19-31), value for value.
constexpr int kIndexAdjust[] = {-1, -1, -1, -1, 2, 4, 6, 8};
constexpr int kStepTable[] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,
    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,   494,
    544,   598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,
    9493,  10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

}  // namespace

std::int16_t DecodeImaAdpcmSample(std::uint8_t uint1_b, int& int4_index, int& int4_current) {
  const bool bool_sign = (uint1_b & 8) != 0;
  uint1_b &= 7;

  auto int4_delta = kStepTable[int4_index] * uint1_b / 4 + kStepTable[int4_index] / 8;
  if (bool_sign)
    int4_delta = -int4_delta;

  int4_current += int4_delta;
  if (int4_current > 32767)
    int4_current = 32767;

  if (int4_current < -32768)
    int4_current = -32768;

  int4_index += kIndexAdjust[uint1_b];
  if (int4_index < 0)
    int4_index = 0;

  if (int4_index > 88)
    int4_index = 88;

  return static_cast<std::int16_t>(int4_current);
}

void LoadImaAdpcmSound(std::span<const std::byte> vec_raw, int& int4_index, std::span<std::byte> vec_output) {
  auto int4_current_sample = 0;
  LoadImaAdpcmSound(vec_raw, int4_index, int4_current_sample, vec_output);
}

void LoadImaAdpcmSound(std::span<const std::byte> vec_raw, int& int4_index, int& int4_current_sample,
                       std::span<std::byte> vec_output) {
  auto int4_data_size = static_cast<int>(vec_raw.size());
  if (vec_output.size() != vec_raw.size() * 4) [[unlikely]]
    throw std::runtime_error("output must be 4 times the length of raw.");

  auto st_offset = std::size_t{0};

  while (int4_data_size-- > 0) {
    const auto uint1_b = static_cast<std::uint8_t>(vec_raw[st_offset / 4]);

    auto int2_t = DecodeImaAdpcmSample(uint1_b, int4_index, int4_current_sample);
    vec_output[st_offset++] = static_cast<std::byte>(int2_t);
    vec_output[st_offset++] = static_cast<std::byte>(int2_t >> 8);

    int2_t = DecodeImaAdpcmSample(static_cast<std::uint8_t>(uint1_b >> 4), int4_index, int4_current_sample);
    vec_output[st_offset++] = static_cast<std::byte>(int2_t);
    vec_output[st_offset++] = static_cast<std::byte>(int2_t >> 8);
  }
}

}  // namespace ora::fmt::ima_adpcm
