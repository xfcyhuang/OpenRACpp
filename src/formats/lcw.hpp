// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LCWCompression.cs @7d57605 L16-167(全文)
// Lempel-Castle-Welch 算法(aka Format80)。解码五 case 逐分支照抄:
//   case1 = 0x80|n 之后的 n 字节原样拷贝(n==0 即终止标记);
//   case2 = 回拷 ((i&0x70)>>4)+3 字节自 destIndex-rpos;
//   case3/5 = 自绝对/相对 srcIndex 拷 count 字节;case4 = 单字节重复。
// Encode = 上游"quick and dirty v2"(raw copy + RLE;命令 4 与终止符
// 0x80)。异常消息逐字(NotImplementedException 文本保留)。
// 形态适配:byte[] → span;MemoryStream → std::vector<std::byte>。
// The Lempel-Castle-Welch algorithm (aka Format80). The decoder's five
// cases are copied branch for branch: case1 copies n raw bytes following
// 0x80|n (n==0 is the terminator); case2 replicates ((i&0x70)>>4)+3 bytes
// from destIndex-rpos; case3/5 copy count bytes from the absolute/relative
// srcIndex; case4 repeats a single byte. Encode is the upstream "quick and
// dirty v2" (raw copy + RLE; command 4 and the 0x80 terminator). Exception
// messages verbatim (NotImplementedException texts kept). Shape
// adaptation: byte[] → span; MemoryStream → std::vector<std::byte>.
#pragma once
import std;

namespace ora::fmt {

namespace lcw {

/// DecodeInto(L21-65)。dest 写越界处:上游 case4/3/5 抛
/// IndexOutOfRangeException(case2 有显式提前返回),C++ 统一
/// runtime_error(坏数据专用路径)。返回写入的字节数。
/// DecodeInto (L21-65). Where a write would run past dest, upstream's
/// cases 4/3/5 throw IndexOutOfRangeException (case2 returns early
/// explicitly); C++ throws std::runtime_error uniformly (a malformed-data
/// only path). Returns the number of bytes written.
std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest,
                        std::int32_t int4_src_offset = 0, bool b_reverse = false);

/// Encode(L88-166):上游"quick and dirty LCW encoder version 2"(raw
/// copy + RLE,命令 4/终止符 0x80)。运行时消费者为 ShpTDSprite.Write
/// (Utility 面,Phase 8);本批落地以供 oracle 对拍。
/// Encode (L88-166): the upstream "quick and dirty LCW encoder version 2"
/// (raw copy + RLE, command 4 / the 0x80 terminator). The runtime consumer
/// is ShpTDSprite.Write (the Utility face, Phase 8); landed now for the
/// oracle differential.
std::vector<std::byte> Encode(std::span<const std::byte> vec_src);

}  // namespace lcw

}  // namespace ora::fmt
