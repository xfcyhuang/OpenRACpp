#include "formats/wav_reader.hpp"

#include "formats/ima_adpcm.hpp"

namespace ora::fmt {
namespace {

bool TagIs(std::span<const std::byte> vec_tag, std::string_view str_ascii) {
  return vec_tag.size() == str_ascii.size() &&
         std::equal(str_ascii.begin(), str_ascii.end(), vec_tag.begin(),
                    [](char chr_c, std::byte byte_b) { return static_cast<std::byte>(chr_c) == byte_b; });
}

constexpr int kAdaptationTable[] = {230, 230, 230, 230, 307, 409, 512, 614,
                                    768, 614, 512, 409, 307, 230, 230, 230};
constexpr int kAdaptCoeff1[] = {256, 512, 0, 192, 240, 460, 392};
constexpr int kAdaptCoeff2[] = {0, -256, 0, 64, 0, -208, -232};

std::int16_t DecodeNibble(std::int16_t int2_nibble, std::uint8_t uint1_bpred, std::int16_t& int2_idelta,
                          std::int16_t& int2_s1, std::int16_t& int2_s2) {
  if (uint1_bpred >= std::size(kAdaptCoeff1)) [[unlikely]]
    throw std::runtime_error("Index was outside the bounds of the array.");

  const auto int4_predict = (int2_s1 * kAdaptCoeff1[uint1_bpred] + int2_s2 * kAdaptCoeff2[uint1_bpred]) >> 8;

  const auto int4_twos_compliment = (int2_nibble & 0x8) > 0 ? int2_nibble - 0x10 : int2_nibble;

  int2_s2 = int2_s1;
  const auto int4_new_sample = int4_twos_compliment * int2_idelta + int4_predict;
  int2_s1 = std::clamp(int4_new_sample, -32768, 32767);

  int2_idelta = static_cast<std::int16_t>(static_cast<std::uint16_t>((kAdaptationTable[int2_nibble] * int2_idelta) >> 8));
  if (int2_idelta < 16)
    int2_idelta = 16;

  return int2_s1;
}

}

bool LoadWavSound(std::span<const std::byte> vec_file, WavInfo& info) {
  auto reader = SpanReader{vec_file};

  if (!TagIs(reader.ReadBytes(4), "RIFF"))
    return false;

  reader.ReadInt32();
  if (!TagIs(reader.ReadBytes(4), "WAVE"))
    return false;

  auto enum_audio_type = WaveType{};
  auto int8_data_offset = std::int64_t{-1};
  auto int4_data_size = std::int32_t{-1};
  auto int4_uncompressed_size = std::int32_t{-1};
  auto int2_block_align = std::int16_t{-1};
  while (reader.Position() < reader.Length()) {
    if ((reader.Position() & 1) == 1)
      reader.ReadUInt8();

    if (reader.Position() == reader.Length())
      break;

    const auto vec_block_type = reader.ReadBytes(4);
    auto uint4_chunk_size = reader.ReadUInt32();

    if (TagIs(vec_block_type, "fmt ")) {
      const auto int2_audio_format = static_cast<std::int16_t>(reader.ReadUInt16());
      enum_audio_type = static_cast<WaveType>(int2_audio_format);

      if (int2_audio_format != static_cast<std::int16_t>(WaveType::Pcm) &&
          int2_audio_format != static_cast<std::int16_t>(WaveType::MsAdpcm) &&
          int2_audio_format != static_cast<std::int16_t>(WaveType::ImaAdpcm))
        throw std::runtime_error(
            std::format("System.NotSupportedException: Compression type {} is not supported.", int2_audio_format));

      info.int2_channels = static_cast<std::int16_t>(reader.ReadUInt16());
      info.int4_sample_rate = reader.ReadInt32();
      reader.ReadInt32();
      int2_block_align = static_cast<std::int16_t>(reader.ReadUInt16());
      info.int4_sample_bits = static_cast<std::int16_t>(reader.ReadUInt16());

      const auto uint4_denominator = static_cast<std::uint32_t>(info.int2_channels) *
                                     static_cast<std::uint32_t>(info.int4_sample_rate) *
                                     static_cast<std::uint32_t>(info.int4_sample_bits);
      info.fp4_length_in_seconds =
          static_cast<float>(reader.Length() * 8) / static_cast<float>(uint4_denominator);
      reader.Skip(static_cast<std::int64_t>(uint4_chunk_size) - 16);
    } else if (TagIs(vec_block_type, "fact")) {
      int4_uncompressed_size = reader.ReadInt32();
      reader.Skip(static_cast<std::int64_t>(uint4_chunk_size) - 4);
    } else if (TagIs(vec_block_type, "data")) {
      if (reader.Position() + static_cast<std::int64_t>(uint4_chunk_size) > reader.Length())
        uint4_chunk_size = static_cast<std::uint32_t>(reader.Length() - reader.Position());

      int8_data_offset = reader.Position();
      int4_data_size = static_cast<std::int32_t>(uint4_chunk_size);
      reader.Skip(uint4_chunk_size);
    } else {

      reader.Skip(uint4_chunk_size);
    }
  }

  if (enum_audio_type != WaveType::Pcm)
    info.int4_sample_bits = 16;

  if (info.int2_channels != 1 && info.int2_channels != 2)
    throw std::runtime_error(std::format(
        "System.NotSupportedException: Expected 1 or 2 channels only for WAV file, received: {}", info.int2_channels));

  info.enum_audio_type = enum_audio_type;
  info.int8_data_offset = int8_data_offset;
  info.int4_data_size = int4_data_size;
  info.int4_uncompressed_size = int4_uncompressed_size;
  info.int2_block_align = int2_block_align;
  return true;
}

std::vector<std::byte> DecodeWavPcm(std::span<const std::byte> vec_file, const WavInfo& info) {
  auto reader = SpanReader{vec_file};

  if (info.int8_data_offset < 0 || info.int4_data_size < 0) [[unlikely]]
    throw std::runtime_error("WAV data segment is not present.");

  switch (info.enum_audio_type) {
    case WaveType::Pcm: {

      reader.Seek(info.int8_data_offset);
      const auto vec_segment = reader.ReadBytes(info.int4_data_size);
      return std::vector<std::byte>(vec_segment.begin(), vec_segment.end());
    }

    case WaveType::ImaAdpcm: {

      if (info.int2_block_align == 0) [[unlikely]]
        throw std::runtime_error("System.DivideByZeroException: blockAlign is zero.");

      const auto int4_channels = info.int2_channels;
      const auto int4_num_blocks = info.int4_data_size / info.int2_block_align;
      const auto int4_block_data_size = info.int2_block_align - int4_channels * 4;
      const auto int4_output_size = static_cast<std::int32_t>(static_cast<std::uint32_t>(info.int4_uncompressed_size) *
                                                              static_cast<std::uint32_t>(int4_channels) * 2u);
      if (int4_block_data_size < 0) [[unlikely]]
        throw std::runtime_error("System.OverflowException: negative IMA block size.");

      auto vec_out = std::vector<std::byte>{};
      auto int4_out_offset = 0;
      auto int4_current_block = 0;

      int arr4_predictor[2] = {0, 0};
      int arr4_index[2] = {0, 0};
      std::array<std::byte, 8> arr_channel_data{};
      std::vector<std::byte> vec_block_data(static_cast<std::size_t>(int4_block_data_size));
      std::array<std::byte, 16> arr_decoded{};
      std::array<std::byte, 32> arr_interleave_buffer{};

      reader.Seek(info.int8_data_offset);
      while (true) {

        reader.ReadInto(arr_channel_data, 0, static_cast<std::size_t>(int4_channels) * 4);
        auto int4_cd = 0;
        for (auto int4_c = 0; int4_c < int4_channels; int4_c++) {
          arr4_predictor[int4_c] = static_cast<std::int16_t>(
              static_cast<int>(arr_channel_data[static_cast<std::size_t>(int4_cd)]) |
              (static_cast<int>(arr_channel_data[static_cast<std::size_t>(int4_cd) + 1]) << 8));
          int4_cd += 2;
          arr4_index[int4_c] = static_cast<std::uint8_t>(arr_channel_data[static_cast<std::size_t>(int4_cd++)]);
          int4_cd++;

          vec_out.push_back(static_cast<std::byte>(arr4_predictor[int4_c]));
          vec_out.push_back(static_cast<std::byte>(arr4_predictor[int4_c] >> 8));
          int4_out_offset += 2;

          if (int4_out_offset >= int4_output_size)
            return vec_out;
        }

        reader.ReadInto(vec_block_data, 0, vec_block_data.size());
        auto int4_block_offset = 0;
        while (int4_block_offset < int4_block_data_size) {
          for (auto int4_c = 0; int4_c < int4_channels; int4_c++) {

            ima_adpcm::LoadImaAdpcmSound(
                std::span<const std::byte>{vec_block_data}
                    .subspan(static_cast<std::size_t>(int4_block_offset), 4),
                arr4_index[int4_c], arr4_predictor[int4_c], arr_decoded);

            auto int4_interleave_channel_offset = 2 * int4_c;
            for (auto int4_i = 0; int4_i < static_cast<int>(arr_decoded.size()); int4_i += 2) {
              const auto int4_interleave_sample_offset = int4_interleave_channel_offset + int4_i;
              arr_interleave_buffer[static_cast<std::size_t>(int4_interleave_sample_offset)] =
                  arr_decoded[static_cast<std::size_t>(int4_i)];
              arr_interleave_buffer[static_cast<std::size_t>(int4_interleave_sample_offset) + 1] =
                  arr_decoded[static_cast<std::size_t>(int4_i) + 1];
              int4_interleave_channel_offset += 2 * (int4_channels - 1);
            }

            int4_block_offset += 4;
          }

          const auto int4_output_remaining = int4_output_size - int4_out_offset;

          const auto int4_to_copy = std::min(int4_output_remaining, int4_channels * 16);
          for (auto int4_i = 0; int4_i < int4_to_copy; int4_i++)
            vec_out.push_back(arr_interleave_buffer[static_cast<std::size_t>(int4_i)]);

          int4_out_offset += 16 * int4_channels;

          if (int4_out_offset >= int4_output_size)
            return vec_out;
        }

        if (++int4_current_block >= int4_num_blocks)
          return vec_out;
      }
    }

    case WaveType::MsAdpcm: {

      if (info.int2_block_align == 0) [[unlikely]]
        throw std::runtime_error("System.DivideByZeroException: blockAlign is zero.");

      const auto int4_channels = info.int2_channels;
      const auto int4_block_data_size = info.int2_block_align - int4_channels * 7;
      const auto int4_num_blocks = info.int4_data_size / info.int2_block_align;
      if (int4_block_data_size < 0) [[unlikely]]
        throw std::runtime_error("System.OverflowException: negative MS ADPCM block size.");

      auto vec_out = std::vector<std::byte>{};
      auto int4_current_block = 0;

      std::array<std::uint8_t, 2> arr_bpred{};
      std::array<std::int16_t, 2> arr_chan_idelta{};
      std::array<std::int16_t, 2> arr_s1{};
      std::array<std::int16_t, 2> arr_s2{};
      std::vector<std::byte> vec_block_data(static_cast<std::size_t>(int4_block_data_size));

      const auto fn_write_sample = [&vec_out](std::int16_t int2_t) {
        vec_out.push_back(static_cast<std::byte>(int2_t));
        vec_out.push_back(static_cast<std::byte>(int2_t >> 8));
        return int2_t;
      };

      reader.Seek(info.int8_data_offset);
      while (true) {
        reader.ReadInto(std::as_writable_bytes(std::span{arr_bpred}).first(
                            static_cast<std::size_t>(int4_channels)),
                        0, static_cast<std::size_t>(int4_channels));
        reader.ReadInto(std::as_writable_bytes(std::span{arr_chan_idelta}).first(
                            static_cast<std::size_t>(int4_channels) * 2),
                        0, static_cast<std::size_t>(int4_channels) * 2);
        reader.ReadInto(std::as_writable_bytes(std::span{arr_s1}).first(static_cast<std::size_t>(int4_channels) * 2),
                        0, static_cast<std::size_t>(int4_channels) * 2);
        reader.ReadInto(std::as_writable_bytes(std::span{arr_s2}).first(static_cast<std::size_t>(int4_channels) * 2),
                        0, static_cast<std::size_t>(int4_channels) * 2);

        for (auto int4_c = 0; int4_c < int4_channels; int4_c++)
          arr_s2[static_cast<std::size_t>(int4_c)] = fn_write_sample(arr_s2[static_cast<std::size_t>(int4_c)]);

        for (auto int4_c = 0; int4_c < int4_channels; int4_c++)
          fn_write_sample(arr_s1[static_cast<std::size_t>(int4_c)]);

        const auto int4_channel_number = int4_channels > 1 ? 1 : 0;

        reader.ReadInto(vec_block_data, 0, vec_block_data.size());
        for (auto int4_blockindx = 0; int4_blockindx < int4_block_data_size; int4_blockindx++) {
          const auto uint1_bytecode = static_cast<std::uint8_t>(vec_block_data[static_cast<std::size_t>(int4_blockindx)]);

          fn_write_sample(DecodeNibble(static_cast<std::int16_t>((uint1_bytecode >> 4) & 0x0F), arr_bpred[0],
                                       arr_chan_idelta[0], arr_s1[0], arr_s2[0]));

          fn_write_sample(DecodeNibble(static_cast<std::int16_t>(uint1_bytecode & 0x0F),
                                       arr_bpred[static_cast<std::size_t>(int4_channel_number)],
                                       arr_chan_idelta[static_cast<std::size_t>(int4_channel_number)],
                                       arr_s1[static_cast<std::size_t>(int4_channel_number)],
                                       arr_s2[static_cast<std::size_t>(int4_channel_number)]));
        }

        if (++int4_current_block >= int4_num_blocks)
          return vec_out;
      }
    }
  }

  return {};
}

bool IsWave(std::span<const std::byte> vec_file) {

  return vec_file.size() >= 12 && TagIs(vec_file.first(4), "RIFF") && TagIs(vec_file.subspan(8, 4), "WAVE");
}

bool TryParseWav(std::span<const std::byte> vec_file, WavInfo& info, std::vector<std::byte>& vec_pcm) {
  try {
    if (!IsWave(vec_file))
      return false;
    if (!LoadWavSound(vec_file, info))
      return false;
    vec_pcm = DecodeWavPcm(vec_file, info);
    return true;
  } catch (...) {

    return false;
  }
}

}
