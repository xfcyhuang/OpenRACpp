#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

struct VocSampleBlock {
  std::int32_t int4_rate = 0;
  std::int32_t int4_samples = 0;
  std::int64_t int8_offset = 0;

  constexpr bool operator==(const VocSampleBlock&) const = default;
};

struct VocLoopBlock {
  std::int32_t int4_count = 0;
  constexpr bool operator==(const VocLoopBlock&) const = default;
};

struct VocBlock {
  std::int32_t int4_code = 0;
  std::int32_t int4_length = 0;
  VocSampleBlock block_sample{};
  VocLoopBlock block_loop{};

  constexpr bool operator==(const VocBlock&) const = default;
};

struct VocInfo {
  std::int32_t int4_sample_rate = 0;
  std::int32_t int4_sample_bits = 8;
  std::int32_t int4_channels = 1;
  float fp4_length_in_seconds = 0.0f;
  std::int32_t int4_total_samples = 0;
  std::vector<VocBlock> vec_blocks;
};

void LoadVoc(std::span<const std::byte> vec_file, VocInfo& info);

std::vector<std::byte> DecodeVocPcm(std::span<const std::byte> vec_file, const VocInfo& info);

bool TryParseVoc(std::span<const std::byte> vec_file, VocInfo& info, std::vector<std::byte>& vec_pcm);

}
