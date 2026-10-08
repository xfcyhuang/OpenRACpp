// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteTurret.cs @b6fc03f
//          L21-144(实现部分;头注见 with_sprite_turret.hpp)
//          The implementation half of WithSpriteTurret.cs L21-144 (the
//          header note lives in with_sprite_turret.hpp).
#include "mods/with_sprite_turret.hpp"

#include "mods/armament.hpp"
#include "mods/body_orientation.hpp"
#include "mods/render_utils.hpp"
#include "mods/turreted.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

WithSpriteTurretInfoData WithSpriteTurretInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithSpriteTurretInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto s = sim::RecordFieldString(rec_info, "Sequence"))
    data.str_sequence = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec_info, "Palette"))
    data.str_palette = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  if (const auto s = sim::RecordFieldString(rec_info, "Turret"))
    data.str_turret = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec_info, "Recoils"))
    data.b_recoils = *v != 0;
  return data;
}

WithSpriteTurret::WithSpriteTurret(Actor& self,
                                   const WithSpriteTurretInfoData& info)
    : sim::ConditionalTraitCore<WithSpriteTurret>(info.conditional),
      info_{info} {
  // L82-101
  auto* rs = self.Trait<RenderSprites>();
  ptr_body_ = self.Trait<BodyOrientation>();
  for (auto* tt : self.TraitsImplementing<Turreted>())
    if (tt->Name() == info.str_turret) {
      ptr_turreted_ = tt;
      break;
    }
  for (auto* w : self.TraitsImplementing<Armament>())
    if (w->InfoData().str_turret == info.str_turret)
      vec_arms_.push_back(w);

  up_default_animation_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(self), rs->GetImage(self),
      [this]() { return ptr_turreted_->WorldOrientation().Yaw; });
  up_default_animation_->PlayRepeating(NormalizeSequence(self, info.str_sequence));
  awo_default_ = gfx::AnimationWithOffset{
      *up_default_animation_,
      [this, &self]() { return TurretOffset(self); },
      [this]() { return IsTraitDisabled(); },
      [this, &self](WPos pos) {
        return render::ZOffsetFromCenter(self, pos, 1);
      }};
  rs->Add(*awo_default_, info.str_palette.empty()
                            ? std::string_view{}
                            : std::string_view{info.str_palette},
          info.b_is_player_palette);

  // 炮塔朝向限定到精灵
  // Restrict the turret facings to match the sprite.
  ptr_turreted_->SetQuantizedFacings(
      up_default_animation_->CurrentSequence()->Facings());
}

WVec WithSpriteTurret::TurretOffset(Actor& self) {
  // L103-113
  if (!info_.b_recoils)
    return ptr_turreted_->Position(self);

  int recoil_dist = 0;
  for (auto* arm : vec_arms_)
    recoil_dist += arm->Recoil.Length;
  const WVec recoil{WDist{-recoil_dist}, WDist{0}, WDist{0}};
  return ptr_turreted_->Position(self) +
         recoil.Rotate(ptr_turreted_->WorldOrientation());
}

std::string WithSpriteTurret::NormalizeSequence(
    Actor& self, std::string_view str_sequence) {
  // L115-118
  return RenderSprites::NormalizeSequence(*up_default_animation_,
                                          render::GetDamageState(self),
                                          str_sequence);
}

void WithSpriteTurret::DamageStateChanged(Actor& self,
                                          const sim::AttackInfo& /*e*/) {
  // L126-129
  DamageStateChangedInner(self);
}

void WithSpriteTurret::DamageStateChangedInner(Actor& self) {
  // L120-124
  if (up_default_animation_->CurrentSequence() != nullptr)
    up_default_animation_->ReplaceAnim(NormalizeSequence(
        self, up_default_animation_->CurrentSequence()->Name()));
}

void WithSpriteTurret::PlayCustomAnimation(Actor& self,
                                           std::string_view str_name,
                                           std::function<void()> fn_after) {
  // L131-138
  up_default_animation_->PlayThen(
      NormalizeSequence(self, str_name), [this, &self, fn_after]() {
        CancelCustomAnimation(self);
        if (fn_after)
          fn_after();
      });
}

void WithSpriteTurret::CancelCustomAnimation(Actor& self) {
  // L140-143
  up_default_animation_->PlayRepeating(
      NormalizeSequence(self, info_.str_sequence));
}

}  // namespace ora::mods
