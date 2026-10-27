#pragma once
import std;

#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class ExternalCondition final : public sim::TraitBase,
                                public sim::ITick,
                                public sim::INotifyCreated,
                                public sim::INotifyOwnerChanged {
 public:
  ORA_TRAIT_INTERFACES(ExternalCondition,
                       OpenRA_Mods_Common_Traits_ExternalCondition,
                       sim::ITick, sim::INotifyCreated,
                       sim::INotifyOwnerChanged)

  ExternalCondition(std::string str_condition, int source_cap, int total_cap)
      : str_condition_{std::move(str_condition)},
        int4_source_cap_{source_cap}, int4_total_cap_{total_cap} {}

  bool CanGrantCondition(const void* source) const;

  int GrantCondition(sim::Actor& self, const void* source, int duration = 0,
                     int remaining = 0);

  bool TryRevokeCondition(sim::Actor& self, const void* source, int token);

  void Tick(sim::Actor& self) override;
  void Created(sim::Actor& self) override;
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

  const std::string& Condition() const { return str_condition_; }

 private:

  struct TimedToken {
    int int4_expires = 0;
    int int4_token = 0;
    const void* ptr_source = nullptr;
  };

  std::string str_condition_;
  int int4_source_cap_ = 0;
  int int4_total_cap_ = 0;

  std::map<const void*, std::vector<int>> map_permanent_tokens_;
  std::vector<TimedToken> vec_timed_tokens_;
  std::vector<sim::IConditionTimerWatcher*> vec_watchers_;
  int int4_duration_ = 0;
  int int4_expires_ = 0;
};

void GrantExternalConditionByName(sim::Actor& target, sim::Actor& source,
                                  const std::string& str_condition,
                                  int duration);

class ProximityExternalCondition final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::ITick,
      public sim::INotifyAddedToWorld,
      public sim::INotifyRemovedFromWorld,
      public sim::INotifyOtherProduction,
      public sim::INotifyProximityOwnerChanged,
      private sim::ConditionalTraitCore<ProximityExternalCondition> {
 public:
  ORA_TRAIT_INTERFACES(
      ProximityExternalCondition,
      OpenRA_Mods_Common_Traits_ProximityExternalCondition,
      sim::IObservesVariables, sim::INotifyCreated, sim::ITick,
      sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld,
      sim::INotifyOtherProduction, sim::INotifyProximityOwnerChanged)

  ProximityExternalCondition(sim::Actor& self, std::string str_condition,
                             WDist range, WDist maximum_vertical_offset,
                             sim::PlayerRelationship valid_relationships,
                             bool b_affects_parent,
                             sim::ConditionalTraitData conditional);
  ~ProximityExternalCondition() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void AddedToWorld(sim::Actor& self) override;
  void RemovedFromWorld(sim::Actor& self) override;
  void Tick(sim::Actor& self) override;
  void UnitProducedByOther(sim::Actor& self, sim::Actor& producer,
                           sim::Actor& produced,
                           const std::string& production_type,
                           sim::TypeDictionary& init) override;
  void OnProximityOwnerChanged(sim::Actor& actor,
                               sim::Player* old_owner,
                               sim::Player* new_owner) override;

 private:
  friend class sim::ConditionalTraitCore<ProximityExternalCondition>;
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);
  void ActorEntered(sim::Actor& a);
  void ActorExited(sim::Actor& a);

  bool GrantInRange(sim::Actor& target, sim::Actor& self);

  std::string str_condition_;
  WDist range_;
  WDist v_range_max_;
  sim::PlayerRelationship valid_relationships_ =
      sim::PlayerRelationship::Ally;
  bool b_affects_parent_ = false;

  sim::Actor* ptr_self_ = nullptr;
  std::map<sim::Actor*, int> map_tokens_;
  int int4_proximity_trigger_ = 0;
  WPos pos_cached_;
  WDist range_cached_;
  WDist v_range_cached_;
  WDist range_desired_;
  WDist v_range_desired_;
};

}
