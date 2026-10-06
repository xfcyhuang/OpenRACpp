// UPSTREAM: OpenRA.Game/ModData.cs @b6fc03f L111(ObjectCreator.GetLoader&lt;
//          ISpriteSequenceLoader&gt;(Manifest.SpriteSequenceFormat, "sequence"))+
//           OpenRA.Game/ObjectCreator.cs @b6fc03f L149-156
// SpriteSequenceFormat 名 → 序列加载器实例(Manifest 名分派;未知名抛
// InvalidOperationException 文本逐字)。
// The SpriteSequenceFormat name → the sequence-loader instance (Manifest
// name dispatch; an unknown name throws the InvalidOperationException text
// verbatim).
#pragma once
import std;

#include "gfx/sequence_set.hpp"

namespace ora::mods {

/// GetLoader&lt;ISpriteSequenceLoader&gt;(format, "sequence")的等价工厂。
/// The equivalent factory of GetLoader<ISpriteSequenceLoader>(format,
/// "sequence").
std::unique_ptr<gfx::ISpriteSequenceLoader> MakeSequenceLoader(const std::string& str_format);

}  // namespace ora::mods
