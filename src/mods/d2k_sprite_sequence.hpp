// UPSTREAM: OpenRA.Mods.D2k/Graphics/D2kSpriteSequence.cs @b6fc03f L19-116
// D2k 色重映射序列:Remap/UseShadow/ConvertShroudToFog 经 AdjustFrame 把
// R8RemappableFrame 按序列标志重建。
// The D2k color-remap sequence: Remap/UseShadow/ConvertShroudToFog rebuild
// each R8RemappableFrame with the sequence flags through AdjustFrame.
#pragma once
import std;

#include "formats/r8_loader.hpp"
#include "mods/default_sprite_sequence.hpp"

namespace ora::mods::d2k {

/// D2kSpriteSequenceLoader(L19-26)。
class D2kSpriteSequenceLoader : public mods::DefaultSpriteSequenceLoader {
 public:
  std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                      const std::string& str_image,
                                                      const std::string& str_sequence,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults) override;
};

/// D2kSpriteSequence(L29-116)。
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

  // 包装帧的存活区(LoadReservations 结束前有效 = D96 的调用方保证)
  // The keep-alive storage of wrapped frames (alive until LoadReservations
  // ends — D96's caller guarantee).
  std::deque<fmt::R8RemappableFrame> deque_wrapped_frames_;
};

}  // namespace ora::mods::d2k
