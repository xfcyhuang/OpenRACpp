#pragma once
import std;

#include "core/bitset.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::TraitBase;

struct TargetableInfoData {
  core::BitSet<sim::TargetableType> bitset_target_types;
  bool b_requires_force_fire = false;
  sim::ConditionalTraitData conditional;

  static TargetableInfoData Parse(const meta::RecordObject& rec_info);
};

class Targetable : public TraitBase,
                   public sim::ConditionalTraitCore<Targetable>,
                   public sim::IObservesVariables,
                   public sim::INotifyCreated,
                   public sim::ITargetable {
 public:
  explicit Targetable(const TargetableInfoData& info)
      : sim::ConditionalTraitCore<Targetable>(info.conditional),
        info_{info} {}

  ORA_TRAIT_INTERFACES(Targetable,
                       OpenRA_Mods_Common_Traits_Targetable,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITargetable)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Targetable>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Targetable>::IsTraitDisabled();
  }

  void Created(Actor& self) override { CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  bool TargetableBy(Actor& self, Actor& viewer) override;

  core::BitSet<sim::TargetableType> TargetTypes() const override {
    return info_.bitset_target_types;
  }

  bool RequiresForceFire() const override {
    return info_.b_requires_force_fire;
  }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 protected:
  friend class sim::ConditionalTraitCore<Targetable>;
  TargetableInfoData info_;
};

}
