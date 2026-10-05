// UPSTREAM: OpenRA.Mods.Common/FileFormats/RLEZerosCompression.cs @7d57605 L16-37(全文)
// 零值游程编码(aka Format2):cmd==0 时后跟 count 字节零(Array.Clear
// 语义,destIndex 起清 count 字节);否则 cmd 本身即一个字面字节。
// ShpD2/ShpTS(Format3 扫描线)消费。越界(零段与字面量两分支)= 上游 IndexOutOfRange 的
// runtime_error 等价抛点(坏数据专用)。
// Run-length encoded sequences of zeros (aka Format2): cmd==0 is followed
// by a count of zeroed bytes (Array.Clear semantics, clearing count bytes
// from destIndex); otherwise cmd itself is one literal byte. Consumed by
// ShpD2 and ShpTS (Format3 scanlines). Out of bounds throws
// std::runtime_error at the equivalent point of upstream's
// IndexOutOfRangeException (malformed data only).
#pragma once
import std;

namespace ora::fmt {

namespace rle_zeros {

/// DecodeInto(L19-36)。
void DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest, std::size_t st_dest_index);

}  // namespace rle_zeros

}  // namespace ora::fmt
