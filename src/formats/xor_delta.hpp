// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XORDeltaCompression.cs @b6fc03f L14-82(全文)
// 与既有数据异或合并的增量流(aka Format40)。六 case 逐分支照抄:
// case1/2 跳过;case3/5 逐字节 XOR;case4/6 单值 XOR。终止 = 0x80 0x00
// 0x0000(word 0)。SHPTD 的 XORPrev/XORLCW 帧引用它。
// 形态适配:byte[] → span;dest 写越界处统一 runtime_error(上游抛
// IndexOutOfRangeException,坏数据专用路径)。
// A delta stream XORed into existing data (aka Format40). The six cases
// are copied branch for branch: cases 1/2 skip; cases 3/5 XOR byte by
// byte; cases 4/6 XOR with a single value. Termination = 0x80 0x00 0x0000
// (a zero word). SHPTD's XORPrev/XORLCW frames reference it. Shape
// adaptation: byte[] → span; out-of-bounds dest writes throw
// std::runtime_error uniformly (upstream throws IndexOutOfRangeException;
// a malformed-data only path).
#pragma once
import std;

namespace ora::fmt {

namespace xor_delta {

/// DecodeInto(L16-82)。destIndex 从 0 起;返回处理到的 destIndex。
/// DecodeInto (L16-82). destIndex starts at 0; returns the resulting
/// destIndex.
std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest,
                        std::int32_t int4_src_offset);

}  // namespace xor_delta

}  // namespace ora::fmt
