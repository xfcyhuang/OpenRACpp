// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpTSLoader.cs @7d57605 L17-200(全文)
// Tiberian Sun 的 TMP 地形 tile 集:菱形 tile(UnpackTileData:行宽自 4
// 起 +4/-4 递变,展开进矩形帧)+ 悬崖附加数据(flags&1 时 bounds 与主
// 帧联合、extra 两层主/深度回填)+ 深度通道作为第二组帧(tiles[k+
// stride] = TmpTSDepthFrame,共享 parent 的 DepthData/Size/Offset)。
// 判定:tileW*tileH/2+52 出现于首非空 tile 头 @+12。
// The Tiberian Sun TMP terrain tile set: diamond tiles (UnpackTileData:
// the row width steps +4/-4 from 4, unpacked into rectangular frames) +
// cliff extra data (with flags&1 the bounds union with the main frame
// and a two-pass main/depth extra refill) + the depth channel exposed as
// a second frame set (tiles[k+stride] = TmpTSDepthFrame sharing the
// parent's DepthData/Size/Offset). Probe: tileW*tileH/2+52 appears at
// @+12 of the first non-empty tile header.
#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// IsTmpTS(L146-167)。
bool IsTmpTS(std::span<const std::byte> vec_file);

/// TmpTSLoader.TryParseSprite(L192-200)形态。
/// The TmpTSLoader.TryParseSprite (L192-200) form.
bool TryParseTmpTS(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}  // namespace ora::fmt
