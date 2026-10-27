#pragma once
import std;

#include "core/wangle.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/render_sprites.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

struct WithInfantryBodyInfoData {
  sim::ConditionalTraitData conditional;
  int int4_min_idle_delay = 30;
  int int4_max_idle_delay = 110;
  std::string str_move_sequence = "run";
  std::string str_default_attack_sequence;
  std::vector<std::pair<std::string, std::vector<std::string>>>
      vec_attack_sequences;
  std::vector<std::string> vec_idle_sequences;
  std::vector<std::string> vec_stand_sequences{"stand"};
  std::string str_palette;
  bool b_is_player_palette = false;

  static WithInfantryBodyInfoData Parse(const meta::RecordObject& rec_info);
};

class WithInfantryBody : public TraitBase,
                         public sim::ConditionalTraitCore<WithInfantryBody>,
                         public sim::IObservesVariables,
                         public sim::INotifyCreated,
                         public sim::ITick,
                         public sim::INotifyAttack,
                         public sim::INotifyIdle {
 public:
  WithInfantryBody(const ActorInitializer& init,
                   const WithInfantryBodyInfoData& info);

  ORA_TRAIT_INTERFACES(
      WithInfantryBody, OpenRA_Mods_Common_Traits_Render_WithInfantryBody,
      sim::IObservesVariables, sim::INotifyCreated, sim::ITick,
      sim::INotifyAttack, sim::INotifyIdle)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithInfantryBody>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithInfantryBody>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const WithInfantryBodyInfoData& Info() const { return info_; }

  void PlayStandAnimation(Actor& self);

  void PreparingAttack(Actor& self, const sim::Target& target,
                       Armament& armament,
                       const Barrel& barrel) override;
  void Attacking(Actor& self, const sim::Target& target, Armament& armament,
                 const Barrel& barrel) override {}

  void Tick(Actor& self) override;
  void TickIdle(Actor& self) override;

  void TraitEnabledHook(Actor&) {}
  void TraitDisabledHook(Actor&) {}
  void TraitResumedHook(Actor&) {}
  void TraitPausedHook(Actor&) {}

  gfx::Animation& DefaultAnimation() { return *up_default_animation_; }

  enum class AnimationState {
    Idle = 0,
    Attacking = 1,
    Moving = 2,
    Waiting = 3,
    IdleAnimating = 4,
  };

 protected:

  virtual const WithInfantryBodyInfoData& GetDisplayInfo() const {
    return info_;
  }
  virtual std::string NormalizeInfantrySequence(
      Actor& self, std::string_view str_base_sequence);
  virtual bool AllowIdleAnimation(Actor& self);
  virtual void AttackingInner(Actor& self, Armament& armament,
                              const Barrel* ptr_barrel);
  virtual void TickInner(Actor& self);

  WithInfantryBodyInfoData info_;
  std::unique_ptr<gfx::Animation> up_default_animation_;
  std::optional<gfx::AnimationWithOffset> awo_default_;
  sim::IMove* ptr_move_ = nullptr;
  sim::IRenderInfantrySequenceModifier* ptr_rsm_ = nullptr;

  bool b_dirty_ = false;
  std::string str_idle_sequence_;
  int int4_idle_delay_ = 0;
  AnimationState state_animation_ = AnimationState::Waiting;
  bool b_was_modifying_ = false;

 private:
  bool IsModifyingSequence() const {
    return ptr_rsm_ != nullptr && ptr_rsm_->IsModifyingSequence();
  }
};

}
