#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

enum class AudSoundFormat : std::uint8_t {
  WestwoodCompressed = 1,
  ImaAdpcm = 99,
};

struct AudInfo {
  std::int32_t int4_sample_rate = 0;
  std::int32_t int4_sample_bits = 0;
  std::int32_t int4_channels = 0;
  float fp4_length_in_seconds = 0.0f;

  AudSoundFormat enum_format{};
  std::int64_t int8_data_offset = 0;
  std::int32_t int4_data_size = 0;
  std::int32_t int4_output_size = 0;
};

bool LoadAudSound(std::span<const std::byte> vec_file, AudInfo& info);

std::vector<std::byte> DecodeAudPcm(std::span<const std::byte> vec_file, const AudInfo& info);

bool IsAud(std::span<const std::byte> vec_file);

bool TryParseAud(std::span<const std::byte> vec_file, AudInfo& info, std::vector<std::byte>& vec_pcm);

}
