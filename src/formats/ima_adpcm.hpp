// UPSTREAM: OpenRA.Mods.Common/FileFormats/ImaAdpcmReader.cs @7d57605 L16-87(全文)
// IMA ADPCM 单样点解码 + 4 字节组解码:IndexAdjust 8 项 / StepTable 89 项
// 逐值照搬;delta = StepTable[index]*b/4 + StepTable[index]/8 的两次整除
// 均为 C# int 除(向零截断);current/index 的饱和边界 32767/-32768 与
// 0/88 逐行保留。AudReader(IMA 流)与 WavReader 的 WavStreamImaAdpcm 消费。
// [UPSTREAM continued] the whole of ImaAdpcmReader.cs — the single-sample decode plus
// the 4-byte-group decode: the 8-entry IndexAdjust and the 89-entry
// StepTable copied value for value; delta = StepTable[index]*b/4 +
// StepTable[index]/8 keeps both C# int divisions (truncation towards
// zero); the saturation bounds of current/index (32767/-32768, 0/88) stay
// line by line. Consumed by AudReader (the IMA stream) and WavReader's
// WavStreamImaAdpcm.
#pragma once
import std;

namespace ora::fmt::ima_adpcm {

/// DecodeImaAdpcmSample(L33-57):b 低 3 位量级 + bit3 符号;index/current
/// 为跨样点状态(调用方持有)。
/// DecodeImaAdpcmSample (L33-57): the low 3 bits of b carry the magnitude
/// with bit 3 as the sign; index/current are cross-sample state held by
/// the caller.
std::int16_t DecodeImaAdpcmSample(std::uint8_t uint1_b, int& int4_index, int& int4_current);

/// LoadImaAdpcmSound(L59-63):currentSample 从 0 起的便捷重载。
/// LoadImaAdpcmSound (L59-63): the convenience overload starting
/// currentSample at 0.
void LoadImaAdpcmSound(std::span<const std::byte> vec_raw, int& int4_index, std::span<std::byte> vec_output);

/// LoadImaAdpcmSound(L65-85):每 4 个输出字节消费同一输入字节(两样点:
/// 低半字节先行),output 长度必须 = raw × 4(ArgumentException 等价抛)。
/// LoadImaAdpcmSound (L65-85): every 4 output bytes consume the same input
/// byte (two samples, the low nibble first); output's length must equal
/// raw × 4 (the ArgumentException's equivalent throw).
void LoadImaAdpcmSound(std::span<const std::byte> vec_raw, int& int4_index, int& int4_current_sample,
                       std::span<std::byte> vec_output);

}  // namespace ora::fmt::ima_adpcm
