// UPSTREAM: OpenRA.Mods.Cnc/Graphics/ClassicTilesetSpecificSpriteSequence.cs @b6fc03f L21-95
// Classic + TilesetSpecific 的组合:Classic 的面向怪癖叠加地形集文件名变体。
// The Classic + TilesetSpecific combination: Classic's facing quirks stacked
// with the tileset filename variants.
#pragma once
import std;

#include "mods/classic_sprite_sequence.hpp"
#include "mods/tileset_specific_sprite_sequence.hpp"

namespace ora::mods::cnc {

/// ClassicTilesetSpecificSpriteSequenceLoader(L21-28)。
class ClassicTilesetSpecificSpriteSequenceLoader : public ClassicSpriteSequenceLoader {
 public:
  std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                      const std::string& str_image,
                                                      const std::string& str_sequence,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults) override;
};

/// ClassicTilesetSpecificSpriteSequence(L32-95)。
class ClassicTilesetSpecificSpriteSequence : public ClassicSpriteSequence {
 public:
  using ClassicSpriteSequence::ClassicSpriteSequence;

 protected:
  std::vector<ReservationInfo> ParseFilenames(const std::string& str_tile_set,
                                              const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
                                              const yaml::MiniYaml& yaml_data,
                                              const yaml::MiniYaml& yaml_defaults) override;
  std::vector<ReservationInfo> ParseCombineFilenames(
      const std::string& str_tile_set,
      const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
      const yaml::MiniYaml& yaml_data) override;
};

}  // namespace ora::mods::cnc
