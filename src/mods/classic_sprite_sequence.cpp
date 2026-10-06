// UPSTREAM: OpenRA.Mods.Cnc/Graphics/ClassicSpriteSequence.cs @b6fc03f L18-48 +
//           OpenRA.Mods.Cnc/Util.cs @b6fc03f L18-67
//          (classic_sprite_sequence.hpp 的实现)
//          Implementation of classic_sprite_sequence.hpp.
import std;
#include "mods/classic_sprite_sequence.hpp"

namespace ora::mods::cnc {

namespace {

// 32 面向的非线性角度上界表(Util.cs L20-27 逐值)
// The non-linear angle upper bounds for 32 facings (Util.cs L20-27, value
// for value).
constexpr std::array<std::int32_t, 32> kSpriteRanges{
    20, 56, 88, 132, 156, 184, 212, 240, 268, 296, 324, 352, 384, 416, 452, 488,
    532, 568, 604, 644, 668, 696, 724, 752, 780, 808, 836, 864, 896, 928, 964, 1000};

// 每帧的真实面向(Util.cs L30-35 逐值)
// The actual facing of each frame (Util.cs L30-35, value for value).
constexpr std::array<std::int32_t, 32> kSpriteFacings{
    0, 40, 74, 112, 146, 172, 200, 228, 256, 284, 312, 340, 370, 402, 436, 472,
    512, 552, 588, 626, 658, 684, 712, 740, 768, 796, 824, 852, 882, 914, 948, 984};

}  // namespace

std::int32_t ClassicIndexFacing(WAngle wangle_facing, std::int32_t int4_num_frames) {
  if (int4_num_frames == 32) {
    const std::int32_t int4_angle = wangle_facing.Angle;
    for (std::size_t int4_i{}; int4_i < kSpriteRanges.size(); int4_i++)
      if (int4_angle < kSpriteRanges[int4_i])
        return static_cast<std::int32_t>(int4_i);

    return 0;
  }

  return gfx::IndexFacing(wangle_facing, int4_num_frames);
}

WAngle ClassicQuantizeFacing(WAngle wangle_facing, std::int32_t int4_steps) {
  if (int4_steps == 32)
    return WAngle{kSpriteFacings[static_cast<std::size_t>(
        ClassicIndexFacing(wangle_facing, int4_steps))]};

  // QuantizeFacing = IndexFacing * (1024/facings)(Mods.Common/Util.cs L106-108)
  // QuantizeFacing = IndexFacing * (1024/facings) (Mods.Common/Util.cs
  // L106-108).
  return WAngle{gfx::IndexFacing(wangle_facing, int4_steps) * (1024 / int4_steps)};
}

std::unique_ptr<gfx::ISpriteSequence> ClassicSpriteSequenceLoader::CreateSequence(
    gfx::SpriteCache& cache_sprites, const std::string& str_image, const std::string& str_sequence,
    const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults) {
  return std::make_unique<ClassicSpriteSequence>(cache_sprites, *this, str_image, str_sequence,
                                                 yaml_data, yaml_defaults);
}

ClassicSpriteSequence::ClassicSpriteSequence(gfx::SpriteCache& cache_sprites,
                                             gfx::ISpriteSequenceLoader& loader,
                                             std::string str_image, std::string str_sequence,
                                             const yaml::MiniYaml& yaml_data,
                                             const yaml::MiniYaml& yaml_defaults)
    : DefaultSpriteSequence{cache_sprites, loader, std::move(str_image), std::move(str_sequence),
                            yaml_data, yaml_defaults} {
  b_use_classic_facings_ = LoadBool("UseClassicFacings", false, yaml_data, &yaml_defaults);

  if (b_use_classic_facings_ && int4_facings_ != 32)
    throw std::runtime_error(std::format(
        "Sequence {}.{}: UseClassicFacings is only valid for 32 facings", str_image_, str_name_));
}

std::int32_t ClassicSpriteSequence::GetFacingFrameOffset(WAngle wangle_facing) {
  return b_use_classic_facings_ ? ClassicIndexFacing(wangle_facing, int4_facings_)
                                : gfx::IndexFacing(wangle_facing, int4_facings_);
}

}  // namespace ora::mods::cnc
