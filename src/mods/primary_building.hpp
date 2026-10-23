// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/PrimaryBuilding.cs @e180e3225
//          L19-134 全文
//          The whole of PrimaryBuilding.cs L19-134.
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
  std::string str_primary_condition;      // L34
  std::string str_selection_notification;  // L38
  std::string str_selection_text_notification;  // L42
  std::vector<std::string> vec_production_queues;  // L46
  std::string str_cursor{"deploy"};       // L50
  sim::ConditionalTraitData conditional;

  static PrimaryBuildingInfoData Parse(const meta::RecordObject& rec_info);
};

/// PrimaryBuilding(L55-133;ClassicProductionQueue 的主建筑面)
/// PrimaryBuilding (L55-133; ClassicProductionQueue's primary face).
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

  /// IIssueOrder.Orders(L66-73;targeter 于 world arena 逐查询分配 —— 上游
  /// GC 域的等价)
  /// IIssueOrder.Orders (L66-73; the targeter allocates per query in the
  /// world arena — the GC-domain equivalent).
  std::vector<sim::IOrderTargeter*> Orders() override;

  /// IIssueOrder.IssueOrder(L77-84)
  /// IIssueOrder.IssueOrder (L77-84).
  std::unique_ptr<net::Order> IssueOrder(
      sim::Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;

  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;  // L85-92

  /// L94-124 | L94-124.
  void SetPrimaryProducer(sim::Actor& self, bool b_is_primary);

  bool IsPrimary() const { return b_is_primary_; }
  const PrimaryBuildingInfoData& Info() const { return info_; }

  void TraitDisabledHook(sim::Actor& self);

 private:
  PrimaryBuildingInfoData info_;
  sim::Actor* p_self_ = nullptr;
  int int4_primary_token_ = sim::Actor::InvalidConditionToken;  // L59
  bool b_is_primary_ = false;                                   // L61
};

}  // namespace ora::mods
