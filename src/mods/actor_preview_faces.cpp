import std;

#include "mods/actor_preview_faces.hpp"

#include "game/actor_info.hpp"
#include "gfx/actor_preview.hpp"
#include "gfx/world_renderer.hpp"
#include "mods/body_orientation.hpp"
#include "mods/render_sprites.hpp"
#include "mods/turreted.hpp"
#include "mods/with_infantry_body.hpp"
#include "mods/with_sprite_body.hpp"
#include "mods/with_parachute.hpp"
#include "mods/with_sprite_turret.hpp"
#include "meta/type_registry.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {
namespace {

const meta::RecordObject* FindTraitInfoOfBase(
    const game::ActorInfo& actor, std::string_view str_type_name) {
  for (const auto& up_rec_trait : actor.Traits()) {
    const meta::RecordDesc* desc = &up_rec_trait->record_desc();
    while (desc != nullptr) {
      if (desc->str_name == str_type_name)
        return up_rec_trait.get();
      desc = desc->str_base.empty()
                 ? nullptr
                 : meta::TypeRegistry::FindType(desc->str_base);
    }
  }
  return nullptr;
}

void RenderSpritesRenderPreviewFace(
    const meta::RecordObject& rec, gfx::ActorPreviewInitializer& init,
    std::vector<std::unique_ptr<gfx::IActorPreview>>& vec_out) {
  const RenderSpritesInfoData data = RenderSpritesInfoData::Parse(rec);
  const std::string str_faction =
      init.GetValue<sim::FactionInit, std::string>(
          init.Actor().InstanceNameOf(&rec));
  const std::string& str_owner_name =
      init.Get<sim::OwnerInit>()->InternalName();
  const std::string str_image = data.GetImage(init.Actor(), str_faction);
  gfx::PaletteReference* ptr_palette = init.WR().Palette(
      !data.str_palette.empty()
          ? std::string_view{data.str_palette}
          : std::string_view{data.str_player_palette + str_owner_name});

  int facings = 0;
  const meta::RecordObject* rec_body =
      FindTraitInfoOfBase(init.Actor(), "BodyOrientationInfo");

  if (rec_body != nullptr) {
    facings = BodyOrientationInfoData::Parse(*rec_body).int4_quantized_facings;
    if (facings == -1)
      facings = ResolvePreviewQuantizedBodyFacings(init.Actor(), str_faction);
  }

  for (const meta::RecordObject* rec_spi : init.Actor().TraitInfosByInterface(
           "OpenRA.Mods.Common.Traits.Render.IRenderActorPreviewSpritesInfo"))
    for (const auto& [name, fn] : gfx::ActorPreviewSpritesFaces())
      if (name == rec_spi->record_desc().str_name) {
        fn(*rec_spi, init, str_image, facings, ptr_palette, vec_out);
        break;
      }
}

void WithSpriteBodyPreviewFace(
    const meta::RecordObject& rec, gfx::ActorPreviewInitializer& init,
    const std::string& str_image, int facings,
    gfx::PaletteReference* ptr_palette,
    std::vector<std::unique_ptr<gfx::IActorPreview>>& vec_out) {
  const WithSpriteBodyInfoData data = WithSpriteBodyInfoData::Parse(rec);
  if (!data.conditional.b_enabled_by_default)
    return;

  if (data.b_is_player_palette)
    ptr_palette = init.WR().Palette(
        data.str_palette + init.Get<sim::OwnerInit>()->InternalName());
  else if (!data.str_palette.empty())
    ptr_palette = init.WR().Palette(data.str_palette);

  gfx::Animation anim(RenderSprites::MakeAnimationDeps(init.World()),
                      str_image);
  anim.PlayRepeating(RenderSprites::NormalizeSequence(
      anim, init.GetDamageState(), data.str_sequence));

  vec_out.push_back(std::make_unique<gfx::SpriteActorPreview>(
      std::move(anim), [] { return WVec{}; }, [] { return 0; }, ptr_palette));
  (void)facings;
}

void WithFacingSpriteBodyPreviewFace(
    const meta::RecordObject& rec, gfx::ActorPreviewInitializer& init,
    const std::string& str_image, int facings,
    gfx::PaletteReference* ptr_palette,
    std::vector<std::unique_ptr<gfx::IActorPreview>>& vec_out) {
  const WithSpriteBodyInfoData data = WithSpriteBodyInfoData::Parse(rec);
  if (!data.conditional.b_enabled_by_default)
    return;

  gfx::Animation anim(RenderSprites::MakeAnimationDeps(init.World()),
                      str_image, init.GetFacing());
  anim.PlayRepeating(RenderSprites::NormalizeSequence(
      anim, init.GetDamageState(), data.str_sequence));

  vec_out.push_back(std::make_unique<gfx::SpriteActorPreview>(
      std::move(anim), [] { return WVec{}; }, [] { return 0; }, ptr_palette));
  (void)facings;
}

void WithInfantryBodyPreviewFace(
    const meta::RecordObject& rec, gfx::ActorPreviewInitializer& init,
    const std::string& str_image, int facings,
    gfx::PaletteReference* ptr_palette,
    std::vector<std::unique_ptr<gfx::IActorPreview>>& vec_out) {
  const WithInfantryBodyInfoData data = WithInfantryBodyInfoData::Parse(rec);
  if (!data.conditional.b_enabled_by_default)
    return;

  gfx::Animation anim(RenderSprites::MakeAnimationDeps(init.World()),
                      str_image, init.GetFacing());
  anim.PlayRepeating(RenderSprites::NormalizeSequence(
      anim, init.GetDamageState(), data.vec_stand_sequences.at(0)));

  if (data.b_is_player_palette)
    ptr_palette = init.WR().Palette(
        data.str_palette + init.Get<sim::OwnerInit>()->InternalName());
  else if (!data.str_palette.empty())
    ptr_palette = init.WR().Palette(data.str_palette);

  vec_out.push_back(std::make_unique<gfx::SpriteActorPreview>(
      std::move(anim), [] { return WVec{}; }, [] { return 0; }, ptr_palette));
  (void)facings;
}

void WithSpriteTurretPreviewFace(
    const meta::RecordObject& rec, gfx::ActorPreviewInitializer& init,
    const std::string& str_image, int facings,
    gfx::PaletteReference* ptr_palette,
    std::vector<std::unique_ptr<gfx::IActorPreview>>& vec_out) {
  const WithSpriteTurretInfoData data = WithSpriteTurretInfoData::Parse(rec);
  if (!data.conditional.b_enabled_by_default)
    return;

  const meta::RecordObject* rec_body =
      FindTraitInfoOfBase(init.Actor(), "BodyOrientationInfo");
  if (rec_body == nullptr)
    throw std::runtime_error("Sequence contains no elements");

  const meta::RecordObject* rec_turret = nullptr;
  TurretedInfoData turret_data;
  for (const auto& up_rec_trait : init.Actor().Traits()) {
    const meta::RecordDesc* desc = &up_rec_trait->record_desc();
    bool b_is_turreted = false;
    while (desc != nullptr) {
      if (desc->str_name == std::string_view{"TurretedInfo"}) {
        b_is_turreted = true;
        break;
      }
      desc = desc->str_base.empty()
                 ? nullptr
                 : meta::TypeRegistry::FindType(desc->str_base);
    }
    if (!b_is_turreted)
      continue;
    const TurretedInfoData parsed = TurretedInfoData::Parse(*up_rec_trait);
    if (parsed.str_turret == data.str_turret) {
      rec_turret = up_rec_trait.get();
      turret_data = parsed;
      break;
    }
  }
  if (rec_turret == nullptr)
    throw std::runtime_error("Sequence contains no elements");

  const BodyOrientationInfoData body =
      BodyOrientationInfoData::Parse(*rec_body);
  const std::function<WAngle()> fn_turret_facing =
      TurretedInfoData::WorldFacingFromInit(
          init.Dict(), init.Actor().InstanceNameOf(rec_turret),
          turret_data.angle_initial_facing);
  gfx::Animation anim(RenderSprites::MakeAnimationDeps(init.World()),
                      str_image, fn_turret_facing);
  anim.Play(RenderSprites::NormalizeSequence(anim, init.GetDamageState(),
                                             data.str_sequence));

  const std::function<WAngle()> fn_facing = init.GetFacing();
  const WVec turret_offset = turret_data.vec_offset;
  const int facings_value = facings;

  std::function<WRot()> fn_orientation =
      [body, fn_facing, facings_value]() {
        return body.QuantizeOrientation(WRot::FromYaw(fn_facing()),
                                        facings_value);
      };
  std::function<WVec()> fn_offset = [body, turret_offset, fn_orientation]() {
    return body.LocalToWorld(turret_offset.Rotate(fn_orientation()));
  };
  std::function<int()> fn_z_offset = [fn_offset]() {
    const WVec tmp_offset = fn_offset();
    return -(tmp_offset.Y + tmp_offset.Z) + 1;
  };

  if (data.b_is_player_palette)
    ptr_palette = init.WR().Palette(
        data.str_palette + init.Get<sim::OwnerInit>()->InternalName());
  else if (!data.str_palette.empty())
    ptr_palette = init.WR().Palette(data.str_palette);

  vec_out.push_back(std::make_unique<gfx::SpriteActorPreview>(
      std::move(anim), std::move(fn_offset), std::move(fn_z_offset),
      ptr_palette));
}

void WithParachutePreviewFace(
    const meta::RecordObject& rec, gfx::ActorPreviewInitializer& init,
    const std::string& str_image, int facings,
    gfx::PaletteReference* ptr_palette,
    std::vector<std::unique_ptr<gfx::IActorPreview>>& vec_out) {
  const WithParachuteInfoData data = WithParachuteInfoData::Parse(rec);
  if (!data.conditional.b_enabled_by_default)
    return;

  if (str_image.empty())
    return;

  if (!data.str_palette.empty())
    ptr_palette = init.WR().Palette(data.str_palette);

  const auto* dynamic_facing_init =
      init.GetOrDefault<sim::DynamicFacingInit>({});
  const std::function<WAngle()> fn_facing =
      dynamic_facing_init != nullptr
          ? dynamic_facing_init->Value()
          : [f = init.GetValue<sim::FacingInit, WAngle>({}, WAngle{})]() {
              return f;
            };

  gfx::Animation anim(RenderSprites::MakeAnimationDeps(init.World()),
                      str_image, fn_facing);
  const std::string str_sequence = data.str_sequence;
  anim.PlayThen(data.str_opening_sequence, [&anim, str_sequence]() {
    anim.PlayRepeating(str_sequence);
  });

  const meta::RecordObject* rec_body =
      FindTraitInfoOfBase(init.Actor(), "BodyOrientationInfo");
  if (rec_body == nullptr)
    throw std::runtime_error("Sequence contains no elements");
  const BodyOrientationInfoData body =
      BodyOrientationInfoData::Parse(*rec_body);

  const WVec offset_value = data.wvec_offset;
  std::function<WRot()> fn_orientation =
      [body, fn_facing, facings]() {
        return body.QuantizeOrientation(WRot::FromYaw(fn_facing()), facings);
      };
  std::function<WVec()> fn_offset = [body, offset_value, fn_orientation]() {
    return body.LocalToWorld(offset_value.Rotate(fn_orientation()));
  };
  std::function<int()> fn_z_offset = [fn_offset]() {
    const WVec tmp_offset = fn_offset();
    return tmp_offset.Y + tmp_offset.Z + 1;
  };

  vec_out.push_back(std::make_unique<gfx::SpriteActorPreview>(
      std::move(anim), std::move(fn_offset), std::move(fn_z_offset),
      ptr_palette));
}

}

void RegisterActorPreviewFaces() {
  static const bool b_registered = [] {
    gfx::RegisterRenderActorPreview("RenderSpritesInfo",
                                    RenderSpritesRenderPreviewFace);
    gfx::RegisterRenderActorPreview("RenderSpritesEditorOnlyInfo",
                                    RenderSpritesRenderPreviewFace);
    gfx::RegisterActorPreviewSprites("WithSpriteBodyInfo",
                                     WithSpriteBodyPreviewFace);
    gfx::RegisterActorPreviewSprites("WithFacingSpriteBodyInfo",
                                     WithFacingSpriteBodyPreviewFace);
    gfx::RegisterActorPreviewSprites("WithInfantryBodyInfo",
                                     WithInfantryBodyPreviewFace);
    gfx::RegisterActorPreviewSprites("WithSpriteTurretInfo",
                                     WithSpriteTurretPreviewFace);
    gfx::RegisterActorPreviewSprites("WithParachuteInfo",
                                     WithParachutePreviewFace);
    return true;
  }();
  (void)b_registered;
}

}
