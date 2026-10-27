#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

import std;
#include "formats/mp3_loader.hpp"

namespace ora::fmt {

bool IsMp3(std::span<const std::byte> vec_file) {

  if (vec_file.size() >= 3 &&
      static_cast<char>(vec_file[0]) == 'I' && static_cast<char>(vec_file[1]) == 'D' &&
      static_cast<char>(vec_file[2]) == '3')
    return true;

  if (vec_file.size() >= 2) {
    const auto uint2_frame_sync = static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(vec_file[0]) | (static_cast<std::uint8_t>(vec_file[1]) << 8));
    if (uint2_frame_sync == 0xfbff)
      return true;
  }

  return false;
}

bool TryParseMp3(std::span<const std::byte> vec_file, Mp3Info& info, std::vector<std::byte>& vec_pcm) {
  try {
    if (!IsMp3(vec_file))
      return false;
  } catch (const std::exception&) {

    return false;
  }

  mp3dec_t dec_mp3;
  mp3dec_init(&dec_mp3);

  std::vector<std::uint8_t> vec_input(reinterpret_cast<const std::uint8_t*>(vec_file.data()),
                                      reinterpret_cast<const std::uint8_t*>(vec_file.data()) +
                                          vec_file.size());
  mp3dec_frame_info_t info_frame{};
  mp3d_sample_t arr_samples[MINIMP3_MAX_SAMPLES_PER_FRAME];

  bool b_header_set = false;
  std::size_t st_cursor = 0;
  vec_pcm.clear();
  while (st_cursor < vec_input.size()) {
    const int int4_samples = mp3dec_decode_frame(
        &dec_mp3, vec_input.data() + st_cursor,
        static_cast<int>(vec_input.size() - st_cursor), arr_samples, &info_frame);
    st_cursor += static_cast<std::size_t>(info_frame.frame_bytes);

    if (info_frame.frame_bytes == 0)
      break;

    if (!b_header_set && info_frame.channels > 0 && info_frame.hz > 0) {
      info.int4_channels = info_frame.channels;
      info.int4_sample_rate = info_frame.hz;
      info.int4_sample_bits = 16;
      b_header_set = true;
    }

    if (int4_samples > 0) {
      const std::size_t st_bytes = static_cast<std::size_t>(int4_samples) *
                                   static_cast<std::size_t>(info_frame.channels) * sizeof(mp3d_sample_t);
      const auto* ptr_bytes = reinterpret_cast<const std::byte*>(arr_samples);
      vec_pcm.insert(vec_pcm.end(), ptr_bytes, ptr_bytes + st_bytes);
    }
  }

  if (!b_header_set)
    return false;

  const std::int64_t int8_samples =
      static_cast<std::int64_t>(vec_pcm.size() / 2 / static_cast<std::size_t>(info.int4_channels));
  info.fp4_length_in_seconds =
      static_cast<float>(static_cast<double>(int8_samples) / info.int4_sample_rate);
  return true;
}

}
