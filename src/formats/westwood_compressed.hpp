// UPSTREAM: OpenRA.Mods.Common/FileFormats/WestwoodCompressedReader.cs
// @7d57605 L18-86(全文)
// Westwood 块压缩音频解码( AUD 的 WS 编码):五分支按命令字节高 2 位
// 分派 —— 00:每码字节 4 个 2 位差分(±2/±1);01:每码字节 2 个 4 位
// 差分(±9..±8 表);10 且 count&0x20:单样点跳变((sbyte)(count<<3)>>3
// 的符号扩展);10:count+1 个字面字节(样点取末字节);11:count+1 个
// 当前样点重复。sample 初值 0x80,差分后按 byte 域饱和。input == output
// 长度时直通拷贝。AudReader 的 WestwoodCompressedAudStream 消费;越界写
// = 上游 IndexOutOfRange 的等价抛点(坏数据专用)。
// [UPSTREAM continued] the whole of WestwoodCompressedReader.cs — the Westwood
// block-compressed audio decoder (the AUD "WS" coding): five branches on
// the top 2 bits of the command byte — 00: four 2-bit deltas per code
// byte (±2/±1); 01: two 4-bit deltas per code byte (the ±9..±8 table);
// 10 with count&0x20: a single-sample jump (the sign extension of
// (sbyte)(count<<3)>>3); 10: count+1 literal bytes (the sample takes the
// last byte); 11: count+1 repeats of the current sample. The sample
// starts at 0x80 and saturates to the byte domain after each delta. An
// input of the same length as output passes straight through. Consumed by
// AudReader's WestwoodCompressedAudStream; out-of-bounds writes throw at
// the equivalent points of upstream's IndexOutOfRangeException (malformed
// data only).
#pragma once
import std;

namespace ora::fmt::westwood_compressed {

/// DecodeWestwoodCompressedSample(L23-84)。
/// DecodeWestwoodCompressedSample (L23-84).
void DecodeWestwoodCompressedSample(std::span<const std::byte> vec_input, std::span<std::byte> vec_output);

}  // namespace ora::fmt::westwood_compressed
