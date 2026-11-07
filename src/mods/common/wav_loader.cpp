#include "wav_loader.hpp"

#include "ima_adpcm.hpp"

namespace ora::mods::common {

namespace {

constexpr std::int16_t kWaveTypePcm{0x1};
constexpr std::int16_t kWaveTypeMsAdpcm{0x2};
constexpr std::int16_t kWaveTypeImaAdpcm{0x11};

struct byte_reader {
  std::span<const std::uint8_t> span_bytes;
  std::size_t sz_pos{0};

  [[nodiscard]] bool read_u8(std::uint8_t& u1_out) {
    if (sz_pos >= span_bytes.size()) return false;
    u1_out = span_bytes[sz_pos++];
    return true;
  }

  [[nodiscard]] bool read_ascii4(char (&chr_out)[4]) {
    if (span_bytes.size() - sz_pos < 4u) return false;
    for (std::size_t sz_i{0}; sz_i < 4u; ++sz_i) chr_out[sz_i] = static_cast<char>(span_bytes[sz_pos++]);
    return true;
  }

  [[nodiscard]] bool read_u32(std::uint32_t& u4_out) {
    if (span_bytes.size() - sz_pos < 4u) return false;
    u4_out = static_cast<std::uint32_t>(span_bytes[sz_pos]) |
             (static_cast<std::uint32_t>(span_bytes[sz_pos + 1u]) << 8) |
             (static_cast<std::uint32_t>(span_bytes[sz_pos + 2u]) << 16) |
             (static_cast<std::uint32_t>(span_bytes[sz_pos + 3u]) << 24);
    sz_pos += 4u;
    return true;
  }

  [[nodiscard]] bool read_i16(std::int16_t& int4_out) {
    if (span_bytes.size() - sz_pos < 2u) return false;
    const std::uint32_t u4_value{static_cast<std::uint32_t>(span_bytes[sz_pos]) |
                                 (static_cast<std::uint32_t>(span_bytes[sz_pos + 1u]) << 8)};
    int4_out = static_cast<std::int16_t>(u4_value);
    sz_pos += 2u;
    return true;
  }

  [[nodiscard]] bool read_i32(std::int32_t& int4_out) {
    std::uint32_t u4_value{0};
    if (!read_u32(u4_value)) return false;
    int4_out = static_cast<std::int32_t>(u4_value);
    return true;
  }

  [[nodiscard]] bool block_type_is(std::string_view str_tag, std::string_view str_want) const {
    return str_tag.substr(0u, 4u) == str_want.substr(0u, 4u);
  }
};

void enqueue_pcm_bytes(std::vector<std::uint8_t>& vec_out, std::span<const std::uint8_t> span_bytes) {
  vec_out.insert(vec_out.end(), span_bytes.begin(), span_bytes.end());
}

void decode_ima_blocks(std::span<const std::uint8_t> span_bytes, std::size_t sz_data_offset,
                       std::int32_t int4_data_size, std::int16_t int4_block_align,
                       std::int32_t int4_channels, std::int32_t int4_uncompressed_size,
                       std::vector<std::uint8_t>& vec_out) {
  const std::int32_t int4_num_blocks{int4_data_size / int4_block_align};
  const std::int32_t int4_block_data_size{int4_block_align - int4_channels * 4};
  const std::int32_t int4_output_size{int4_uncompressed_size * int4_channels * 2};
  if (int4_block_data_size < 0) return;

  std::vector<std::int32_t> vec_predictor(static_cast<std::size_t>(int4_channels), 0);
  std::vector<std::int32_t> vec_index(static_cast<std::size_t>(int4_channels), 0);
  std::vector<std::uint8_t> vec_block_data(static_cast<std::size_t>(int4_block_data_size), 0);
  std::vector<std::uint8_t> vec_decoded(16u, 0);
  std::vector<std::uint8_t> vec_interleave(static_cast<std::size_t>(int4_channels) * 16u, 0);

  std::int32_t int4_out_offset{0};
  std::int32_t int4_current_block{0};
  std::size_t sz_stream_pos{sz_data_offset};
  const std::size_t sz_data_end{sz_data_offset + static_cast<std::size_t>(int4_data_size)};

  while (true) {
    if (sz_stream_pos + static_cast<std::size_t>(int4_channels) * 4u > sz_data_end) return;

    for (std::int32_t int4_c{0}; int4_c < int4_channels; ++int4_c) {
      const std::uint16_t u4_lo{static_cast<std::uint16_t>(span_bytes[sz_stream_pos++])};
      const std::uint16_t u4_hi{static_cast<std::uint16_t>(span_bytes[sz_stream_pos++])};
      vec_predictor[static_cast<std::size_t>(int4_c)] =
          static_cast<std::int16_t>(u4_lo | (u4_hi << 8));
      vec_index[static_cast<std::size_t>(int4_c)] =
          static_cast<std::int32_t>(span_bytes[sz_stream_pos++]);
      ++sz_stream_pos;

      vec_out.push_back(static_cast<std::uint8_t>(vec_predictor[static_cast<std::size_t>(int4_c)]));
      vec_out.push_back(static_cast<std::uint8_t>(
          static_cast<std::uint16_t>(vec_predictor[static_cast<std::size_t>(int4_c)]) >> 8));
      int4_out_offset += 2;

      if (int4_out_offset >= int4_output_size) return;
    }

    if (sz_stream_pos + static_cast<std::size_t>(int4_block_data_size) > sz_data_end) return;
    for (std::size_t sz_i{0}; sz_i < vec_block_data.size(); ++sz_i)
      vec_block_data[sz_i] = span_bytes[sz_stream_pos++];

    std::int32_t int4_block_offset{0};
    while (int4_block_offset < int4_block_data_size) {
      for (std::int32_t int4_c{0}; int4_c < int4_channels; ++int4_c) {
        load_ima_adpcm_block(
            std::span<const std::uint8_t>{vec_block_data}.subspan(
                static_cast<std::size_t>(int4_block_offset), 4u),
            vec_index[static_cast<std::size_t>(int4_c)],
            vec_predictor[static_cast<std::size_t>(int4_c)], vec_decoded);

        std::int32_t int4_interleave_channel_offset{2 * int4_c};
        for (std::size_t sz_i{0}; sz_i < vec_decoded.size(); sz_i += 2u) {
          const std::int32_t int4_sample_offset{int4_interleave_channel_offset +
                                                static_cast<std::int32_t>(sz_i)};
          vec_interleave[static_cast<std::size_t>(int4_sample_offset)] = vec_decoded[sz_i];
          vec_interleave[static_cast<std::size_t>(int4_sample_offset) + 1u] = vec_decoded[sz_i + 1u];
          int4_interleave_channel_offset += 2 * (int4_channels - 1);
        }

        int4_block_offset += 4;
      }

      const std::int64_t int4_output_remaining{
          static_cast<std::int64_t>(int4_output_size) - int4_out_offset};
      const std::size_t sz_to_copy{static_cast<std::size_t>(
          std::min<std::int64_t>(int4_output_remaining,
                                 static_cast<std::int64_t>(vec_interleave.size())))};
      for (std::size_t sz_i{0}; sz_i < sz_to_copy; ++sz_i) vec_out.push_back(vec_interleave[sz_i]);

      int4_out_offset += 16 * int4_channels;

      if (int4_out_offset >= int4_output_size) return;
    }

    ++int4_current_block;
    if (int4_current_block >= int4_num_blocks) return;
  }
}

}

std::optional<decoded_sound> wav_try_parse(std::span<const std::uint8_t> span_bytes) {
  byte_reader reader{span_bytes, 0u};

  char chr_type[4]{};
  char chr_format[4]{};
  if (!reader.read_ascii4(chr_type)) return std::nullopt;
  if (!reader.block_type_is(std::string_view{chr_type, 4u}, "RIFF")) return std::nullopt;
  std::uint32_t u4_ignore{0};
  if (!reader.read_u32(u4_ignore)) return std::nullopt;
  if (!reader.read_ascii4(chr_format)) return std::nullopt;
  if (!reader.block_type_is(std::string_view{chr_format, 4u}, "WAVE")) return std::nullopt;

  std::int16_t int4_audio_type{0};
  std::int16_t int4_channels{-1};
  std::int32_t int4_sample_bits{-1};
  std::int32_t int4_sample_rate{-1};
  float f4_length_in_seconds{-1.0f};
  std::int32_t int4_uncompressed_size{-1};
  std::int16_t int4_block_align{-1};
  std::size_t sz_data_offset{0};
  std::int32_t int4_data_size{-1};

  while (reader.sz_pos < span_bytes.size()) {
    if ((reader.sz_pos & 1u) == 1u) {
      std::uint8_t u1_ignore{0};
      if (!reader.read_u8(u1_ignore)) return std::nullopt;
    }

    if (reader.sz_pos == span_bytes.size()) break;

    char chr_block_type[4]{};
    std::uint32_t u4_chunk_size{0};
    if (!reader.read_ascii4(chr_block_type)) return std::nullopt;
    if (!reader.read_u32(u4_chunk_size)) return std::nullopt;

    if (reader.block_type_is(std::string_view{chr_block_type, 4u}, "fmt ")) {
      std::int16_t int4_audio_format{0};
      if (!reader.read_i16(int4_audio_format)) return std::nullopt;
      int4_audio_type = int4_audio_format;

      if (int4_audio_format != kWaveTypePcm && int4_audio_format != kWaveTypeMsAdpcm &&
          int4_audio_format != kWaveTypeImaAdpcm)
        return std::nullopt;

      std::int32_t int4_byte_rate{0};
      std::int16_t int4_bits{0};
      if (!reader.read_i16(int4_channels)) return std::nullopt;
      if (!reader.read_i32(int4_sample_rate)) return std::nullopt;
      if (!reader.read_i32(int4_byte_rate)) return std::nullopt;
      if (!reader.read_i16(int4_block_align)) return std::nullopt;
      if (!reader.read_i16(int4_bits)) return std::nullopt;
      int4_sample_bits = int4_bits;

      const std::int32_t int4_denominator{int4_channels * int4_sample_rate * int4_sample_bits};
      if (int4_denominator != 0)
        f4_length_in_seconds =
            static_cast<float>(static_cast<std::int64_t>(span_bytes.size()) * 8) /
            static_cast<float>(int4_denominator);

      reader.sz_pos += static_cast<std::size_t>(u4_chunk_size - 16u);
    } else if (reader.block_type_is(std::string_view{chr_block_type, 4u}, "fact")) {
      if (!reader.read_i32(int4_uncompressed_size)) return std::nullopt;
      reader.sz_pos += static_cast<std::size_t>(u4_chunk_size - 4u);
    } else if (reader.block_type_is(std::string_view{chr_block_type, 4u}, "data")) {
      std::uint64_t u4_size{u4_chunk_size};
      if (static_cast<std::uint64_t>(reader.sz_pos) + u4_size > span_bytes.size())
        u4_size = span_bytes.size() - reader.sz_pos;
      sz_data_offset = reader.sz_pos;
      int4_data_size = static_cast<std::int32_t>(u4_size);
      reader.sz_pos += static_cast<std::size_t>(u4_size);
    } else {
      reader.sz_pos += static_cast<std::size_t>(u4_chunk_size);
    }
  }

  if (int4_audio_type != kWaveTypePcm) int4_sample_bits = 16;

  if (int4_channels != 1 && int4_channels != 2) return std::nullopt;

  decoded_sound snd_out{};
  snd_out.int4_channels = int4_channels;
  snd_out.int4_sample_bits = int4_sample_bits;
  snd_out.int4_sample_rate = int4_sample_rate;
  snd_out.f4_length_in_seconds = f4_length_in_seconds;

  if (int4_data_size < 0) return std::nullopt;

  if (int4_audio_type == kWaveTypeImaAdpcm) {
    if (int4_block_align <= 0) return std::nullopt;
    decode_ima_blocks(span_bytes, sz_data_offset, int4_data_size, int4_block_align,
                      int4_channels, int4_uncompressed_size, snd_out.vec_pcm);
    return snd_out;
  }

  if (int4_audio_type != kWaveTypePcm) return std::nullopt;

  enqueue_pcm_bytes(snd_out.vec_pcm,
                    span_bytes.subspan(sz_data_offset, static_cast<std::size_t>(int4_data_size)));
  return snd_out;
}

}
