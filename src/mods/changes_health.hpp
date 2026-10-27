#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct ChangesHealthInfoData {
  int int4_step = 0;
  int int4_delay = 5;
  int int4_damage_cooldown = 0;
  int int4_start_if_below = 50;
  bool b_kill_if_below = 0;
  sim::ConditionalTraitData conditional;

  static ChangesHealthInfoData Parse(const meta::RecordObject& rec_info);
};

class ChangesHealth final : public TraitBase,
                            public sim::ConditionalTraitCore<ChangesHealth>,
                            public sim::IObservesVariables,
                            public sim::ISync {
 public:
  ChangesHealth(ChangesHealthInfoData&& info);

  ORA_TRAIT_INTERFACES(ChangesHealth,
                       OpenRA_Mods_Common_Traits_ChangesHealth,
                       sim::IObservesVariables, sim::ISync)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<ChangesHealth>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<ChangesHealth>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  int int4_ticks = 0;
  int int4_damage_ticks = 0;

  const ChangesHealthInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<ChangesHealth>;

  ChangesHealthInfoData info_;
};

}
