// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpRALoader.cs @7d57605 L17-102(全文)
// Red Alert 的 TMP 地形 tile 集:与 TmpTD 同构,头布局多 4 字节
// (imgStart@16、魔数 @26 u16 == 0x2c73)、indexStart@36。空 tile 语义
// 同 TD。
// The Red Alert TMP terrain tile set: isomorphic to TmpTD with a
// 4-byte-larger header layout (imgStart@16, the magic @26 u16 ==
// 0x2c73, indexStart@36). Empty-tile semantics as in TD.
#pragma once
import std;

#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// IsTmpRA(L49-61)。
bool IsTmpRA(std::span<const std::byte> vec_file);

/// TmpRALoader.TryParseSprite(L94-102)形态。
/// The TmpRALoader.TryParseSprite (L94-102) form.
bool TryParseTmpRA(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}  // namespace ora::fmt
