// UPSTREAM: OpenRA.Mods.Common/Graphics/TilesetSpecificSpriteSequence.cs @b6fc03f L20-93
// 带地形集变体的序列:TilesetFilenames/TilesetFilenamesPattern 命中当前
// tileset 时覆盖 Filename/FilenamePattern,未命中回退 Default 的解析。
// A sequence with tileset-specific variants: a TilesetFilenames/
// TilesetFilenamesPattern hit for the active tileset overrides Filename/
// FilenamePattern, and a miss falls back to the Default parsing.
#pragma once
import std;

#include "mods/default_sprite_sequence.hpp"

namespace ora::mods {

/// TilesetSpecificSpriteSequenceLoader(L20-27)。
class TilesetSpecificSpriteSequenceLoader : public DefaultSpriteSequenceLoader {
 public:
  std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                      const std::string& str_image,
                                                      const std::string& str_sequence,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults) override;
};

/// TilesetSpecificSpriteSequence(L30-93)。
class TilesetSpecificSpriteSequence : public DefaultSpriteSequence {
 public:
  using DefaultSpriteSequence::DefaultSpriteSequence;

 protected:
  std::vector<ReservationInfo> ParseFilenames(
      const std::string& str_tile_set,
      const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
      const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults) override;
  std::vector<ReservationInfo> ParseCombineFilenames(
      const std::string& str_tile_set,
      const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
      const yaml::MiniYaml& yaml_data) override;
};

}  // namespace ora::mods
