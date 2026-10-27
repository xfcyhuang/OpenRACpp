import std;

#include "mods/pathfinding/path_finder.hpp"

#include "sim/actor.hpp"
#include "sim/world.hpp"
#include "mods/pathfinding/locomotor.hpp"

namespace ora::mods::pathfinding {

using sim::BlockedByActor;

PathFinder::PathFinder(Actor& self, int heuristic_weight_percentage)
    : world_{self.world()}, heuristic_weight_percentage_{heuristic_weight_percentage} {}

void PathFinder::WorldLoaded(World& w, gfx::WorldRenderer*) {

  const std::vector<Locomotor*> locomotors =
      w.WorldActor()->TraitsImplementing<Locomotor>();
  if (!locomotors.empty()) {

    if (fn_actor_locomotor_ == nullptr) {
      Locomotor* first = locomotors.front();
      fn_actor_locomotor_ = [first](Actor&) { return first; };
    }
    hpf_blocked_by_none_ = std::make_unique<HierarchicalPathFinder>(
        w, locomotors.front(), w.ActorMapFace(), BlockedByActor::None);

    hpf_blocked_by_immovable_ = std::make_unique<HierarchicalPathFinder>(
        w, locomotors.front(), w.ActorMapFace(), BlockedByActor::Immovable);
  }
}

std::vector<CPos> PathFinder::FindPathToTargetCell(
    Actor* self, std::span<const CPos> sources, CPos target,
    BlockedByActor check, const std::function<int(CPos)>& custom_cost,
    Actor* ignore_actor, bool lane_bias) {

  return FindPathToTarget(self, sources, target, check, custom_cost,
                          ignore_actor, false, lane_bias);
}

std::vector<CPos> PathFinder::FindPathToTargetCells(
    Actor* self, CPos source, std::span<const CPos> targets,
    BlockedByActor check, const std::function<int(CPos)>& custom_cost,
    Actor* ignore_actor, bool lane_bias) {

  if (targets.empty())
    return NoPathVector();

  Locomotor* locomotor = GetActorLocomotor(*self);
  std::vector<CPos> accessible_targets;
  for (const CPos& target : targets)
    if (PathSearch::CellAllowsMovement(world_, locomotor, target, custom_cost) &&
        locomotor->MovementCostToEnterCell(self, target, check, ignore_actor,
                                           true) !=
            kMovementCostForUnreachableCell)
      accessible_targets.push_back(target);
  if (accessible_targets.empty())
    return NoPathVector();

  std::vector<CPos> path;
  const bool source_is_accessible =
      PathSearch::CellAllowsMovement(world_, locomotor, source, custom_cost) &&
      locomotor->MovementCostToEnterCell(self, source, check, ignore_actor,
                                         true) !=
          kMovementCostForUnreachableCell;
  if (source_is_accessible) {

    path = FindPathToTarget(self, accessible_targets, source, check,
                            custom_cost, ignore_actor, true, lane_bias);
  } else {

    std::unique_ptr<PathSearch> search = PathSearch::ToTargetCell(
        world_, locomotor, self, accessible_targets, source, check,
        HeuristicWeightPercentage(), custom_cost, ignore_actor, lane_bias,
        true, nullptr, nullptr, nullptr);
    path = search->FindPath();
  }

  std::ranges::reverse(path);
  return path;
}

std::vector<CPos> PathFinder::FindPathToTarget(
    Actor* self, std::span<const CPos> sources, CPos target,
    BlockedByActor check, const std::function<int(CPos)>& custom_cost,
    Actor* ignore_actor, bool in_reverse, bool lane_bias) {
  if (sources.empty())
    return NoPathVector();

  Locomotor* locomotor = GetActorLocomotor(*self);

  if (!PathSearch::CellAllowsMovement(world_, locomotor, target, custom_cost) ||
      locomotor->MovementCostToEnterCell(self, target, check, ignore_actor,
                                         in_reverse) ==
          kMovementCostForUnreachableCell)
    return NoPathVector();

  if (sources.size() == 1) {
    const CPos source = sources[0];

    if (source.Layer() == target.Layer() &&
        (source - target).LengthSquared() < 3) {

      if (!PathSearch::CellAllowsMovement(world_, locomotor, source,
                                          custom_cost))
        return NoPathVector();
      return {target, source};
    }

    return GetHierarchicalPathFinder(locomotor, check, ignore_actor)
        .FindPath(self, source, target, check, HeuristicWeightPercentage(),
                  custom_cost, ignore_actor, in_reverse, lane_bias);
  }

  return GetHierarchicalPathFinder(locomotor, check, ignore_actor)
      .FindPath(self, sources, target, check, HeuristicWeightPercentage(),
                custom_cost, ignore_actor, in_reverse, lane_bias);
}

HierarchicalPathFinder& PathFinder::GetHierarchicalPathFinder(
    Locomotor* locomotor, BlockedByActor check, Actor* ignore_actor) const {

  const bool use_none =
      check == BlockedByActor::None || ignore_actor != nullptr;
  HierarchicalPathFinder* hpf =
      use_none ? hpf_blocked_by_none_.get() : hpf_blocked_by_immovable_.get();
  if (hpf == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  return *hpf;
}

std::vector<CPos> PathFinder::FindPathToTargetCellByPredicate(
    Actor* self, std::span<const CPos> sources,
    const std::function<bool(CPos)>& target_predicate, BlockedByActor check,
    const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
    bool lane_bias) {

  std::unique_ptr<PathSearch> search = PathSearch::ToTargetCellByPredicate(
      world_, GetActorLocomotor(*self), self, sources, target_predicate, check,
      custom_cost, ignore_actor, lane_bias, nullptr);
  return search->FindPath();
}

bool PathFinder::PathExistsForLocomotor(Locomotor* locomotor, CPos source,
                                        CPos target) {

  if (hpf_blocked_by_none_ == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  (void)locomotor;
  return hpf_blocked_by_none_->PathExists(source, target);
}

bool PathFinder::PathMightExistForLocomotorBlockedByImmovable(
    Locomotor* locomotor, CPos source, CPos target) {
  if (hpf_blocked_by_immovable_ == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  (void)locomotor;
  return hpf_blocked_by_immovable_->PathExists(source, target);
}

Locomotor* PathFinder::GetActorLocomotor(Actor& self) const {

  if (fn_actor_locomotor_ != nullptr)
    return fn_actor_locomotor_(self);
  throw std::runtime_error("NullReferenceException");
}

}
