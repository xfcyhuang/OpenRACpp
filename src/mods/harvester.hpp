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

struct HarvesterInfoData {
  core::BitSet<sim::DockType> bitset_type;
  CVec vec_unblock_cell{0, 4};
  int int4_bale_load_delay = 4;
  int int4_bale_unload_delay = 4;
  int int4_bale_unload_amount = 1;
  int int4_harvest_facings = 0;
  std::vector<std::string> vec_resources;
  int int4_fully_loaded_speed = 85;
  bool b_search_on_creation = true;
  int int4_search_from_proc_radius = 24;
  int int4_search_from_harvester_radius = 12;
  int int4_wait_duration = 25;
  int int4_resource_refinery_direction_penalty = 200;
  bool b_queue_full_load = false;
  std::string str_empty_condition;
  std::string str_harvest_voice{"Action"};
  core::Color color_harvest_line =
      core::Color::FromArgb(0xFFDC143C);
  std::string str_harvest_cursor{"harvest"};
  sim::ConditionalTraitData conditional;

  static HarvesterInfoData Parse(const meta::RecordObject& rec_info);

  void ValidateResources(const game::ActorInfo& info) const;
};

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

  void AddResource(Actor& self, const std::string& resource_type);

  void OnDockStarted(Actor& self, Actor& host_actor,
                     sim::IDockHost* host) override;
  bool OnDockTick(Actor& self, Actor& host_actor,
                  sim::IDockHost* host) override;
  void OnDockCompleted(Actor& self, Actor& host_actor,
                       sim::IDockHost* dock) override;

  bool CanHarvestCell(CPos cell) const;

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

  int GetSpeedModifier() override;

  bool IsFull() const;
  bool IsEmpty() const;
  int Fullness() const;

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& self);
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  const HarvesterInfoData& Info() const { return info_; }
  DockClientManager* GetDockClientManager() const {
    return p_dock_client_manager_;
  }

  int current_unload_ticks = 0;

 private:

  void UpdateCondition(Actor& self);

  const HarvesterInfoData info_;
  Mobile* p_mobile_ = nullptr;
  sim::IResourceLayer* p_resource_layer_ = nullptr;
  ResourceClaimLayer* p_claim_layer_ = nullptr;
  std::vector<sim::IStoresResources*> vec_stores_resources_;
  Actor* p_self_ = nullptr;
  int int4_condition_token_ = Actor::InvalidConditionToken;

  sim::IAcceptResources* p_accept_resources_ = nullptr;

  std::unique_ptr<sim::IOrderTargeter> ptr_harvest_order_targeter_;
};

}
