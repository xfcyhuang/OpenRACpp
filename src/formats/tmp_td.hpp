// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpTDLoader.cs @b6fc03f L17-99(全文)
// Command & Conquer TD 的 TMP 地形 tile 集:头(w/h/imgStart/indexEnd/
// indexStart)+ 单字节 tile 索引(255 = 空 tile)。魔数:@16 u32 == 0
// 且 @20 u32 == 0x0D1AFFFF。空 tile 的 Data = 空、Size = (0,0)、
// FrameSize = tile 尺寸。
// The Command & Conquer TD TMP terrain tile set: the header (w/h/
// imgStart/indexEnd/indexStart) + a one-byte tile index (255 = an empty
// tile). Magic: @16 u32 == 0 and @20 u32 == 0x0D1AFFFF. An empty tile's
// Data is empty, its Size (0,0), and its FrameSize the tile size.
#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// IsTmpTD(L49-61)。
bool IsTmpTD(std::span<const std::byte> vec_file);

/// TmpTDLoader.TryParseSprite(L91-99)形态。
/// The TmpTDLoader.TryParseSprite (L91-99) form.
bool TryParseTmpTD(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}  // namespace ora::fmt
