// UPSTREAM: OpenRA.Mods.Common/FileFormats/WavReader.cs @b6fc03f +
// WavLoader.cs 的嗅探/try 面:逐句照抄;两个 ReadOnlyAdapterStream
// (WavStreamImaAdpcm/WavStreamMsAdpcm)的 BufferData 循环物化为整段
// 输出(Queue<byte> 逐字节 Enqueue 改 vector 追加,return true 改循环
// 退出),数据面逐字节等价。blockAlign 为 0 的除零 / 负 blockDataSize
// 的 new byte[] 在上游抛于工厂调用点,此处等价抛(仅坏数据可达)。
// [UPSTREAM continued] WavReader.cs + WavLoader.cs's sniff/try surface, copied
// statement by statement; the two ReadOnlyAdapterStream BufferData loops
// (WavStreamImaAdpcm/WavStreamMsAdpcm) materialize into one output (the
// byte-by-byte Queue.Enqueue becomes a vector append, return true becomes
// loop exit), byte-for-byte equivalent on the data plane. A zero
// blockAlign's division / a negative blockDataSize's new byte[] throw
// upstream at the factory call point; here they throw the equivalent
// (malformed data only).
#include "formats/wav_reader.hpp"

#include "formats/ima_adpcm.hpp"

namespace ora::fmt {
namespace {

bool TagIs(std::span<const std::byte> vec_tag, std::string_view str_ascii) {
  return vec_tag.size() == str_ascii.size() &&
         std::equal(str_ascii.begin(), str_ascii.end(), vec_tag.begin(),
                    [](char chr_c, std::byte byte_b) { return static_cast<std::byte>(chr_c) == byte_b; });
}

// ———— WavStreamMsAdpcm 的表与核(L207-302)————
// ———— The WavStreamMsAdpcm tables and core (L207-302) ————

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

  // Compute next Adaptive Scale Factor (ASF), saturating to lower bound of 16
  int2_idelta = static_cast<std::int16_t>(static_cast<std::uint16_t>((kAdaptationTable[int2_nibble] * int2_idelta) >> 8));
  if (int2_idelta < 16)
    int2_idelta = 16;

  return int2_s1;
}

}  // namespace

bool LoadWavSound(std::span<const std::byte> vec_file, WavInfo& info) {
  auto reader = SpanReader{vec_file};

  if (!TagIs(reader.ReadBytes(4), "RIFF"))
    return false;

  reader.ReadInt32();  // File-size
  if (!TagIs(reader.ReadBytes(4), "WAVE"))
    return false;

  auto enum_audio_type = WaveType{};
  auto int8_data_offset = std::int64_t{-1};
  auto int4_data_size = std::int32_t{-1};
  auto int4_uncompressed_size = std::int32_t{-1};
  auto int2_block_align = std::int16_t{-1};
  while (reader.Position() < reader.Length()) {
    if ((reader.Position() & 1) == 1)
      reader.ReadUInt8();  // Alignment

    if (reader.Position() == reader.Length())
      break;  // Break if we aligned with end of stream

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
      reader.ReadInt32();  // Byte Rate
      int2_block_align = static_cast<std::int16_t>(reader.ReadUInt16());
      info.int4_sample_bits = static_cast<std::int16_t>(reader.ReadUInt16());

      // (float)(s.Length * 8) / (channels * sampleRate * sampleBits):分母
      // 三连 int 乘按 C# unchecked 回绕。
      // (float)(s.Length * 8) / (channels * sampleRate * sampleBits): the
      // denominator's int triple product keeps C#'s unchecked wrap.
      const auto uint4_denominator = static_cast<std::uint32_t>(info.int2_channels) *
                                     static_cast<std::uint32_t>(info.int4_sample_rate) *
                                     static_cast<std::uint32_t>(info.int4_sample_bits);
      info.fp4_length_in_seconds =
          static_cast<float>(reader.Length() * 8) / static_cast<float>(uint4_denominator);
      reader.Skip(static_cast<std::int64_t>(uint4_chunk_size) - 16);  // Ignoring any optional extra params
    } else if (TagIs(vec_block_type, "fact")) {
      int4_uncompressed_size = reader.ReadInt32();
      reader.Skip(static_cast<std::int64_t>(uint4_chunk_size) - 4);  // Ignoring other formats than ADPCM, fact chunk not in standard PCM files
    } else if (TagIs(vec_block_type, "data")) {
      if (reader.Position() + static_cast<std::int64_t>(uint4_chunk_size) > reader.Length())
        uint4_chunk_size = static_cast<std::uint32_t>(reader.Length() - reader.Position());  // Handle defective data chunk size by assuming it's the remainder of the file

      int8_data_offset = reader.Position();
      int4_data_size = static_cast<std::int32_t>(uint4_chunk_size);
      reader.Skip(uint4_chunk_size);
    } else {
      // LIST / cue / 未知块跳过 | LIST / cue / unknown chunks skipped
      reader.Skip(uint4_chunk_size);
    }
  }

  // sampleBits refers to the output bitrate, which is always 16 for adpcm.
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
      // Data is already PCM format.(SegmentStream 切片直通)
      // Data is already PCM format. (the SegmentStream slice passthrough)
      reader.Seek(info.int8_data_offset);
      const auto vec_segment = reader.ReadBytes(info.int4_data_size);
      return std::vector<std::byte>(vec_segment.begin(), vec_segment.end());
    }

    case WaveType::ImaAdpcm: {
      // WavStreamImaAdpcm(L116-202):块级 predictor/index + 半字节交错;
      // outputSize 为负(fact 缺失)时首样点后即截。
      // WavStreamImaAdpcm (L116-202): per-block predictor/index with
      // nibble interleaving; a negative outputSize (missing fact) cuts
      // right after the first sample.
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
        // Decode each block of IMA ADPCM data
        // Each block starts with a initial state per-channel
        reader.ReadInto(arr_channel_data, 0, static_cast<std::size_t>(int4_channels) * 4);
        auto int4_cd = 0;
        for (auto int4_c = 0; int4_c < int4_channels; int4_c++) {
          arr4_predictor[int4_c] = static_cast<std::int16_t>(
              static_cast<int>(arr_channel_data[static_cast<std::size_t>(int4_cd)]) |
              (static_cast<int>(arr_channel_data[static_cast<std::size_t>(int4_cd) + 1]) << 8));
          int4_cd += 2;
          arr4_index[int4_c] = static_cast<std::uint8_t>(arr_channel_data[static_cast<std::size_t>(int4_cd++)]);
          int4_cd++;  // Unknown/Reserved

          // Output first sample from input
          vec_out.push_back(static_cast<std::byte>(arr4_predictor[int4_c]));
          vec_out.push_back(static_cast<std::byte>(arr4_predictor[int4_c] >> 8));
          int4_out_offset += 2;

          if (int4_out_offset >= int4_output_size)
            return vec_out;
        }

        // Decode and output remaining data in this block
        reader.ReadInto(vec_block_data, 0, vec_block_data.size());
        auto int4_block_offset = 0;
        while (int4_block_offset < int4_block_data_size) {
          for (auto int4_c = 0; int4_c < int4_channels; int4_c++) {
            // Decode 4 bytes (to 16 bytes of output) per channel
            ima_adpcm::LoadImaAdpcmSound(
                std::span<const std::byte>{vec_block_data}
                    .subspan(static_cast<std::size_t>(int4_block_offset), 4),
                arr4_index[int4_c], arr4_predictor[int4_c], arr_decoded);

            // Interleave output, one sample per channel
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
          // interleaveBuffer 长 = channels × 16(交错重组后的本组全声道样本)。
          // The interleaveBuffer's length is channels × 16 (this group's
          // full-channel samples after the interleave reshuffle).
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
      // WavStreamMsAdpcm(L205-303);格式文档
      // https://wiki.multimedia.cx/index.php/Microsoft_ADPCM
      // WavStreamMsAdpcm (L205-303); format docs
      // https://wiki.multimedia.cx/index.php/Microsoft_ADPCM
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

          // Decode the first nibble, this is always left channel
          fn_write_sample(DecodeNibble(static_cast<std::int16_t>((uint1_bytecode >> 4) & 0x0F), arr_bpred[0],
                                       arr_chan_idelta[0], arr_s1[0], arr_s2[0]));

          // Decode the second nibble, for stereo this will be the right channel
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
  // 上游 ReadASCII×2(跳 4 字节 size 字段)= 12 字节魔数面。
  // Upstream's ReadASCII×2 (skipping the 4-byte size field) = a 12-byte
  // magic surface.
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
    // Not a (supported) WAV(上游 catch 面照抄)。
    // Not a (supported) WAV (upstream's catch surface, kept).
    return false;
  }
}

}  // namespace ora::fmt
