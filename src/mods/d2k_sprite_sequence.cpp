// UPSTREAM: OpenRA.Mods.D2k/Graphics/D2kSpriteSequence.cs @b6fc03f L19-116
//          (d2k_sprite_sequence.hpp 的实现)
//          Implementation of d2k_sprite_sequence.hpp.
import std;
#include "mods/d2k_sprite_sequence.hpp"

#include "gfx/gfx_util.hpp"

namespace ora::mods::d2k {

// core::Vector3 的渲染运算在 ora::gfx(ADL 不达;显式引入)
// core::Vector3's rendering arithmetic lives in ora::gfx (beyond ADL;
// imported explicitly).
using gfx::operator+;

std::unique_ptr<gfx::ISpriteSequence> D2kSpriteSequenceLoader::CreateSequence(
    gfx::SpriteCache& cache_sprites, const std::string& str_image, const std::string& str_sequence,
    const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults) {
  return std::make_unique<D2kSpriteSequence>(cache_sprites, *this, str_image, str_sequence,
                                             yaml_data, yaml_defaults);
}

D2kSpriteSequence::D2kSpriteSequence(gfx::SpriteCache& cache_sprites,
                                     gfx::ISpriteSequenceLoader& loader, std::string str_image,
                                     std::string str_sequence, const yaml::MiniYaml& yaml_data,
                                     const yaml::MiniYaml& yaml_defaults)
    : DefaultSpriteSequence{cache_sprites, loader, std::move(str_image), std::move(str_sequence),
                            yaml_data, yaml_defaults} {
  color_remap_ = LoadColor("Remap", core::Color{}, yaml_data, &yaml_defaults);
  b_use_shadow_ = LoadBool("UseShadow", true, yaml_data, &yaml_defaults);
  b_convert_shroud_to_fog_ = LoadBool("ConvertShroudToFog", false, yaml_data, &yaml_defaults);
}

void D2kSpriteSequence::ReserveSprites(const std::string& str_tile_set,
                                       const yaml::MiniYaml& yaml_data,
                                       const yaml::MiniYaml& yaml_defaults,
                                       gfx::SpriteCache& cache_sprites) {
  str_tile_set_ = str_tile_set;
  const std::optional<std::vector<std::int32_t>> opt_vec_frames =
      LoadFrames("Frames", yaml_data, &yaml_defaults);
  const bool b_flip_x = LoadBool("FlipX", false, yaml_data, &yaml_defaults);
  const bool b_flip_y = LoadBool("FlipY", false, yaml_data, &yaml_defaults);
  const std::int32_t int4_z_ramp = LoadInt32("ZRamp", 0, yaml_data, &yaml_defaults);
  const core::Vector3 vec_offset = LoadVector3("Offset", core::Vector3{}, yaml_data,
                                               &yaml_defaults);
  const gfx::BlendMode kind_blend_mode =
      LoadBlendMode("BlendMode", gfx::BlendMode::Alpha, yaml_data, &yaml_defaults);

  // 仅在需要重映射时挂 AdjustFrame(上游 null 委托语义)
  // AdjustFrame attaches only when a remap is needed (upstream's null-
  // delegate semantics).
  gfx::AdjustFrameFn fn_adjust_frame = nullptr;
  if (color_remap_.argb != core::Color{}.argb || b_convert_shroud_to_fog_)
    fn_adjust_frame = [this](const gfx::ISpriteFrame& frame_f, std::int32_t,
                           std::int32_t) -> const gfx::ISpriteFrame* {
      // f is R8Loader.RemappableFrame → WithSequenceFlags;否则原帧
      // f is R8Loader.RemappableFrame → WithSequenceFlags; else the input.
      if (const auto* frame_rf = dynamic_cast<const fmt::R8RemappableFrame*>(&frame_f))
        return &deque_wrapped_frames_.emplace_back(
            frame_rf->WithSequenceFlags(b_use_shadow_, b_convert_shroud_to_fog_, color_remap_));
      return &frame_f;
    };

  const yaml::MiniYamlNode* node_combine = yaml_data.NodeWithKeyOrDefault("Combine");
  if (node_combine != nullptr) {
    for (const yaml::MiniYamlNode& node_sub : node_combine->Value.Nodes) {
      const yaml::MiniYaml& yaml_sub_data = node_sub.Value;
      const core::Vector3 vec_sub_offset = LoadVector3("Offset", core::Vector3{}, yaml_sub_data,
                                                       &NoData());
      const bool b_sub_flip_x = LoadBool("FlipX", false, yaml_sub_data, &NoData());
      const bool b_sub_flip_y = LoadBool("FlipY", false, yaml_sub_data, &NoData());
      const std::optional<std::vector<std::int32_t>> opt_vec_sub_frames =
          LoadFrames("Frames", yaml_sub_data, nullptr);

      for (const ReservationInfo& info_f :
           ParseCombineFilenames(str_tile_set, opt_vec_sub_frames, yaml_sub_data)) {
        const std::int32_t int4_token = cache_sprites.ReserveSprites(
            info_f.str_filename, info_f.opt_vec_load_frames, info_f.location_source,
            fn_adjust_frame);

        SpriteReservation reservation;
        reservation.int4_token = int4_token;
        reservation.vec_offset = vec_sub_offset + vec_offset;
        reservation.b_flip_x = b_sub_flip_x ^ b_flip_x;
        reservation.b_flip_y = b_sub_flip_y ^ b_flip_y;
        reservation.kind_blend_mode = kind_blend_mode;
        reservation.fp4_z_ramp = static_cast<float>(int4_z_ramp);
        reservation.opt_vec_frames = info_f.opt_vec_frames;
        vec_sprites_to_load_.push_back(std::move(reservation));
      }
    }
  } else {
    for (const ReservationInfo& info_f :
         ParseFilenames(str_tile_set, opt_vec_frames, yaml_data, yaml_defaults)) {
      const std::int32_t int4_token = cache_sprites.ReserveSprites(
          info_f.str_filename, info_f.opt_vec_load_frames, info_f.location_source,
          fn_adjust_frame);

      SpriteReservation reservation;
      reservation.int4_token = int4_token;
      reservation.vec_offset = vec_offset;
      reservation.b_flip_x = b_flip_x;
      reservation.b_flip_y = b_flip_y;
      reservation.kind_blend_mode = kind_blend_mode;
      reservation.fp4_z_ramp = static_cast<float>(int4_z_ramp);
      reservation.opt_vec_frames = info_f.opt_vec_frames;
      vec_sprites_to_load_.push_back(std::move(reservation));
    }
  }
}

}  // namespace ora::mods::d2k
