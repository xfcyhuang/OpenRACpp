#include "stb_vorbis.c"

import std;
#include "formats/ogg_loader.hpp"

namespace ora::fmt {

bool TryParseOgg(std::span<const std::byte> vec_file, OggInfo& info, std::vector<std::byte>& vec_pcm) {
  int int4_error = 0;
  stb_vorbis* ptr_vorbis = stb_vorbis_open_memory(
      reinterpret_cast<const unsigned char*>(vec_file.data()), static_cast<int>(vec_file.size()),
      &int4_error, nullptr);
  if (ptr_vorbis == nullptr)
    return false;

  try {
    const stb_vorbis_info info_vorbis = stb_vorbis_get_info(ptr_vorbis);
    info.int4_channels = info_vorbis.channels;
    info.int4_sample_rate = static_cast<std::int32_t>(info_vorbis.sample_rate);
    info.int4_sample_bits = 16;
    info.int8_total_samples = static_cast<std::int64_t>(stb_vorbis_stream_length_in_samples(ptr_vorbis));
    info.fp4_length_in_seconds =
        static_cast<float>(static_cast<double>(info.int8_total_samples) / info_vorbis.sample_rate);

    vec_pcm.clear();
    float** vec_frame_channels = nullptr;
    int int4_frame_samples = 0;
    while ((int4_frame_samples = stb_vorbis_get_frame_float(ptr_vorbis, nullptr,
                                                            &vec_frame_channels)) > 0) {
      const std::size_t st_old_size = vec_pcm.size();
      vec_pcm.resize(st_old_size +
                     static_cast<std::size_t>(int4_frame_samples) * static_cast<std::size_t>(info.int4_channels) * 2);
      for (std::int32_t int4_i{}; int4_i < int4_frame_samples; int4_i++) {
        for (std::int32_t int4_c{}; int4_c < info.int4_channels; int4_c++) {
          float fp4_sample = vec_frame_channels[int4_c][int4_i];

          if (fp4_sample > 1.0f)
            fp4_sample = 1.0f;
          else if (fp4_sample < -1.0f)
            fp4_sample = -1.0f;
          const auto uint2_pcm = static_cast<std::int16_t>(fp4_sample * 32767.0f);
          const std::size_t st_index =
              st_old_size + (static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(info.int4_channels) +
                             static_cast<std::size_t>(int4_c)) * 2;
          vec_pcm[st_index] = static_cast<std::byte>(static_cast<std::uint16_t>(uint2_pcm) & 0xFF);
          vec_pcm[st_index + 1] =
              static_cast<std::byte>((static_cast<std::uint16_t>(uint2_pcm) >> 8) & 0xFF);
        }
      }
    }
    stb_vorbis_close(ptr_vorbis);
    return true;
  } catch (...) {
    stb_vorbis_close(ptr_vorbis);
    return false;
  }
}

}
