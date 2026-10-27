#pragma once
import std;

namespace ora::sound {

struct SoundFormatInfo {
  std::int32_t int4_channels = 0;
  std::int32_t int4_sample_bits = 0;
  std::int32_t int4_sample_rate = 0;
  float fp4_length_in_seconds = 0.0f;
};

using SoundLoaderFn = bool (*)(std::span<const std::byte> vec_file, SoundFormatInfo& info_out,
                               std::vector<std::byte>& vec_pcm);

std::vector<SoundLoaderFn> MakeSoundLoaders(const std::vector<std::string>& vec_format_names);

}
