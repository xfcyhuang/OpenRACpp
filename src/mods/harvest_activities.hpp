#pragma once
import std;

#include "core/cell_pos.hpp"
#include "mods/harvester.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "sim/activity.hpp"
#include "sim/target.hpp"

namespace ora::mods {
class BodyOrientation;

}

namespace ora::mods::activities {

using mods::Harvester;

class HarvestResource final : public sim::Activity {
 public:
  HarvestResource(sim::Actor& self, CPos target_cell);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;
  void Cancel(sim::Actor& self, bool keep_queue = false) override;

 private:
  Harvester* p_harv_;
  const HarvesterInfoData* p_harv_info_;
  sim::IFacing* p_facing_;
  ResourceClaimLayer* p_claim_layer_;
  sim::IResourceLayer* p_resource_layer_;
  BodyOrientation* p_body_;
  sim::IMove* p_move_;
  CPos cell_target_;
  std::vector<sim::INotifyHarvestAction*> vec_notify_harvest_actions_;
  MoveCooldownHelper move_cooldown_helper_;
};

class FindAndDeliverResources final : public sim::Activity {
 public:
  FindAndDeliverResources(sim::Actor& self,
                          std::optional<CPos> order_location = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

  bool LastSearchFailed() const { return b_last_search_failed_; }

 private:

  std::optional<CPos> ClosestHarvestablePos(sim::Actor& self);

  Harvester* p_harv_;
  const HarvesterInfoData* p_harv_info_;
  Mobile* p_mobile_;
  ResourceClaimLayer* p_claim_layer_;
  DockClientManager* p_dock_client_;
  MoveCooldownHelper move_cooldown_helper_;
  std::optional<CPos> opt_order_location_;
  std::optional<CPos> opt_last_harvested_cell_;
  bool b_has_delivered_load_ = false;
  bool b_has_harvested_cell_ = false;
  bool b_has_waited_ = false;
  bool b_last_search_failed_ = false;
};

}
