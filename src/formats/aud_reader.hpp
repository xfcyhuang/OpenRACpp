// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/AudReader.cs @7d57605 L20-205(全文)
// + OpenRA.Mods.Cnc/AudioLoaders/AudLoader.cs L20-73(IsAud 嗅探与
// TryParseSound 的 catch 面)。
// Westwood .aud 音频:12 字节头(sampleRate/dataSize/outputSize/flags/
// format)+ 块序列(每块 8 字节头:compressedSize u16、outputSize u16、
// 0xdeaf 魔数)。两解码路径:IMA ADPCM(DecodeImaAdpcmSample 逐样点,
// index/currentSample 跨块持久,outputSize 奇数时末半字节按
// baseOffset < outputSize 截)与 Westwood 压缩(块级解码,
// outputBuffer 只增复用 —— 解码短写时 OutputSize 尾段为上一块残留,
// 上游 EnsureArraySize 语义照抄)。
// 形态偏离(D74):上游 LoadSound 返回惰性工厂(Func<Stream> +
// ReadOnlyAdapterStream 逐块 Queue<byte>);C++ 侧物化为整段 PCM
// vector(消费端 Sound/OpenAL 批次本就整读)。字节序输出等价。
// [UPSTREAM continued] the whole of AudReader.cs plus the AudLoader.cs L20-73 sniff
// and TryParseSound catch surface. Westwood .aud audio: a 12-byte header
// (sampleRate/dataSize/outputSize/flags/format) followed by chunks (each
// with an 8-byte header: compressedSize u16, outputSize u16, the 0xdeaf
// magic). Two decode paths: IMA ADPCM (DecodeImaAdpcmSample per sample
// with index/currentSample persisting across chunks, an odd outputSize
// truncating the final half-nibble via baseOffset < outputSize) and
// Westwood compressed (chunk-level decoding with a grow-only output
// buffer — a short decode leaves the tail of OutputSize holding the
// previous chunk's residue, upstream's EnsureArraySize semantics kept).
// Shape deviation (D74): upstream's LoadSound returns a lazy factory
// (Func<Stream> + ReadOnlyAdapterStream enqueuing byte by byte per
// chunk); the C++ side materializes the whole PCM into a vector (the
// Sound/OpenAL consumer batch reads it whole anyway). The byte output is
// equivalent.
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

/// AudReader.cs L27-31 的 SoundFormat(值域 = IsDefined 检查面)。
/// The SoundFormat of AudReader.cs L27-31 (the values behind the
/// IsDefined check).
enum class AudSoundFormat : std::uint8_t {
  WestwoodCompressed = 1,
  ImaAdpcm = 99,
};

/// AudReader.LoadSound 的出参面(AudFormat 的元数据属性)。
/// The out-parameter surface of AudReader.LoadSound (the AudFormat
/// metadata properties).
struct AudInfo {
  std::int32_t int4_sample_rate = 0;
  std::int32_t int4_sample_bits = 0;
  std::int32_t int4_channels = 0;
  float fp4_length_in_seconds = 0.0f;

  // 解码所需的解析内部量(上游闭包捕获;dataOffset 起的 segmentLength
  // = 文件尾)。
  // Parse internals needed for decoding (captured by upstream's closure;
  // the segment from dataOffset runs to end of file).
  AudSoundFormat enum_format{};
  std::int64_t int8_data_offset = 0;
  std::int32_t int4_data_size = 0;
  std::int32_t int4_output_size = 0;
};

/// AudReader.LoadSound(L53-98):解析头;返回 false = readFormat 未知。
/// 抛点:压缩块头的 0xdeaf 魔数失配("Chunk header is bogus" 逐字)。
/// AudReader.LoadSound (L53-98): parses the header; false = an unknown
/// readFormat. Throw points: the chunk header's 0xdeaf magic mismatch
/// ("Chunk header is bogus" verbatim).
bool LoadAudSound(std::span<const std::byte> vec_file, AudInfo& info);

/// 上游 result() 工厂 + 整流消费的物化等价:按格式解码全量 PCM。
/// The materialized equivalent of upstream's result() factory plus a full
/// stream read: decodes the whole PCM per the format.
std::vector<std::byte> DecodeAudPcm(std::span<const std::byte> vec_file, const AudInfo& info);

/// AudLoader.IsAud(L20-28):字节 @11 ∈ {1, 99}(短文件 = 上游
/// ReadByte 的 -1,返回 false 不抛)。
/// AudLoader.IsAud (L20-28): byte @11 in {1, 99} (a short file maps to
/// upstream ReadByte's -1: false, no throw).
bool IsAud(std::span<const std::byte> vec_file);

/// AudLoader.TryParseSound(L30-47)+ GetPCMInputStream 整读:嗅探 +
/// 解析 + 解码一体;上游 catch 一切异常归 false,此处同面。
/// AudLoader.TryParseSound (L30-47) + a full GetPCMInputStream read:
/// sniff + parse + decode in one; upstream catches every exception into
/// false, and so does this.
bool TryParseAud(std::span<const std::byte> vec_file, AudInfo& info, std::vector<std::byte>& vec_pcm);

}  // namespace ora::fmt
