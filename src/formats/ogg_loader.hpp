#pragma once
import std;

namespace ora::fmt {

struct OggInfo {
  std::int32_t int4_channels = 0;
  std::int32_t int4_sample_bits = 16;
  std::int32_t int4_sample_rate = 0;
  float fp4_length_in_seconds = 0.0f;
  std::int64_t int8_total_samples = 0;
};

bool TryParseOgg(std::span<const std::byte> vec_file, OggInfo& info, std::vector<std::byte>& vec_pcm);

}
