// UPSTREAM: OpenRA.Mods.Common/FileFormats/WavReader.cs @7d57605 L20-305
// (全文)+ OpenRA.Mods.Common/AudioLoaders/WavLoader.cs L20-49(IsWave 嗅
// 探与 TryParseSound 的 catch 面)。
// RIFF/WAVE 解析:块循环(奇地址对齐衬垫、fmt/fact/data/LIST 及未知块
// 跳过、data 块尺寸超端按余量钳制);非 PCM 时 sampleBits 恒报 16;
// channels 限 1/2(NotSupportedException 消息逐字)。三解码路径:PCM 直
// 通(data 段切片)、IMA ADPCM(块级 predictor/index 状态 + 交错;
// outputSize = uncompressedSize × channels × 2,fact 缺失时
// uncompressedSize = -1 的提前截断怪癖照抄)、MS ADPCM(bpred/idelta/
// s1/s2 块头 + 高半字节恒左声道、idelta 下限 16)。
// 形态偏离(D74):上游返回惰性工厂(Func<Stream> + 两个
// ReadOnlyAdapterStream 的 Queue<byte>);C++ 物化为整段 PCM vector,
// 字节输出等价;TryParse 内的解码失败折入 false(上游在流读取时才抛,
// 仅坏数据可达)。
// [UPSTREAM continued] the whole of WavReader.cs plus WavLoader.cs L20-49 (the IsWave
// sniff and the TryParseSound catch surface). RIFF/WAVE parsing: the chunk
// loop (odd-address alignment padding; fmt/fact/data plus LIST and unknown
// chunks skipped; a data chunk size beyond EOF clamped to the remainder);
// sampleBits reports 16 for anything but PCM; channels limited to 1/2 (the
// NotSupportedException message verbatim). Three decode paths: PCM
// passthrough (a slice of the data chunk), IMA ADPCM (per-block
// predictor/index state plus interleaving; outputSize = uncompressedSize ×
// channels × 2, with the missing-fact uncompressedSize = -1
// early-truncation quirk kept), and MS ADPCM (the bpred/idelta/s1/s2 block
// header; the high nibble always the left channel; the idelta floor of
// 16). Shape deviation (D74): upstream returns a lazy factory (Func<Stream>
// + two ReadOnlyAdapterStreams enqueueing Queue<byte>); the C++ side
// materializes the whole PCM into a vector with equivalent byte output, and
// a decode failure inside TryParse folds into false (upstream throws later
// at stream read time — malformed data only).
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

/// WavReader.cs L22 的 WaveType(: short 底层;值域 = IsDefined 检查面)。
/// The WaveType of WavReader.cs L22 (short-backed; the values behind the
/// IsDefined check).
enum class WaveType : std::int16_t {
  Pcm = 0x1,
  MsAdpcm = 0x2,
  ImaAdpcm = 0x11,
};

/// WavReader.LoadSound 的出参面(WavFormat 的元数据属性)+ 解析内部量。
/// The out-parameter surface of WavReader.LoadSound (the WavFormat
/// metadata properties) plus the parse internals.
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

/// WavReader.LoadSound(L24-114):解析头与块表;false = 非 RIFF/WAVE。
/// 抛点:不支持压缩类型 / channels 逾 1-2(两条 NotSupportedException
/// 消息逐字)。
/// WavReader.LoadSound (L24-114): parses the header and chunk table;
/// false = not RIFF/WAVE. Throw points: unsupported compression types /
/// channels beyond 1-2 (both NotSupportedException messages verbatim).
bool LoadWavSound(std::span<const std::byte> vec_file, WavInfo& info);

/// 上游 result() 工厂 + 整流消费的物化等价:PCM 直通 / IMA / MS ADPCM。
/// The materialized equivalent of upstream's result() factory plus a full
/// stream read: PCM passthrough / IMA / MS ADPCM.
std::vector<std::byte> DecodeWavPcm(std::span<const std::byte> vec_file, const WavInfo& info);

/// WavLoader.IsWave(L20-28):RIFF/WAVE 魔数(短文件 = 上游 ReadASCII
/// 的 EndOfStream 被 TryParse 吞,此处等价返回 false)。
/// WavLoader.IsWave (L20-28): the RIFF/WAVE magic (a short file maps to
/// upstream's ReadASCII EndOfStream swallowed by TryParse; here the
/// equivalent false).
bool IsWave(std::span<const std::byte> vec_file);

/// WavLoader.TryParseSound(L30-48)+ GetPCMInputStream 整读:嗅探 +
/// 解析 + 解码一体;上游 catch 一切异常归 false,此处同面。
/// WavLoader.TryParseSound (L30-48) + a full GetPCMInputStream read:
/// sniff + parse + decode in one; upstream catches every exception into
/// false, and so does this.
bool TryParseWav(std::span<const std::byte> vec_file, WavInfo& info, std::vector<std::byte>& vec_pcm);

}  // namespace ora::fmt
