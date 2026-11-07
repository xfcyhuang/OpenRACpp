#pragma once

import std;

namespace ora::mods::common {

std::int16_t decode_ima_adpcm_sample(std::uint8_t u1_b, std::int32_t& int4_index,
                                     std::int32_t& int4_current);

void load_ima_adpcm_block(std::span<const std::uint8_t> span_raw, std::int32_t& int4_index,
                          std::int32_t& int4_current, std::span<std::uint8_t> span_output);

}
