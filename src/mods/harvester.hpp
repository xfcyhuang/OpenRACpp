// UPSTREAM: OpenRA.Mods.Common/Traits/Harvester.cs @b6fc03f L20-330 全文
//          (逐语义重写;IResourceRenderer 的游标面 = 未移植空集:CanTarget
//          恒 false —— 上游无渲染器时同域;COVERAGE 登记)
//          The whole of Harvester.cs L20-330 (a verbatim-semantics rewrite;
//          the IResourceRenderer cursor face is the unported empty set:
//          CanTarget stays false — upstream's same domain without a
//          renderer; registered in COVERAGE).
//
// 机制对照 / Mechanism mapping:
//  - DockClientBase<HarvesterInfo> → DockClientBaseCore<Harvester>(CRTP)
//    DockClientBase<HarvesterInfo> → DockClientBaseCore<Harvester> (CRTP).
//  - IRulesetLoaded 的 Resources 校验(L80-88)→ 工厂解析时点(异常文本
//    逐字;时点差异同 D 系登记)
//    The IRulesetLoaded Resources validation (L80-88) → the factory-parse
//    moment (the exception texts verbatim; the timing difference is
//    registered like the D series).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/cvec.hpp"
#include "core/color.hpp"
#include "mods/dock_client.hpp"
#include "mods/mobile.hpp"
#include "mods/resource_layer.hpp"
#include "mods/stores_resources.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

namespace activities {
class FindAndDeliverResources;
}

using sim::Actor;
using sim::ActorInitializer;

/// HarvesterInfo(L22-89)的解析面(DockClientBaseInfo + 自有字段)
/// The parsed face of HarvesterInfo (DockClientBaseInfo + own fields).
struct HarvesterInfoData {
  core::BitSet<sim::DockType> bitset_type;  // L25("Unload")
  CVec vec_unblock_cell{0, 4};              // L28
  int int4_bale_load_delay = 4;             // L30
  int int4_bale_unload_delay = 4;           // L33
  int int4_bale_unload_amount = 1;          // L36
  int int4_harvest_facings = 0;             // L38
  std::vector<std::string> vec_resources;   // L41
  int int4_fully_loaded_speed = 85;         // L44
  bool b_search_on_creation = true;         // L47
  int int4_search_from_proc_radius = 24;    // L50
  int int4_search_from_harvester_radius = 12;  // L53
  int int4_wait_duration = 25;              // L56
  int int4_resource_refinery_direction_penalty = 200;  // L59
  bool b_queue_full_load = false;           // L62
  std::string str_empty_condition;          // L66
  std::string str_harvest_voice{"Action"};  // L69
  core::Color color_harvest_line =
      core::Color::FromArgb(0xFFDC143C);    // L72 Crimson
  std::string str_harvest_cursor{"harvest"};  // L76
  sim::ConditionalTraitData conditional;

  static HarvesterInfoData Parse(const meta::RecordObject& rec_info);

  /// L80-88:RulesetLoaded 的 Resources 校验(工厂时点)
  /// L80-88: the RulesetLoaded Resources validation (factory time).
  void ValidateResources(const game::ActorInfo& info) const;
};

/// Harvester(L91-329)
class Harvester final : public DockClientBaseCore<Harvester>,
                        public sim::IObservesVariables,
                        public sim::INotifyCreated,
                        public sim::IIssueOrder,
                        public sim::IResolveOrder,
                        public sim::IOrderVoice,
                        public sim::ISpeedModifier,
                        public sim::ISync {
 public:
  Harvester(ActorInitializer& init, const HarvesterInfoData& info);

  ORA_TRAIT_INTERFACES(Harvester, OpenRA_Mods_Common_Traits_Harvester,
                       sim::IDockClient, sim::IObservesVariables,
                       sim::INotifyCreated, sim::IIssueOrder,
                       sim::IResolveOrder, sim::IOrderVoice,
                       sim::ISpeedModifier, sim::ISync)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Harvester>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Harvester>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  // ———— IDockClient(L101/126-145)————
  // ———— IDockClient (L101/126-145) ————
  core::BitSet<sim::DockType> GetDockType() override {
    return info_.bitset_type;
  }
  bool CanDock(const core::BitSet<sim::DockType>& type,
               bool force_enter = false) override;
  bool CanDockAt(Actor& host_actor, sim::IDockHost* host,
                 bool force_enter = false,
                 bool ignore_occupancy = false) override;
  bool CanQueueDockAt(Actor& host_actor, sim::IDockHost* host,
                      bool force_enter, bool is_queued) override;

  /// L161-167:AddResource(首存满者链)
  /// L161-167: AddResource (the first-stores chain).
  void AddResource(Actor& self, const std::string& resource_type);

  /// L169-202:OnDock 族(卸货节拍)
  /// L169-202: the OnDock family (the unload cadence).
  void OnDockStarted(Actor& self, Actor& host_actor,
                     sim::IDockHost* host) override;
  bool OnDockTick(Actor& self, Actor& host_actor,
                  sim::IDockHost* host) override;
  void OnDockCompleted(Actor& self, Actor& host_actor,
                       sim::IDockHost* dock) override;

  /// L217-229:CanHarvestCell | L217-229: CanHarvestCell.
  bool CanHarvestCell(CPos cell) const;

  // ———— order 面(L231-257)————
  // ———— The order faces (L231-257) ————
  std::vector<sim::IOrderTargeter*> Orders() override;
  net::Order* IssueOrder(Actor& self, sim::IOrderTargeter* order,
                         const sim::Target& target, bool queued) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

  /// L281-284:ISpeedModifier | L281-284: ISpeedModifier.
  int GetSpeedModifier() override;

  /// L126-128:IsFull/IsEmpty/Fullness
  /// L126-128: IsFull/IsEmpty/Fullness.
  bool IsFull() const;
  bool IsEmpty() const;
  int Fullness() const;

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& self);
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  const HarvesterInfoData& Info() const { return info_; }
  DockClientManager* GetDockClientManager() const {
    return p_dock_client_manager_;
  }

  /// [VerifySync] L103-104 | the [VerifySync] member L103-104.
  int current_unload_ticks = 0;

 private:
  /// L147-158:UpdateCondition | L147-158: UpdateCondition.
  void UpdateCondition(Actor& self);

  const HarvesterInfoData info_;
  Mobile* p_mobile_ = nullptr;
  sim::IResourceLayer* p_resource_layer_ = nullptr;
  ResourceClaimLayer* p_claim_layer_ = nullptr;
  std::vector<sim::IStoresResources*> vec_stores_resources_;
  Actor* p_self_ = nullptr;
  int int4_condition_token_ = Actor::InvalidConditionToken;  // L99

  sim::IAcceptResources* p_accept_resources_ = nullptr;  // L169

  std::unique_ptr<sim::IOrderTargeter> ptr_harvest_order_targeter_;
};

}  // namespace ora::mods
