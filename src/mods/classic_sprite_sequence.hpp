#pragma once
import std;

#include "core/wangle.hpp"
#include "mods/default_sprite_sequence.hpp"

namespace ora::mods::cnc {

std::int32_t ClassicIndexFacing(WAngle wangle_facing, std::int32_t int4_num_frames);

WAngle ClassicQuantizeFacing(WAngle wangle_facing, std::int32_t int4_steps);

class ClassicSpriteSequenceLoader : public mods::DefaultSpriteSequenceLoader {
 public:
  std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                      const std::string& str_image,
                                                      const std::string& str_sequence,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults) override;
};

class ClassicSpriteSequence : public mods::DefaultSpriteSequence {
 public:
  ClassicSpriteSequence(gfx::SpriteCache& cache_sprites, gfx::ISpriteSequenceLoader& loader,
                        std::string str_image, std::string str_sequence,
                        const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults);

 protected:
  std::int32_t GetFacingFrameOffset(WAngle wangle_facing) override;

 private:
  bool b_use_classic_facings_ = false;
};

}
