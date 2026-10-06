// UPSTREAM: OpenRA.Mods.Cnc/Graphics/ClassicSpriteSequence.cs @b6fc03f L18-48 +
//           OpenRA.Mods.Cnc/Util.cs @b6fc03f L18-67(SpriteRanges/SpriteFacings 表
//           与 ClassicIndexFacing/ClassicQuantizeFacing)
// 一代 Westwood 作品怪癖的序列:UseClassicFacings(仅 32 面向合法)启用
// 非线性面向映射 ClassicIndexFacing。
// The sequence carrying first-generation Westwood oddities: UseClassicFacings
// (valid only with 32 facings) enables the non-linear facing map of
// ClassicIndexFacing.
#pragma once
import std;

#include "core/wangle.hpp"
#include "mods/default_sprite_sequence.hpp"

namespace ora::mods::cnc {

/// ClassicIndexFacing(Util.cs L44-58):32 面向走 SpriteRanges 非线性表,
/// 其余回退 IndexFacing。
/// ClassicIndexFacing (Util.cs L44-58): 32 facings take the non-linear
/// SpriteRanges table; everything else falls back to IndexFacing.
std::int32_t ClassicIndexFacing(WAngle wangle_facing, std::int32_t int4_num_frames);

/// ClassicQuantizeFacing(Util.cs L61-67):32 面向查 SpriteFacings 表。
/// ClassicQuantizeFacing (Util.cs L61-67): 32 facings read the SpriteFacings
/// table.
WAngle ClassicQuantizeFacing(WAngle wangle_facing, std::int32_t int4_steps);

/// ClassicSpriteSequenceLoader(L18-25)。
class ClassicSpriteSequenceLoader : public mods::DefaultSpriteSequenceLoader {
 public:
  std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                      const std::string& str_image,
                                                      const std::string& str_sequence,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults) override;
};

/// ClassicSpriteSequence(L28-48)。
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

}  // namespace ora::mods::cnc
