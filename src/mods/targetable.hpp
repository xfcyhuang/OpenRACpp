// UPSTREAM: OpenRA.Mods.Common/Traits/Targetable.cs @b6fc03f L18-55 全文
//          (逐语义重写;Cloak trait 未移植 → cloaks 空集的等价面,
//          COVERAGE 登记)
//          The whole of Targetable.cs L18-55 (verbatim-semantics rewrite;
//          the Cloak trait is unported → the empty-cloaks equivalent face,
//          registered in COVERAGE).
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

/// TargetableInfo 的解析面(L21-30)
/// The parsed face of TargetableInfo (L21-30).
struct TargetableInfoData {
  core::BitSet<sim::TargetableType> bitset_target_types;  // L24
  bool b_requires_force_fire = false;                     // L28
  sim::ConditionalTraitData conditional;

  static TargetableInfoData Parse(const meta::RecordObject& rec_info);
};

/// Targetable(L32-55;ITargetable)
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

  /// L40-49:TargetableBy(Cloak 空集 → 非禁用即真)
  /// L40-49: TargetableBy (the empty Cloak set → true unless disabled).
  bool TargetableBy(Actor& self, Actor& viewer) override;

  core::BitSet<sim::TargetableType> TargetTypes() const override {
    return info_.bitset_target_types;  // L51
  }

  bool RequiresForceFire() const override {
    return info_.b_requires_force_fire;  // L53
  }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 protected:
  friend class sim::ConditionalTraitCore<Targetable>;
  TargetableInfoData info_;
};

}  // namespace ora::mods
