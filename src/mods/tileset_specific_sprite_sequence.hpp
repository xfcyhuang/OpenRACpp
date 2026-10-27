#pragma once
import std;

#include "mods/default_sprite_sequence.hpp"

namespace ora::mods {

class TilesetSpecificSpriteSequenceLoader : public DefaultSpriteSequenceLoader {
 public:
  std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                      const std::string& str_image,
                                                      const std::string& str_sequence,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults) override;
};

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

}
