// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDamageOverlay.cs @b6fc03f
//          L18-165 重损烟雾(start/loop/end 三段链 + 延迟触发)。
//          The heavy-damage smoke (the start/loop/end chain + the delayed
//          trigger).
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

/// WithDamageOverlayInfo 的解析面(L22-59)
/// The parsed face of WithDamageOverlayInfo (L22-59).
struct WithDamageOverlayInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_image = "smoke_m";  // L24
  std::string str_start_sequence;     // L27
  std::string str_loop_sequence = "loop";  // L30
  std::string str_end_sequence;       // L33
  WVec wvec_offset{};                 // L36
  std::vector<int> vec_loop_count{1, 3};     // L41
  std::vector<int> vec_initial_delay{0};     // L46
  std::string str_palette;                   // L50
  bool b_is_player_palette = false;          // L53
  core::BitSet<sim::DamageType> bitset_damage_types;  // L56
  sim::DamageState minimum_damage_state = sim::DamageState::Heavy;  // L60
  sim::DamageState maximum_damage_state = sim::DamageState::Dead;   // L63

  static WithDamageOverlayInfoData Parse(const meta::RecordObject& rec_info);
};

/// WithDamageOverlay(L65-165)
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
    // L141-143
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

}  // namespace ora::mods
