#pragma once
import std;

#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/body_orientation.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct WithDamageOverlayInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_image = "smoke_m";
  std::string str_start_sequence;
  std::string str_loop_sequence = "loop";
  std::string str_end_sequence;
  WVec wvec_offset{};
  std::vector<int> vec_loop_count{1, 3};
  std::vector<int> vec_initial_delay{0};
  std::string str_palette;
  bool b_is_player_palette = false;
  core::BitSet<sim::DamageType> bitset_damage_types;
  sim::DamageState minimum_damage_state = sim::DamageState::Heavy;
  sim::DamageState maximum_damage_state = sim::DamageState::Dead;

  static WithDamageOverlayInfoData Parse(const meta::RecordObject& rec_info);
};

class WithDamageOverlay final : public TraitBase,
                                public sim::ConditionalTraitCore<WithDamageOverlay>,
                                public sim::IObservesVariables,
                                public sim::INotifyCreated,
                                public sim::INotifyDamage,
                                public sim::ITick {
 public:
  WithDamageOverlay(const ActorInitializer& init,
                    WithDamageOverlayInfoData info);

  ORA_TRAIT_INTERFACES(
      WithDamageOverlay, OpenRA_Mods_Common_Traits_Render_WithDamageOverlay,
      sim::IObservesVariables, sim::INotifyCreated, sim::INotifyDamage,
      sim::ITick)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithDamageOverlay>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithDamageOverlay>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void Created(Actor& self) override;
  void Damaged(Actor& self, const sim::AttackInfo& e) override;
  void Tick(Actor& self) override;

  const WithDamageOverlayInfoData& Info() const { return info_; }
  bool IsPlayingAnimation() const { return b_is_playing_animation_; }

  void TraitDisabledHook(Actor& self) {

    b_is_playing_animation_ = false;
    (void)self;
  }

 private:
  void StartAnimation(Actor& self);
  void PlayAnimation(int int4_animation_state);

  WithDamageOverlayInfoData info_;
  gfx::Animation anim_;
  std::optional<gfx::AnimationWithOffset> awo_overlay_;
  bool b_is_playing_animation_ = false;
  int int4_loop_count = 0;
  int int4_delay = -1;
};

}
