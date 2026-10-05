// UPSTREAM: OpenRA.Mods.Cnc/AudioLoaders/VocLoader.cs @b6fc03f L19-394(全文逐语义)
// Creative Voice File(.voc):26 字节头(20 字节描述 + datablockOffset u16 +
// version u16 + id u16,id = ~version + 0x1234 校验)+ 块序列(1 字节 code +
// 3 字节 LE 长度)。声音块(code 1)携 freqDiv/codec 与绝对数据偏移;前一块
// 为附加信息块(code 8)时其采样率覆盖本块并自列表移除(struct 按值相等
// Remove 的首例语义照抄);静默块(code 3)/循环块(code 6/7)入列表但读取
// 端只消费 code 1/9 块 —— 静默不产样点(上游 UpdateBlockIfNeeded 的可观测
// 语义);code 9 走 "Unhandled code in voc file" 抛(上游 default 分支)。
// 采样率 = 256-除数公式(a5/a6 → 11025、d2/d3 → 22050 特例;256 为非法除数;
// code 8 的 16 位除数走 256000000/(65536-freqDiv),其 65536 非法检查对
// u16 读数恒假 —— 上游不可达检查照抄)。混合采样率抛 "Voc file contains
// chunks with different sample rate"(code 3 不参与校验)。
// 形态适配(D93 同族):Stream → SpanReader;VocStream 惰性流 → 整段 PCM
// 物化(与 D74 的 aud 同形态);EndOfStreamException 的块尾净结束 =
// Position 检查。
// [UPSTREAM continued] the whole of VocLoader.cs L19-394, verbatim
// semantics. The Creative Voice File (.voc): a 26-byte header (a 20-byte
// description + datablockOffset u16 + version u16 + id u16, with the
// id = ~version + 0x1234 check) followed by the block sequence (a 1-byte
// code + a 3-byte LE length). Sound blocks (code 1) carry freqDiv/codec and
// an absolute data offset; when the previous block is an extra-info block
// (code 8) its sample rate overrides this block's and the block removes
// itself from the list (the first-occurrence Remove-by-struct-value
// semantics kept); silence (code 3) and repeat blocks (code 6/7) enter the
// list but the reader consumes code 1/9 blocks only — silence produces no
// samples (the observable semantics of upstream's UpdateBlockIfNeeded);
// code 9 takes the "Unhandled code in voc file" throw (upstream's default
// branch). The sample rate = the 256-divisor formula (a5/a6 → 11025 and
// d2/d3 → 22050 special cases; 256 the illegal divisor; code 8's 16-bit
// divisor goes through 256000000/(65536-freqDiv), whose illegal-65536 check
// is always false for a u16 read — upstream's unreachable check kept);
// mixed sample rates throw "Voc file contains chunks with different sample
// rate" (code 3 exempt from the check). Shape adaptations (the D93 family):
// Stream → SpanReader; VocStream's lazy stream → the whole PCM materialized
// (the same shape as aud's D74); EndOfStreamException's clean block-tail
// ending = a Position check.
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

/// VocFormat 的解析内部量(VocBlock 家族;Offset 为文件内绝对字节偏移)。
/// The parse internals of VocFormat (the VocBlock family; Offset is the
/// absolute in-file byte offset).
struct VocSampleBlock {
  std::int32_t int4_rate = 0;
  std::int32_t int4_samples = 0;
  std::int64_t int8_offset = 0;

  constexpr bool operator==(const VocSampleBlock&) const = default;
};

struct VocLoopBlock {
  std::int32_t int4_count = 0;
  constexpr bool operator==(const VocLoopBlock&) const = default;
};

struct VocBlock {
  std::int32_t int4_code = 0;
  std::int32_t int4_length = 0;
  VocSampleBlock block_sample{};
  VocLoopBlock block_loop{};

  /// 上游 List&lt;VocBlock&gt;.Remove(VocBlock) 的 EqualityComparer 默认 =
  /// 逐字段按值相等。
  /// Upstream List<VocBlock>.Remove(VocBlock) under the default
  /// EqualityComparer = field-by-field value equality.
  constexpr bool operator==(const VocBlock&) const = default;
};

/// VocFormat 的元数据面(SampleRate/LengthInSeconds 的解析产物)。
/// The metadata surface of VocFormat (the parse product behind
/// SampleRate/LengthInSeconds).
struct VocInfo {
  std::int32_t int4_sample_rate = 0;   // 8 位 mono | 8-bit mono
  std::int32_t int4_sample_bits = 8;
  std::int32_t int4_channels = 1;
  float fp4_length_in_seconds = 0.0f;  // totalSamples / SampleRate | totalSamples / SampleRate
  std::int32_t int4_total_samples = 0;
  std::vector<VocBlock> vec_blocks;
};

/// VocFormat 构造(CheckVocHeader + Preload):头校验失败/块结构非法抛
/// std::runtime_error(消息与上游 InvalidDataException 逐字);字节块尾的
/// 净结束(上游 EndOfStreamException break)不抛。
/// The VocFormat construction (CheckVocHeader + Preload): header-check
/// failures / malformed blocks throw std::runtime_error (messages verbatim
/// from upstream's InvalidDataException); the clean ending at the byte tail
/// (upstream's EndOfStreamException break) does not throw.
void LoadVoc(std::span<const std::byte> vec_file, VocInfo& info);

/// VocFormat.Read + VocStream 整读的物化等价:按列表序拼接 code 1/9 块的
/// 样点字节(共 totalSamples 字节;静默/循环块零贡献)。
/// The materialized equivalent of VocFormat.Read plus a whole VocStream
/// read: concatenates the sample bytes of the code 1/9 blocks in list order
/// (totalSamples bytes; silence/repeat blocks contribute nothing).
std::vector<std::byte> DecodeVocPcm(std::span<const std::byte> vec_file, const VocInfo& info);

/// VocLoader.TryParseSound(L20-35)+ GetPCMInputStream 整读:解析 + 解码
/// 一体;上游 catch 一切异常归 false,此处同面。
/// VocLoader.TryParseSound (L20-35) + a full GetPCMInputStream read: parse
/// and decode in one; upstream catches every exception into false, and so
/// does this.
bool TryParseVoc(std::span<const std::byte> vec_file, VocInfo& info, std::vector<std::byte>& vec_pcm);

}  // namespace ora::fmt
