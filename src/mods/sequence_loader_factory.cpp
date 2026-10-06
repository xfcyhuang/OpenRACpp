// UPSTREAM: OpenRA.Game/ModData.cs @b6fc03f L111 + OpenRA.Game/ObjectCreator.cs
//          @b6fc03f L149-156(sequence_loader_factory.hpp 的实现)
//          Implementation of sequence_loader_factory.hpp.
import std;
#include "mods/sequence_loader_factory.hpp"

#include "mods/classic_sprite_sequence.hpp"
#include "mods/classic_tileset_specific_sprite_sequence.hpp"
#include "mods/d2k_sprite_sequence.hpp"
#include "mods/default_sprite_sequence.hpp"
#include "mods/tileset_specific_sprite_sequence.hpp"

namespace ora::mods {

std::unique_ptr<gfx::ISpriteSequenceLoader> MakeSequenceLoader(const std::string& str_format) {
  // FindType(format + "Loader") 的已知集(mods 全部 SpriteSequenceFormat 值)
  // The known set of FindType(format + "Loader") (every mods
  // SpriteSequenceFormat value).
  if (str_format == "DefaultSpriteSequence")
    return std::make_unique<DefaultSpriteSequenceLoader>();
  if (str_format == "TilesetSpecificSpriteSequence")
    return std::make_unique<TilesetSpecificSpriteSequenceLoader>();
  if (str_format == "ClassicSpriteSequence")
    return std::make_unique<cnc::ClassicSpriteSequenceLoader>();
  if (str_format == "ClassicTilesetSpecificSpriteSequence")
    return std::make_unique<cnc::ClassicTilesetSpecificSpriteSequenceLoader>();
  if (str_format == "D2kSpriteSequence")
    return std::make_unique<d2k::D2kSpriteSequenceLoader>();

  throw std::runtime_error(
      std::format("Unable to find a sequence loader for type '{}'.", str_format));
}

}  // namespace ora::mods
