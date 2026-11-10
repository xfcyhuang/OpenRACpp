export module ora.mods.common;

import std;

export {
namespace ora::mods::common {

int common_tag();

std::string_view common_name();

}

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

namespace ora::mods::common {

std::int16_t decode_ima_adpcm_sample(std::uint8_t u1_b, std::int32_t& int4_index,
                                     std::int32_t& int4_current);

void load_ima_adpcm_block(std::span<const std::uint8_t> span_raw, std::int32_t& int4_index,
                          std::int32_t& int4_current, std::span<std::uint8_t> span_output);

}
}
