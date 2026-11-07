#pragma once

import std;

namespace ora::mods::common {

struct decoded_sound {
  std::int32_t int4_channels{};
  std::int32_t int4_sample_bits{};
  std::int32_t int4_sample_rate{};
  float f4_length_in_seconds{};
  std::vector<std::uint8_t> vec_pcm;
};

[[nodiscard]] std::optional<decoded_sound> wav_try_parse(std::span<const std::uint8_t> span_bytes);

}
