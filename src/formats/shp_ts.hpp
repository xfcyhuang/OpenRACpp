// UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/ShpTSLoader.cs @7d57605 L17-163(全文)
// Tiberian Sun 的 SHP 图像:24 字节/帧头表(x/y/宽/高/格式/文件偏移),
// 奇数宽高向上取偶(半像素偏移防御,Offset 记录取整差);三种格式:
// Format3 = RLE0(Format2)压缩扫描线(每行 u16 前缀长);Format2 =
// 未压缩长度前缀扫描线;Format1/0 = 未压缩整宽行。判定循环容忍前导
// 零尺寸伪帧(IsShpTS 的 do-while 逐行照抄)。
// The Tiberian Sun SHP image: a 24-byte-per-frame header table (x/y/
// width/height/format/file offset) with odd widths and heights rounded up
// (the half-pixel-offset defense; Offset records the rounding delta);
// three formats: Format3 = RLE0 (Format2) compressed scanlines (each row
// carrying a u16 length prefix); Format2 = uncompressed length-prefixed
// scanlines; Formats 1/0 = an uncompressed full-width row. The probe
// loop tolerates leading zero-sized bogus frames (IsShpTS's do-while
// copied line by line).
#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// IsShpTS(L105-140)。
bool IsShpTS(std::span<const std::byte> vec_file);

/// ShpTSLoader.TryParseSprite(L155-163)形态。
/// The ShpTSLoader.TryParseSprite (L155-163) form.
bool TryParseShpTS(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}  // namespace ora::fmt
