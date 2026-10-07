// UPSTREAM: OpenRA.Mods.Common/Activities/HarvestResource.cs 实现部分
//          + FindAndDeliverResources.cs | The implementation half.
#include "mods/harvest_activities.hpp"

#include "core/int2.hpp"
#include "mods/body_orientation.hpp"
#include "mods/dock_activities.hpp"
#include "mods/pathfinding/path_finder.hpp"
#include "mods/pathfinding/path_graph.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods::activities {

// ———— HarvestResource(L20-124)————

HarvestResource::HarvestResource(sim::Actor& self, CPos target_cell)
    : cell_target_{target_cell},
      move_cooldown_helper_{self.world(),
                            dynamic_cast<Mobile*>(
                                self.Trait<sim::IMove>())} {
  // L33-45
  p_harv_ = self.Trait<Harvester>();
  p_harv_info_ = &self.Trait<Harvester>()->Info();
  p_facing_ = self.Trait<sim::IFacing>();
  p_body_ = self.Trait<BodyOrientation>();
  p_move_ = self.Trait<sim::IMove>();
  p_claim_layer_ =
      self.world().WorldActor()->Trait<ResourceClaimLayer>();
  p_resource_layer_ =
      self.world().WorldActor()->Trait<sim::IResourceLayer>();
  vec_notify_harvest_actions_ =
      self.TraitsImplementing<sim::INotifyHarvestAction>();
}

void HarvestResource::OnFirstRun(sim::Actor& self) {
  // L47-53
  // We can safely assume the claim is successful, since this is only
  // called in the same actor-tick as the targetCell is selected. Therefore
  // no other harvester would have been able to claim.(上游注释)
  p_claim_layer_->TryClaimCell(self, cell_target_);
}

bool HarvestResource::Tick(sim::Actor& self) {
  // L55-104
  if (p_harv_->IsTraitDisabled())
    Cancel(self, true);

  if (IsCanceling() || p_harv_->IsFull())
    return true;

  const std::optional<bool> result = move_cooldown_helper_.Tick(false);
  if (result.has_value())
    return *result;

  // Move towards the target cell(上游注释)
  if (self.Location() != cell_target_) {
    for (sim::INotifyHarvestAction* n : vec_notify_harvest_actions_)
      n->MovingToResources(self, cell_target_);

    move_cooldown_helper_.NotifyMoveQueued();
    QueueChild(p_move_->MoveTo(cell_target_, 0, nullptr, false,
                               std::optional<core::Color>{}));
    return false;
  }

  if (!p_harv_->CanHarvestCell(self.Location()))
    return true;

  // Turn to one of the harvestable facings(上游注释)
  if (p_harv_info_->int4_harvest_facings != 0) {
    const WAngle current = p_facing_->Facing();
    const WAngle desired =
        p_body_->QuantizeFacing(current, p_harv_info_->int4_harvest_facings);
    if (desired != current) {
      QueueChild(NewActivity<Turn>(self, desired));
      return false;
    }
  }

  const sim::ResourceLayerContents resource =
      p_resource_layer_->GetResource(self.Location());
  if (resource.str_type.empty() ||
      p_resource_layer_->RemoveResource(resource.str_type,
                                        self.Location()) != 1)
    return true;

  p_harv_->AddResource(self, resource.str_type);

  for (sim::INotifyHarvestAction* t : vec_notify_harvest_actions_)
    t->Harvested(self, resource.str_type);

  QueueChild(self.world().Arena().Create<Wait>(
      p_harv_info_->int4_bale_load_delay));
  return false;
}

void HarvestResource::OnLastRun(sim::Actor& /*self*/) {
  // L106-109
  p_claim_layer_->RemoveClaim(*p_harv_->Self());
}

void HarvestResource::Cancel(sim::Actor& self, bool keep_queue) {
  // L111-117
  for (sim::INotifyHarvestAction* n : vec_notify_harvest_actions_)
    n->MovementCancelled(self);

  sim::Activity::Cancel(self, keep_queue);
}

// ———— FindAndDeliverResources(L21-263)————

FindAndDeliverResources::FindAndDeliverResources(
    sim::Actor& self, std::optional<CPos> order_location)
    : move_cooldown_helper_{self.world(), self.Trait<Mobile>()} {
  // L37-48
  p_harv_ = self.Trait<Harvester>();
  p_harv_info_ = &self.Trait<Harvester>()->Info();
  p_dock_client_ = self.Trait<DockClientManager>();

  p_mobile_ = self.Trait<Mobile>();
  p_claim_layer_ =
      self.world().WorldActor()->Trait<ResourceClaimLayer>();
  move_cooldown_helper_.SetRetryIfDestinationBlocked(true);
  if (order_location.has_value())
    opt_order_location_ = *order_location;
}

void FindAndDeliverResources::OnFirstRun(sim::Actor& self) {
  // L50-64
  // If an explicit "harvest" order is given, direct the harvester to the
  // ordered location instead of the previous harvested cell for the
  // initial search.(上游注释)
  if (opt_order_location_.has_value()) {
    opt_last_harvested_cell_ = opt_order_location_;

    // If two "harvest" orders are issued consecutively, we deliver the
    // load first if needed. We have to make sure the actual "harvest"
    // order is not skipped if a third order is queued, so we keep
    // deliveredLoad false.(上游注释)
    if (p_harv_->IsFull())
      QueueChild(NewActivity<MoveToDock>(
          self, nullptr, nullptr, false, false,
          p_dock_client_->DockLineColor()));
  }
}

bool FindAndDeliverResources::Tick(sim::Actor& self) {
  // L66-150
  if (IsCanceling() || p_harv_->IsTraitDisabled())
    return true;

  if (NextActivity() != nullptr) {
    // Interrupt automated harvesting after clearing the first cell.
    // (上游注释)
    if (!p_harv_info_->b_queue_full_load &&
        (b_has_harvested_cell_ || LastSearchFailed()))
      return true;

    // Interrupt automated harvesting after first complete harvest cycle.
    // (上游注释)
    if (b_has_delivered_load_ || p_harv_->IsFull())
      return true;
  }

  // Are we full or have nothing more to gather? Deliver resources.
  // (上游注释)
  if (p_harv_->IsFull() ||
      (!p_harv_->IsEmpty() && LastSearchFailed())) {
    // If we are reserved it means docking was already initiated and we
    // should wait.(上游注释)
    if (p_harv_->GetDockClientManager()->ReservedHost() != nullptr)
      return false;

    QueueChild(NewActivity<MoveToDock>(
        self, nullptr, nullptr, false, false,
        p_dock_client_->DockLineColor()));
    b_has_delivered_load_ = true;
  }

  // After a failed search, wait and sit still for a bit before searching
  // again.(上游注释)
  if (LastSearchFailed() && !b_has_waited_) {
    QueueChild(self.world().Arena().Create<Wait>(
        p_harv_->Info().int4_wait_duration));
    b_has_waited_ = true;
    return false;
  }

  b_has_waited_ = false;

  // Scan for resources. If no resources are found near the current field,
  // search near the refinery instead. If that doesn't help, give up for
  // now.(上游注释)
  std::optional<CPos> closest_harvestable_cell = ClosestHarvestablePos(self);
  if (!closest_harvestable_cell.has_value()) {
    if (opt_last_harvested_cell_.has_value()) {
      opt_last_harvested_cell_ = std::nullopt;  // Forces search from
                                                // backup position.(上游注释)
      closest_harvestable_cell = ClosestHarvestablePos(self);
      b_last_search_failed_ = !closest_harvestable_cell.has_value();
    } else {
      b_last_search_failed_ = true;
    }
  } else {
    b_last_search_failed_ = false;
  }

  const std::optional<bool> result = move_cooldown_helper_.Tick(false);
  if (result.has_value())
    return *result;

  // If no harvestable position could be found and we are at the refinery,
  // get out of the way of the refinery entrance.(上游注释)
  if (LastSearchFailed()) {
    sim::IDockHost* lastproc =
        p_harv_->GetDockClientManager()->LastReservedHost();
    if (lastproc != nullptr) {
      const CPos delivery_loc =
          self.world().Map().CellContaining(lastproc->DockPosition());
      if (self.Location() == delivery_loc && p_harv_->IsEmpty()) {
        const CPos unblock_cell = delivery_loc + p_harv_->Info().vec_unblock_cell;
        const CPos move_to =
            p_mobile_->NearestMoveableCell(unblock_cell, 1, 5);
        move_cooldown_helper_.NotifyMoveQueued();
        QueueChild(p_mobile_->MoveTo(move_to, 1, nullptr, false,
                                     std::optional<core::Color>{}));
      }
    }

    return false;
  }

  // If we get here, our search for resources was successful. Commence
  // harvesting.(上游注释)
  move_cooldown_helper_.NotifyMoveQueued();
  QueueChild(NewActivity<HarvestResource>(self, *closest_harvestable_cell));
  opt_last_harvested_cell_ = *closest_harvestable_cell;
  b_has_harvested_cell_ = true;
  return false;
}

std::optional<CPos> FindAndDeliverResources::ClosestHarvestablePos(
    sim::Actor& self) {
  // L156-240
  // Harvesters should respect an explicit harvest order instead of
  // harvesting the current cell.(上游注释)
  if (!opt_order_location_.has_value()) {
    if (p_harv_->CanHarvestCell(self.Location()) &&
        p_claim_layer_->CanClaimCell(self, self.Location()))
      return self.Location();
  } else {
    if (p_harv_->CanHarvestCell(*opt_order_location_) &&
        p_claim_layer_->CanClaimCell(self, *opt_order_location_))
      return opt_order_location_;

    opt_order_location_ = std::nullopt;
  }

  // Determine where to search from and how far to search: Prioritise
  // search by these locations in this order: lastHarvestedCell ->
  // lastLinkedDock -> self.(上游注释)
  CPos search_from_loc;
  int search_radius;
  const std::optional<WPos> opt_dock_pos = [](
      const Harvester* harv) -> std::optional<WPos> {
    sim::IDockHost* host = harv->GetDockClientManager()->LastReservedHost();
    if (host == nullptr)
      return std::nullopt;
    return host->DockPosition();
  }(p_harv_);

  if (opt_last_harvested_cell_.has_value()) {
    search_radius = p_harv_info_->int4_search_from_harvester_radius;
    search_from_loc = *opt_last_harvested_cell_;
  } else {
    search_radius = p_harv_info_->int4_search_from_proc_radius;
    if (opt_dock_pos.has_value())
      search_from_loc = self.world().Map().CellContaining(*opt_dock_pos);
    else
      search_from_loc = self.Location();
  }

  const std::int64_t search_radius_squared =
      static_cast<std::int64_t>(search_radius) * search_radius;

  const map::Map& map = self.world().Map();
  const WPos harv_pos = self.CenterPosition();

  // Find any harvestable resources:(上游注释)
  const std::vector<CPos> vec_sources{search_from_loc, self.Location()};
  const std::vector<CPos> vec_path =
      p_mobile_->PathFinder()->FindPathToTargetCellByPredicate(
          &self, vec_sources,
          [this, &self](CPos loc) {
            return p_harv_->CanHarvestCell(loc) &&
                   p_claim_layer_->CanClaimCell(self, loc);
          },
          sim::BlockedByActor::Stationary,
          [this, &search_from_loc, search_radius_squared, &opt_dock_pos,
           &map, harv_pos](CPos loc) {
            if ((loc - search_from_loc).LengthSquared() >
                search_radius_squared)
              return pathfinding::kPathCostForInvalidPath;

            // Add a cost modifier to harvestable cells to prefer resources
            // that are closer to the refinery. This reduces the tendency
            // for harvesters to move in straight lines(上游注释)
            if (opt_dock_pos.has_value() &&
                p_harv_info_->int4_resource_refinery_direction_penalty >
                    0 &&
                p_harv_->CanHarvestCell(loc)) {
              const WPos pos = map.CenterOfCell(loc);

              // Calculate harv-cell-refinery angle (cosine rule)
              // (上游注释)
              const WVec b = pos - *opt_dock_pos;

              if (b != WVec{0, 0, 0}) {
                const WVec c = pos - harv_pos;
                if (c != WVec{0, 0, 0}) {
                  const WVec a = harv_pos - *opt_dock_pos;
                  const int cos_a = static_cast<int>(
                      512 *
                      (b.LengthSquared() + c.LengthSquared() -
                       a.LengthSquared()) /
                      b.Length() / c.Length());

                  // Cost modifier varies between 0 and
                  // ResourceRefineryDirectionPenalty(上游注释)
                  return std::abs(
                             p_harv_info_->int4_resource_refinery_direction_penalty /
                             2) +
                         p_harv_info_->int4_resource_refinery_direction_penalty *
                             cos_a / 2048;
                }
              }
            }

            return 0;
          });

  if (!vec_path.empty())
    return vec_path[0];

  return std::nullopt;
}

}  // namespace ora::mods::activities
