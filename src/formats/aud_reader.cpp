#include "formats/aud_reader.hpp"

#include "formats/ima_adpcm.hpp"
#include "formats/westwood_compressed.hpp"

namespace ora::fmt {
namespace {

struct AudChunk {
  std::int32_t int4_compressed_size;
  std::int32_t int4_output_size;

  explicit AudChunk(SpanReader& reader)
      : int4_compressed_size(reader.ReadUInt16()), int4_output_size(reader.ReadUInt16()) {
    if (reader.ReadUInt32() != 0xdeaf) [[unlikely]]
      throw std::runtime_error("Chunk header is bogus");
  }
};

}

bool LoadAudSound(std::span<const std::byte> vec_file, AudInfo& info) {
  auto reader = SpanReader{vec_file};
  info.int4_sample_rate = reader.ReadUInt16();
  info.int4_data_size = reader.ReadInt32();
  info.int4_output_size = reader.ReadInt32();
  const auto uint1_audio_flags = reader.ReadUInt8();
  info.int4_sample_bits = (uint1_audio_flags & 0x2) == 0 ? 8 : 16;
  info.int4_channels = (uint1_audio_flags & 0x1) == 0 ? 1 : 2;

  const auto uint4_numerator = static_cast<std::uint32_t>(info.int4_output_size) * 8u;
  const auto uint4_denominator = static_cast<std::uint32_t>(info.int4_channels) *
                                 static_cast<std::uint32_t>(info.int4_sample_bits) *
                                 static_cast<std::uint32_t>(info.int4_sample_rate);
  info.fp4_length_in_seconds = static_cast<float>(uint4_numerator) / static_cast<float>(uint4_denominator);

  const auto uint1_read_format = reader.ReadUInt8();
  if (uint1_read_format != static_cast<std::uint8_t>(AudSoundFormat::WestwoodCompressed) &&
      uint1_read_format != static_cast<std::uint8_t>(AudSoundFormat::ImaAdpcm))
    return false;
  info.enum_format = static_cast<AudSoundFormat>(uint1_read_format);

  info.int8_data_offset = reader.Position();
  return true;
}

std::vector<std::byte> DecodeAudPcm(std::span<const std::byte> vec_file, const AudInfo& info) {
  auto vec_out = std::vector<std::byte>{};
  auto reader = SpanReader{vec_file};
  reader.Seek(info.int8_data_offset);
  auto int4_data_size = info.int4_data_size;

  switch (info.enum_format) {
    case AudSoundFormat::ImaAdpcm: {

      auto int4_index = 0;
      auto int4_current_sample = 0;
      auto int4_base_offset = 0;

      while (int4_data_size > 0) {
        const auto chunk = AudChunk{reader};
        const auto vec_input = reader.ReadBytes(chunk.int4_compressed_size);

        for (auto int4_n = 0; int4_n < chunk.int4_compressed_size; int4_n++) {
          const auto uint1_b = static_cast<std::uint8_t>(vec_input[static_cast<std::size_t>(int4_n)]);

          auto int2_t = ima_adpcm::DecodeImaAdpcmSample(uint1_b, int4_index, int4_current_sample);
          vec_out.push_back(static_cast<std::byte>(int2_t));
          vec_out.push_back(static_cast<std::byte>(int2_t >> 8));
          int4_base_offset += 2;

          if (int4_base_offset < info.int4_output_size) {

            int2_t = ima_adpcm::DecodeImaAdpcmSample(static_cast<std::uint8_t>(uint1_b >> 4), int4_index,
                                                     int4_current_sample);
            vec_out.push_back(static_cast<std::byte>(int2_t));
            vec_out.push_back(static_cast<std::byte>(int2_t >> 8));
            int4_base_offset += 2;
          }
        }

        int4_data_size -= 8 + chunk.int4_compressed_size;
      }

      break;
    }

    case AudSoundFormat::WestwoodCompressed: {

      auto vec_input_buffer = std::vector<std::byte>{};
      auto vec_output_buffer = std::vector<std::byte>{};

      while (int4_data_size > 0) {
        const auto chunk = AudChunk{reader};

        if (vec_input_buffer.size() < static_cast<std::size_t>(chunk.int4_compressed_size))
          vec_input_buffer.assign(static_cast<std::size_t>(chunk.int4_compressed_size), std::byte{0});
        if (vec_output_buffer.size() < static_cast<std::size_t>(chunk.int4_output_size))
          vec_output_buffer.assign(static_cast<std::size_t>(chunk.int4_output_size), std::byte{0});

        reader.ReadInto(vec_input_buffer, 0, static_cast<std::size_t>(chunk.int4_compressed_size));
        westwood_compressed::DecodeWestwoodCompressedSample(
            std::span<const std::byte>{vec_input_buffer}.first(static_cast<std::size_t>(chunk.int4_compressed_size)),
            std::span<std::byte>{vec_output_buffer}.first(static_cast<std::size_t>(chunk.int4_output_size)));

        const auto st_output_size = static_cast<std::size_t>(chunk.int4_output_size);
        for (std::size_t st_i = 0; st_i < st_output_size; st_i++)
          vec_out.push_back(vec_output_buffer[st_i]);

        int4_data_size -= 8 + chunk.int4_compressed_size;
      }

      break;
    }
  }

  return vec_out;
}

bool IsAud(std::span<const std::byte> vec_file) {
  if (vec_file.size() <= 11)
    return false;
  const auto uint1_read_format = static_cast<std::uint8_t>(vec_file[11]);
  return uint1_read_format == static_cast<std::uint8_t>(AudSoundFormat::ImaAdpcm) ||
         uint1_read_format == static_cast<std::uint8_t>(AudSoundFormat::WestwoodCompressed);
}

bool TryParseAud(std::span<const std::byte> vec_file, AudInfo& info, std::vector<std::byte>& vec_pcm) {
  try {
    if (!IsAud(vec_file))
      return false;
    if (!LoadAudSound(vec_file, info))
      return false;
    vec_pcm = DecodeAudPcm(vec_file, info);
    return true;
  } catch (...) {

    return false;
  }
}

}
