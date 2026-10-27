#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct WandersInfoData {
  int int4_wander_move_radius = 1;
  int int4_reduce_move_radius_delay = 5;
  int int4_min_move_delay = 0;
  int int4_max_move_delay = 0;
  std::vector<std::string> vec_avoid_terrain_types;
  sim::ConditionalTraitData conditional;

  static WandersInfoData Parse(const meta::RecordObject& rec_info);
};

class Wanders final : public TraitBase,
                      public sim::ConditionalTraitCore<Wanders>,
                      public sim::IObservesVariables,
                      public sim::INotifyBecomingIdle,
                      public sim::INotifyIdle {
 public:

  Wanders(ActorInitializer& init, WandersInfoData&& info);

  ORA_TRAIT_INTERFACES(Wanders, OpenRA_Mods_Common_Traits_Wanders,
                       sim::IObservesVariables, sim::INotifyBecomingIdle,
                       sim::INotifyIdle)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<Wanders>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<Wanders>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  void OnBecomingIdle(Actor& self) override;

  void TickIdle(Actor& self) override;

  virtual void DoAction(Actor& self, const CPos& target_cell);

  const WandersInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 protected:

  virtual void OnBecomingIdleInner(Actor& self);

 private:
  friend class sim::ConditionalTraitCore<Wanders>;

  std::optional<CPos> PickTargetLocation(Actor& self);

  WandersInfoData info_;
  sim::IMove* ptr_move_ = nullptr;
  std::optional<core::Color> color_target_line_;
  int int4_countdown = 0;
  int int4_ticks_idle = 0;
  int int4_effective_move_radius = 0;
};

}
