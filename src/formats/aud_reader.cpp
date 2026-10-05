// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/AudReader.cs @7d57605 + AudLoader.cs
// 的嗅探/try 面:逐句照抄;两解码流(ReadOnlyAdapterStream 的
// BufferData 循环)物化为整段输出 —— Queue<byte> 逐字节 Enqueue 改
// vector 追加,上游 return true(流尽)改循环退出,数据面逐字节等价。
// [UPSTREAM continued] AudReader.cs + AudLoader.cs's sniff/try surface, copied
// statement by statement; the two decode streams (the ReadOnlyAdapterStream
// BufferData loops) materialize into one output — the byte-by-byte
// Queue.Enqueue becomes a vector append and upstream's return true
// (stream exhausted) becomes loop exit; the data plane stays byte-for-byte
// equivalent.
#include "formats/aud_reader.hpp"

#include "formats/ima_adpcm.hpp"
#include "formats/westwood_compressed.hpp"

namespace ora::fmt {
namespace {

/// AudChunk.Read(AudReader.cs L36-49):8 字节块头;魔数失配逐字抛。
/// AudChunk.Read (AudReader.cs L36-49): the 8-byte chunk header; a magic
/// mismatch throws verbatim.
struct AudChunk {
  std::int32_t int4_compressed_size;
  std::int32_t int4_output_size;

  explicit AudChunk(SpanReader& reader)
      : int4_compressed_size(reader.ReadUInt16()), int4_output_size(reader.ReadUInt16()) {
    if (reader.ReadUInt32() != 0xdeaf) [[unlikely]]
      throw std::runtime_error("Chunk header is bogus");
  }
};

}  // namespace

bool LoadAudSound(std::span<const std::byte> vec_file, AudInfo& info) {
  auto reader = SpanReader{vec_file};
  info.int4_sample_rate = reader.ReadUInt16();
  info.int4_data_size = reader.ReadInt32();
  info.int4_output_size = reader.ReadInt32();
  const auto uint1_audio_flags = reader.ReadUInt8();
  info.int4_sample_bits = (uint1_audio_flags & 0x2) == 0 ? 8 : 16;
  info.int4_channels = (uint1_audio_flags & 0x1) == 0 ? 1 : 2;

  // (float)(outputSize * 8) / (channels * sampleBits * sampleRate):三段
  // int 乘积均按 C# unchecked 回绕语义复刻。
  // (float)(outputSize * 8) / (channels * sampleBits * sampleRate): all
  // three int products keep C#'s unchecked wrap semantics.
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
      // ImaAdpcmAudStream(L100-158):index/currentSample 跨块持久;
      // 每输入字节先低半字节,baseOffset < outputSize 才取高半字节(末字
      // 节可能只用一半)。
      // ImaAdpcmAudStream (L100-158): index/currentSample persist across
      // chunks; each input byte feeds the low nibble first, the high nibble
      // only while baseOffset < outputSize (the final byte may be half
      // used).
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
            /* possible that only half of the final byte is used! */
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
      // WestwoodCompressedAudStream(L160-203):outputBuffer 的
      // EnsureArraySize 为"增长 = 全新零数组"(new byte[n],旧内容丢弃;
      // 不增长时保留 —— 短写块的 OutputSize 尾段 = 上一块残留,上游可观测
      // 语义,照抄);sample 每块自 0x80 重起(Decode 局部量)。
      // WestwoodCompressedAudStream (L160-203): the outputBuffer's
      // EnsureArraySize means "growing = a fresh zeroed array" (new
      // byte[n] discarding the old; kept as-is when not growing — a short
      // decode block's OutputSize tail is the previous chunk's residue,
      // upstream's observable semantics, kept); sample restarts at 0x80
      // per chunk (a Decode local).
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
    // Not a supported AUD(上游 catch 面照抄)。
    // Not a supported AUD (upstream's catch surface, kept).
    return false;
  }
}

}  // namespace ora::fmt
