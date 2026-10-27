#pragma once
import std;

#include "formats/r8_loader.hpp"
#include "mods/default_sprite_sequence.hpp"

namespace ora::mods::d2k {

class D2kSpriteSequenceLoader : public mods::DefaultSpriteSequenceLoader {
 public:
  std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                      const std::string& str_image,
                                                      const std::string& str_sequence,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults) override;
};

class D2kSpriteSequence : public mods::DefaultSpriteSequence {
 public:
  D2kSpriteSequence(gfx::SpriteCache& cache_sprites, gfx::ISpriteSequenceLoader& loader,
                    std::string str_image, std::string str_sequence,
                    const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults);

  void ReserveSprites(const std::string& str_tile_set, const yaml::MiniYaml& yaml_data,
                      const yaml::MiniYaml& yaml_defaults,
                      gfx::SpriteCache& cache_sprites) override;

 private:
  core::Color color_remap_{};
  bool b_use_shadow_ = true;
  bool b_convert_shroud_to_fog_ = false;

  std::deque<fmt::R8RemappableFrame> deque_wrapped_frames_;
};

}
