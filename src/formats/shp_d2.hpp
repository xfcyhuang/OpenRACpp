// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpD2Loader.cs @7d57605 L17-172(全文)
// Dune II 的 SHP 图像:帧头(flags/宽/高/数据长)+ 可选调色板查表
// (PaletteTable/VariableLengthTable;无表时默认 256 项恒等表改四项)+
// 可选 LCW 预解压 + RLE0(Format2)扫描线。帧偏移表支持 2/4 字节两型
// (首偏移第三字节非零 = 2 字节型)。判定三重(帧数/末偏移 + 2 == 文件
// 长/首帧 flags ∈ {≤3, 5})逐行照抄。
// The Dune II SHP image: a frame header (flags/width/height/data length)
// + an optional palette lookup table (PaletteTable/VariableLengthTable;
// without one, a 256-entry identity table with four entries rewritten) +
// an optional LCW pre-decode + RLE0 (Format2) scanlines. The frame
// offset table comes in 2- and 4-byte flavors (a non-zero third byte of
// the first offset means the 2-byte flavor). The three-part probe (image
// count / final offset + 2 == file length / first frame's flags ∈ {≤3,
// 5}) is copied line by line.
#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// IsShpD2(L92-119)。
bool IsShpD2(std::span<const std::byte> vec_file);

/// ShpD2Loader.TryParseSprite(L158-172)形态。
/// The ShpD2Loader.TryParseSprite (L158-172) form.
bool TryParseShpD2(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}  // namespace ora::fmt
