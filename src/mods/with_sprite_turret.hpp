#pragma once
import std;

#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/render_sprites.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Turreted;
class BodyOrientation;
class Armament;

struct WithSpriteTurretInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_sequence = "turret";
  std::string str_palette;
  bool b_is_player_palette = false;
  std::string str_turret = "primary";
  bool b_recoils = true;

  static WithSpriteTurretInfoData Parse(const meta::RecordObject& rec_info);
};

class WithSpriteTurret : public TraitBase,
                         public sim::ConditionalTraitCore<WithSpriteTurret>,
                         public sim::IObservesVariables,
                         public sim::INotifyCreated,
                         public sim::INotifyDamageStateChanged {
 public:
  WithSpriteTurret(Actor& self, const WithSpriteTurretInfoData& info);

  ORA_TRAIT_INTERFACES(
      WithSpriteTurret, OpenRA_Mods_Common_Traits_Render_WithSpriteTurret,
      sim::IObservesVariables, sim::INotifyCreated,
      sim::INotifyDamageStateChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithSpriteTurret>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithSpriteTurret>::IsTraitDisabled();
  }

  void Created(Actor& self) override {
    sim::ConditionalTraitCore<WithSpriteTurret>::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const WithSpriteTurretInfoData& Info() const { return info_; }

  virtual WVec TurretOffset(Actor& self);

  std::string NormalizeSequence(Actor& self, std::string_view str_sequence);

  void DamageStateChanged(Actor& self, const sim::AttackInfo& e) override;

  void PlayCustomAnimation(Actor& self, std::string_view str_name,
                           std::function<void()> fn_after = {});
  void CancelCustomAnimation(Actor& self);

  void TraitEnabledHook(Actor&) {}
  void TraitDisabledHook(Actor&) {}
  void TraitResumedHook(Actor&) {}
  void TraitPausedHook(Actor&) {}

  gfx::Animation& DefaultAnimation() { return *up_default_animation_; }

 protected:

  virtual void DamageStateChangedInner(Actor& self);

  WithSpriteTurretInfoData info_;
  BodyOrientation* ptr_body_ = nullptr;
  Turreted* ptr_turreted_ = nullptr;
  std::vector<Armament*> vec_arms_;
  std::unique_ptr<gfx::Animation> up_default_animation_;
  std::optional<gfx::AnimationWithOffset> awo_default_;
};

}
