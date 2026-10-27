#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

struct PrimaryBuildingInfoData {
  std::string str_primary_condition;
  std::string str_selection_notification;
  std::string str_selection_text_notification;
  std::vector<std::string> vec_production_queues;
  std::string str_cursor{"deploy"};
  sim::ConditionalTraitData conditional;

  static PrimaryBuildingInfoData Parse(const meta::RecordObject& rec_info);
};

class PrimaryBuilding final : public sim::TraitBase,
                              public sim::IIssueOrder,
                              public sim::IResolveOrder,
                              public sim::IObservesVariables,
                              public sim::INotifyCreated,
                              public sim::ConditionalTraitCore<PrimaryBuilding> {
 public:
  ORA_TRAIT_INTERFACES(PrimaryBuilding,
                       OpenRA_Mods_Common_Traits_PrimaryBuilding,
                       sim::IIssueOrder, sim::IResolveOrder,
                       sim::IObservesVariables, sim::INotifyCreated)

  PrimaryBuilding(sim::ActorInitializer& init, PrimaryBuildingInfoData info);

  void Created(sim::Actor& self) override {
    sim::ConditionalTraitCore<PrimaryBuilding>::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return sim::ConditionalTraitCore<PrimaryBuilding>::CollectObservers();
  }

  bool IsTraitDisabled() const {
    return sim::ConditionalTraitCore<PrimaryBuilding>::IsTraitDisabled();
  }
  bool IsTraitPaused() const {
    return sim::ConditionalTraitCore<PrimaryBuilding>::IsTraitPaused();
  }

  std::vector<sim::IOrderTargeter*> Orders() override;

  std::unique_ptr<net::Order> IssueOrder(
      sim::Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;

  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;

  void SetPrimaryProducer(sim::Actor& self, bool b_is_primary);

  bool IsPrimary() const { return b_is_primary_; }
  const PrimaryBuildingInfoData& Info() const { return info_; }

  void TraitDisabledHook(sim::Actor& self);

 private:
  PrimaryBuildingInfoData info_;
  sim::Actor* p_self_ = nullptr;
  int int4_primary_token_ = sim::Actor::InvalidConditionToken;
  bool b_is_primary_ = false;
};

}
