import std;

#include "gfx/actor_preview.hpp"

#include "game/actor_info.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::gfx {

sim::World& ActorPreviewInitializer::World() { return wr_.World(); }

std::function<WRot()> ActorPreviewInitializer::GetOrientation() {
  const meta::RecordObject* facing_info = sim::FindTraitInfoOfInterface(
      actor_, "OpenRA.Traits.IFacingInfo");
  if (facing_info == nullptr)
    return [] { return WRot::None(); };

  const meta::RecordObject* mobile_info = nullptr;
  for (const auto& up_rec : actor_.Traits())
    if (up_rec->record_desc().str_name == std::string_view{"MobileInfo"}) {
      mobile_info = up_rec.get();
      break;
    }

  const sim::LocationInit* location = dict_.GetOrDefault<sim::LocationInit>();

  WDist margin(-1);
  if (mobile_info != nullptr) {
    if (const auto v = sim::RecordFieldInt(*mobile_info,
                                           "TerrainOrientationAdjustmentMargin"))
      margin = WDist{static_cast<std::int32_t>(*v)};
  }

  if (location == nullptr || mobile_info == nullptr ||
      margin.Length < 0) {
    if (const sim::DynamicFacingInit* dynamic_init =
            dict_.GetOrDefault<sim::DynamicFacingInit>()) {
      const std::function<WAngle()> get_facing = dynamic_init->Value();
      return [get_facing] { return WRot::FromYaw(get_facing()); };
    }

    WAngle facing;
    const sim::FacingInit* facing_init = dict_.GetOrDefault<sim::FacingInit>();
    if (facing_init != nullptr)
      facing = facing_init->Value();
    else if (const auto v = sim::RecordFieldInt(*facing_info, "InitialFacing"))
      facing = WAngle{static_cast<std::int32_t>(*v)};
    return [facing] { return WRot::FromYaw(facing); };
  }

  const sim::TerrainOrientationInit* orientation_init =
      dict_.GetOrDefault<sim::TerrainOrientationInit>();
  const WRot terrain_orientation =
      orientation_init != nullptr
          ? orientation_init->Value()
          : wr_.World().Map().TerrainOrientation(location->Value());
  const WVec normal_vector = WVec(0, 0, 1024).Rotate(terrain_orientation);

  if (const sim::DynamicFacingInit* dynamic_init =
          dict_.GetOrDefault<sim::DynamicFacingInit>()) {
    const std::function<WAngle()> get_facing = dynamic_init->Value();
    return [terrain_orientation, normal_vector, get_facing] {
      return terrain_orientation + WRot(normal_vector, get_facing());
    };
  }

  WAngle facing;
  const sim::FacingInit* facing_init = dict_.GetOrDefault<sim::FacingInit>();
  if (facing_init != nullptr)
    facing = facing_init->Value();
  else if (const auto v = sim::RecordFieldInt(*facing_info, "InitialFacing"))
    facing = WAngle{static_cast<std::int32_t>(*v)};
  return [terrain_orientation, normal_vector, facing] {
    return terrain_orientation + WRot(normal_vector, facing);
  };
}

std::function<WAngle()> ActorPreviewInitializer::GetFacing() {
  const meta::RecordObject* facing_info = sim::FindTraitInfoOfInterface(
      actor_, "OpenRA.Traits.IFacingInfo");
  if (facing_info == nullptr)
    return [] { return WAngle::Zero(); };

  if (const sim::DynamicFacingInit* dynamic_init =
          dict_.GetOrDefault<sim::DynamicFacingInit>())
    return dynamic_init->Value();

  const sim::FacingInit* facing_init = dict_.GetOrDefault<sim::FacingInit>();
  if (facing_init != nullptr) {
    const WAngle facing = facing_init->Value();
    return [facing] { return facing; };
  }

  const auto v = sim::RecordFieldInt(*facing_info, "InitialFacing");
  const WAngle fallback =
      v ? WAngle{static_cast<std::int32_t>(*v)} : WAngle::Zero();
  return [fallback] { return fallback; };
}

sim::DamageState ActorPreviewInitializer::GetDamageState() {
  const sim::HealthInit* health = dict_.GetOrDefault<sim::HealthInit>();
  if (health == nullptr)
    return sim::DamageState::Undamaged;

  const int hf = health->Value();
  if (hf <= 0)
    return sim::DamageState::Dead;
  if (hf < 25)
    return sim::DamageState::Critical;
  if (hf < 50)
    return sim::DamageState::Heavy;
  if (hf < 75)
    return sim::DamageState::Medium;
  if (hf < 100)
    return sim::DamageState::Light;
  return sim::DamageState::Undamaged;
}

void SpriteActorPreview::RenderUI(WorldRenderer& wr, int2 pos,
                                  float scale,
                                  std::vector<RenderItem>& vec_out) {
  std::array<RenderItem, 2> arr_items{};
  std::int32_t count = 0;
  anim_.RenderUI(wr, pos, fn_offset_(), fn_z_offset_(), ptr_palette_, arr_items,
                 count, scale);
  for (std::int32_t i = 0; i < count; i++)
    vec_out.push_back(arr_items[i]);
}

void SpriteActorPreview::Render(WorldRenderer& wr, WPos pos,
                                std::vector<RenderItem>& vec_out) {
  (void)wr;
  std::array<RenderItem, 2> arr_items{};
  std::int32_t count = 0;
  anim_.Render(pos, fn_offset_(), fn_z_offset_(), ptr_palette_, arr_items, count);
  for (std::int32_t i = 0; i < count; i++)
    vec_out.push_back(arr_items[i]);
}

void SpriteActorPreview::ScreenBounds(WorldRenderer& wr, WPos pos,
                                      std::vector<Rectangle>& vec_out) {
  vec_out.push_back(anim_.ScreenBounds(wr, pos, fn_offset_()));
}

namespace {

struct PreviewFaceRegistry {
  std::vector<std::pair<std::string, ActorPreviewSpritesFn>> vec_sprites;
  std::vector<std::pair<std::string, RenderActorPreviewFn>> vec_render;

  static PreviewFaceRegistry& Instance() {
    static PreviewFaceRegistry registry;
    return registry;
  }
};

}

void RegisterActorPreviewSprites(std::string str_type_name,
                                 ActorPreviewSpritesFn fn) {
  ActorPreviewSpritesFaces().emplace_back(std::move(str_type_name),
                                          std::move(fn));
}

void RegisterRenderActorPreview(std::string str_type_name,
                                RenderActorPreviewFn fn) {
  PreviewFaceRegistry::Instance().vec_render.emplace_back(std::move(str_type_name),
                                                   std::move(fn));
}

std::vector<std::pair<std::string, ActorPreviewSpritesFn>>&
ActorPreviewSpritesFaces() {
  return PreviewFaceRegistry::Instance().vec_sprites;
}

std::vector<std::unique_ptr<IActorPreview>> RenderActorPreviewOf(
    const game::ActorInfo& actor, ActorPreviewInitializer& init) {
  std::vector<std::unique_ptr<IActorPreview>> vec_out;
  for (const meta::RecordObject* rec :
       actor.TraitInfosByInterface(
           "OpenRA.Mods.Common.Traits.IRenderActorPreviewInfo"))
    for (const auto& [name, fn] : PreviewFaceRegistry::Instance().vec_render)
      if (name == rec->record_desc().str_name) {
        fn(*rec, init, vec_out);
        break;
      }
  return vec_out;
}

}
