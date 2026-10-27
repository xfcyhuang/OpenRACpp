#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

enum class WaveType : std::int16_t {
  Pcm = 0x1,
  MsAdpcm = 0x2,
  ImaAdpcm = 0x11,
};

struct WavInfo {
  std::int16_t int2_channels = -1;
  std::int32_t int4_sample_bits = -1;
  std::int32_t int4_sample_rate = -1;
  float fp4_length_in_seconds = -1.0f;

  WaveType enum_audio_type{};
  std::int64_t int8_data_offset = -1;
  std::int32_t int4_data_size = -1;
  std::int32_t int4_uncompressed_size = -1;
  std::int16_t int2_block_align = -1;
};

bool LoadWavSound(std::span<const std::byte> vec_file, WavInfo& info);

std::vector<std::byte> DecodeWavPcm(std::span<const std::byte> vec_file, const WavInfo& info);

bool IsWave(std::span<const std::byte> vec_file);

bool TryParseWav(std::span<const std::byte> vec_file, WavInfo& info, std::vector<std::byte>& vec_pcm);

}
