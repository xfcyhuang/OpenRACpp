#pragma once
import std;

namespace ora::fmt {

struct Mp3Info {
  std::int32_t int4_channels = 0;
  std::int32_t int4_sample_bits = 16;
  std::int32_t int4_sample_rate = 0;
  float fp4_length_in_seconds = 0.0f;
};

bool IsMp3(std::span<const std::byte> vec_file);

bool TryParseMp3(std::span<const std::byte> vec_file, Mp3Info& info, std::vector<std::byte>& vec_pcm);

}
