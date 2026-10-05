// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LZOCompression.cs @7d57605 L49-291(minilzo 2.06 的 C# 移植,逐控制流照抄)
// LZO1X 解压(minilzo 子集)。上游由 Frank Razenberg 自 minilzo 源码
// 预处理输出机械移植(goto 语义差异以 gt* 布尔改写);C++ 侧原生支持
// goto,控制流与标签按 C# 版逐分支对照落地(含 MatchNext 后的双重读
// 等移植行为 —— 与 C# 运行事实一致优先)。
// 形态适配:unsafe byte* → uint8_t*;*(uint*)p 未对齐读写 → memcpy
// (等价、无 UB);wrkmem 参数未用(上游亦未用,不移植)。
// 消费方:TS mix 文件、VQA/WSA 视频、Gen2 地图导入(后两者随后续批)。
// LZO1X decompression (the minilzo subset). Upstream is Frank Razenberg's
// mechanical port of the preprocessed minilzo 2.06 source (goto-semantics
// differences rewritten with gt* booleans); C++ natively supports goto, so
// the control flow and labels land branch-for-branch against the C#
// version (including ported behaviors like the double read after
// MatchNext — matching the C# runtime facts first). Shape adaptation:
// unsafe byte* → uint8_t*; the unaligned *(uint*)p accesses → memcpy
// (equivalent, no UB); the unused wrkmem parameter stays unported
// (upstream never uses it). Consumers: TS mix files, VQA/WSA video, and
// the Gen2 map import (the latter with later batches).
#pragma once
import std;

namespace ora::fmt {

namespace lzo {

/// DecodeInto(L281-291 的托管包装形态)。destLength 输入忽略(上游
/// ref 但仅写出);返回 LZO1xDecompress 的返回值(0 成功;-4 输入有
/// 剩余;-8 输出未耗尽输入)。
/// DecodeInto (the managed wrapper form of L281-291). destLength is
/// write-only (upstream's ref is only assigned); returns
/// LZO1xDecompress's value (0 success; -4 input remaining; -8 input
/// exhausted early).
std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::size_t st_src_offset, std::size_t st_src_length,
                        std::span<std::byte> vec_dest, std::size_t st_dest_offset, std::uint32_t& uint4_dest_length);

}  // namespace lzo

}  // namespace ora::fmt
