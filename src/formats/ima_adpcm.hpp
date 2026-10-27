#pragma once
import std;

namespace ora::fmt::ima_adpcm {

std::int16_t DecodeImaAdpcmSample(std::uint8_t uint1_b, int& int4_index, int& int4_current);

void LoadImaAdpcmSound(std::span<const std::byte> vec_raw, int& int4_index, std::span<std::byte> vec_output);

void LoadImaAdpcmSound(std::span<const std::byte> vec_raw, int& int4_index, int& int4_current_sample,
                       std::span<std::byte> vec_output);

}
