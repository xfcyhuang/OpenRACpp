// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteBody.cs @b6fc03f
//          L21-151 + WithFacingSpriteBody.cs L19-39(实现部分)
//          The implementation halves of WithSpriteBody.cs L21-151 +
//          WithFacingSpriteBody.cs L19-39.
#include "mods/with_sprite_body.hpp"

#include "map/map.hpp"
#include "mods/render_utils.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

WithSpriteBodyInfoData WithSpriteBodyInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithSpriteBodyInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto s = sim::RecordFieldString(rec_info, "StartSequence"))
    data.str_start_sequence = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec_info, "Sequence"))
    data.str_sequence = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec_info, "Name"))
    data.str_name = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec_info, "ForceToGround"))
    data.b_force_to_ground = *v != 0;
  if (const auto s = sim::RecordFieldString(rec_info, "Palette"))
    data.str_palette = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  return data;
}

WithSpriteBody::WithSpriteBody(const ActorInitializer& init,
                               const WithSpriteBodyInfoData& info,
                               std::function<WAngle()> fn_base_facing)
    : sim::ConditionalTraitCore<WithSpriteBody>(info.conditional),
      info_{info} {
  // L72-92
  Actor& self = init.Self();
  auto* rs = self.Trait<RenderSprites>();

  if (!fn_base_facing)
    fn_base_facing = [] { return WAngle{}; };
  fn_base_facing_ = std::move(fn_base_facing);

  // Paused 闭包(L79-80):暂停仅当当前即默认序列(自定义动画不被暂停打断)
  // The Paused closure (L79-80): paused only while on the default sequence
  // (custom animations are not paused out from under us).
  auto fn_paused = [this, &self]() {
    return IsTraitPaused() &&
           up_default_animation_->CurrentSequence() != nullptr &&
           up_default_animation_->CurrentSequence()->Name() ==
               NormalizeSequence(self, info_.str_sequence);
  };

  std::function<WVec()> fn_subtract_dat;
  if (info.b_force_to_ground) {
    const map::Map* ptr_map = &self.world().Map();
    fn_subtract_dat = [&self, ptr_map]() {
      return WVec{0, 0, -ptr_map->DistanceAboveTerrain(self.CenterPosition())
                                .Length};
    };
  }

  up_default_animation_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(self), rs->GetImage(self),
      fn_base_facing_, std::move(fn_paused));
  awo_default_ = gfx::AnimationWithOffset{
      *up_default_animation_, std::move(fn_subtract_dat),
      [this]() { return IsTraitDisabled(); }};
  rs->Add(*awo_default_, info.str_palette.empty()
                            ? std::string_view{}
                            : std::string_view{info.str_palette},
          info.b_is_player_palette);

  // 界缓存自默认序列(动画切换防闪烁)
  // The bounds cache off the default sequence (anti-flicker across
  // animation changes).
  up_bounds_animation_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(self), rs->GetImage(self),
      fn_base_facing_, [this, &self]() {
        return IsTraitPaused() &&
               up_bounds_animation_->CurrentSequence() != nullptr &&
               up_bounds_animation_->CurrentSequence()->Name() ==
                   NormalizeSequence(self, info_.str_sequence);
      });
  up_bounds_animation_->PlayRepeating(info.str_sequence);
}

std::string WithSpriteBody::NormalizeSequence(Actor& self,
                                              std::string_view str_sequence) {
  // L94-97
  return RenderSprites::NormalizeSequence(
      *up_default_animation_, render::GetDamageState(self), str_sequence);
}

void WithSpriteBody::TraitEnabledHook(Actor& self) {
  // TraitEnabled(L99-106)
  if (!info_.str_start_sequence.empty())
    PlayCustomAnimation(self, info_.str_start_sequence, [this, &self]() {
      up_default_animation_->PlayRepeating(
          NormalizeSequence(self, info_.str_sequence));
    });
  else
    up_default_animation_->PlayRepeating(
        NormalizeSequence(self, info_.str_sequence));
}

void WithSpriteBody::PlayCustomAnimation(Actor& self, std::string_view str_name,
                                         std::function<void()> fn_after) {
  // L108-115
  up_default_animation_->PlayThen(
      NormalizeSequence(self, str_name), [this, &self, fn_after]() {
        CancelCustomAnimation(self);
        if (fn_after)
          fn_after();
      });
}

void WithSpriteBody::PlayCustomAnimationRepeating(
    Actor& self, std::string_view str_name) {
  // L117-120
  up_default_animation_->PlayRepeating(NormalizeSequence(self, str_name));
}

void WithSpriteBody::PlayCustomAnimationBackwards(
    Actor& self, std::string_view str_name, std::function<void()> fn_after) {
  // L122-129
  up_default_animation_->PlayBackwardsThen(
      NormalizeSequence(self, str_name), [this, &self, fn_after]() {
        CancelCustomAnimation(self);
        if (fn_after)
          fn_after();
      });
}

void WithSpriteBody::CancelCustomAnimation(Actor& self) {
  // L131-134
  up_default_animation_->PlayRepeating(
      NormalizeSequence(self, info_.str_sequence));
}

void WithSpriteBody::DamageStateChanged(Actor& self,
                                        const sim::AttackInfo& /*e*/) {
  // L142-145
  DamageStateChangedInner(self);
}

void WithSpriteBody::DamageStateChangedInner(Actor& self) {
  // L136-140
  if (up_default_animation_->CurrentSequence() != nullptr)
    up_default_animation_->ReplaceAnim(NormalizeSequence(
        self, up_default_animation_->CurrentSequence()->Name()));
}

Rectangle WithSpriteBody::AutoMouseoverBounds(Actor& self,
                                              gfx::WorldRenderer* wr) {
  // L147-150:IAutoMouseBounds(wr 非空 = 屏幕换算装配面)
  // L147-150: IAutoMouseBounds (a non-null wr = the screen-conversion
  // assembly face).
  if (wr != nullptr)
    return up_bounds_animation_->ScreenBounds(*wr, self.CenterPosition(),
                                              WVec{});
  return Rectangle::Empty();
}

WithFacingSpriteBody::WithFacingSpriteBody(const ActorInitializer& init,
                                           const WithSpriteBodyInfoData& info)
    : WithSpriteBody(init, info, RenderSprites::MakeFacingFunc(init.Self())) {}

}  // namespace ora::mods
