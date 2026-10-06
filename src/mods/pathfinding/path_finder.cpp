// UPSTREAM: OpenRA.Mods.Common/Traits/World/PathFinder.cs @b6fc03f(实现部分)
//          The implementation half of PathFinder.cs.
import std;

#include "mods/pathfinding/path_finder.hpp"

#include "sim/actor.hpp"
#include "sim/world.hpp"
#include "mods/pathfinding/locomotor.hpp"

namespace ora::mods::pathfinding {

using sim::BlockedByActor;

// ———— ctor(L51-55)————
PathFinder::PathFinder(Actor& self, int heuristic_weight_percentage)
    : world_{self.world()}, heuristic_weight_percentage_{heuristic_weight_percentage} {}

// ———— WorldLoaded(L65-77)————
void PathFinder::WorldLoaded(World& w, gfx::WorldRenderer*) {
  // pathFinderOverlay = world.WorldActor.TraitOrDefault<PathFinderOverlay>()
  // (overlay 随调试渲染批 —— 恒 null 的空操作面)
  // (the overlay lands with the debug-render batch — the always-null no-op
  // face).

  // Requires<LocomotorInfo> ensures all Locomotors have been initialized.
  // (上游注释)| (upstream comment)
  const std::vector<Locomotor*> locomotors =
      w.WorldActor()->TraitsImplementing<Locomotor>();
  if (!locomotors.empty()) {
    // 缺省的 GetActorLocomotor 解析 = 首个世界 Locomotor(头注)
    // The default GetActorLocomotor resolution = the world's first
    // Locomotor (the header note).
    if (fn_actor_locomotor_ == nullptr) {
      Locomotor* first = locomotors.front();
      fn_actor_locomotor_ = [first](Actor&) { return first; };
    }
    hpf_blocked_by_none_ = std::make_unique<HierarchicalPathFinder>(
        w, locomotors.front(), w.ActorMapFace(), BlockedByActor::None);
    // 上游按 locomotor 字典建立多份;部分覆盖装配面(单 Locomotor 可达域)
    // 下以首个承载 —— 多 Locomotor 字典随 Mobile 批扩展
    // Upstream builds per-locomotor dictionaries; under the
    // partial-coverage assembly face (a single reachable Locomotor) the
    // first carries it — the multi-Locomotor dictionaries land with the
    // Mobile batch.
    hpf_blocked_by_immovable_ = std::make_unique<HierarchicalPathFinder>(
        w, locomotors.front(), w.ActorMapFace(), BlockedByActor::Immovable);
  }
}

// ———— FindPathToTargetCell(L97-104)————
std::vector<CPos> PathFinder::FindPathToTargetCell(
    Actor* self, std::span<const CPos> sources, CPos target,
    BlockedByActor check, const std::function<int(CPos)>& custom_cost,
    Actor* ignore_actor, bool lane_bias) {
  // sources.ToList()(L103)| (L103).
  return FindPathToTarget(self, sources, target, check, custom_cost,
                          ignore_actor, false, lane_bias);
}

// ———— FindPathToTargetCells(L124-176)————
std::vector<CPos> PathFinder::FindPathToTargetCells(
    Actor* self, CPos source, std::span<const CPos> targets,
    BlockedByActor check, const std::function<int(CPos)>& custom_cost,
    Actor* ignore_actor, bool lane_bias) {
  // However there is a case of asymmetry we must handle(上游注释段)
  if (targets.empty())
    return NoPathVector();

  // As targets must be accessible, determine accessible targets in
  // advance(上游注释段)| (upstream comment)
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

  // When checking if the source location is accessible(上游注释段)
  std::vector<CPos> path;
  const bool source_is_accessible =
      PathSearch::CellAllowsMovement(world_, locomotor, source, custom_cost) &&
      locomotor->MovementCostToEnterCell(self, source, check, ignore_actor,
                                         true) !=
          kMovementCostForUnreachableCell;
  if (source_is_accessible) {
    // As both ends are accessible, we can freely swap them.(上游注释)
    path = FindPathToTarget(self, accessible_targets, source, check,
                            custom_cost, ignore_actor, true, lane_bias);
  } else {
    // When we treat the source as a target, we need to be able to path to
    // it.(上游注释段)| (upstream comment)
    std::unique_ptr<PathSearch> search = PathSearch::ToTargetCell(
        world_, locomotor, self, accessible_targets, source, check,
        HeuristicWeightPercentage(), custom_cost, ignore_actor, lane_bias,
        true, nullptr, nullptr, nullptr);
    path = search->FindPath();
  }

  // Since we swapped the positions, we need to reverse the path to swap it
  // back.(上游注释)| (upstream comment)
  std::ranges::reverse(path);
  return path;
}

// ———— FindPathToTarget(L178-216)————
std::vector<CPos> PathFinder::FindPathToTarget(
    Actor* self, std::span<const CPos> sources, CPos target,
    BlockedByActor check, const std::function<int(CPos)>& custom_cost,
    Actor* ignore_actor, bool in_reverse, bool lane_bias) {
  if (sources.empty())
    return NoPathVector();

  Locomotor* locomotor = GetActorLocomotor(*self);

  // If the target cell is inaccessible, bail early.(上游注释段)
  if (!PathSearch::CellAllowsMovement(world_, locomotor, target, custom_cost) ||
      locomotor->MovementCostToEnterCell(self, target, check, ignore_actor,
                                         in_reverse) ==
          kMovementCostForUnreachableCell)
    return NoPathVector();

  // When searching from only one source cell, some optimizations are
  // possible.(上游注释)| (upstream comment)
  if (sources.size() == 1) {
    const CPos source = sources[0];

    // For adjacent cells on the same layer, we can return the path without
    // invoking a full search.(上游注释)| (upstream comment)
    if (source.Layer() == target.Layer() &&
        (source - target).LengthSquared() < 3) {
      // If the source cell is inaccessible, there is no path.(上游注释段)
      if (!PathSearch::CellAllowsMovement(world_, locomotor, source,
                                          custom_cost))
        return NoPathVector();
      return {target, source};
    }

    // Use a hierarchical path search, which performs a guided
    // bidirectional search.(上游注释)| (upstream comment)
    return GetHierarchicalPathFinder(locomotor, check, ignore_actor)
        .FindPath(self, source, target, check, HeuristicWeightPercentage(),
                  custom_cost, ignore_actor, in_reverse, lane_bias);
  }

  // Use a hierarchical path search, which performs a guided unidirectional
  // search.(上游注释)| (upstream comment)
  return GetHierarchicalPathFinder(locomotor, check, ignore_actor)
      .FindPath(self, sources, target, check, HeuristicWeightPercentage(),
                custom_cost, ignore_actor, in_reverse, lane_bias);
}

// ———— GetHierarchicalPathFinder(L218-226)————
HierarchicalPathFinder& PathFinder::GetHierarchicalPathFinder(
    Locomotor* locomotor, BlockedByActor check, Actor* ignore_actor) const {
  // If there is an actor to ignore, we cannot use an HPF that accounts for
  // any blocking actors.(上游注释段)| (upstream comment)
  const bool use_none =
      check == BlockedByActor::None || ignore_actor != nullptr;
  HierarchicalPathFinder* hpf =
      use_none ? hpf_blocked_by_none_.get() : hpf_blocked_by_immovable_.get();
  if (hpf == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  return *hpf;
}

// ———— FindPathToTargetCellByPredicate(L237-249)————
std::vector<CPos> PathFinder::FindPathToTargetCellByPredicate(
    Actor* self, std::span<const CPos> sources,
    const std::function<bool(CPos)>& target_predicate, BlockedByActor check,
    const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
    bool lane_bias) {
  // With no pre-specified target location, we can only use a
  // unidirectional search.(上游注释)| (upstream comment)
  std::unique_ptr<PathSearch> search = PathSearch::ToTargetCellByPredicate(
      world_, GetActorLocomotor(*self), self, sources, target_predicate, check,
      custom_cost, ignore_actor, lane_bias, nullptr);
  return search->FindPath();
}

// ———— PathExistsForLocomotor(L262-265)————
bool PathFinder::PathExistsForLocomotor(Locomotor* locomotor, CPos source,
                                        CPos target) {
  // hierarchicalPathFindersBlockedByNoneByLocomotor[locomotor] 的单实例形态
  // (头注)| the single-instance form of the dictionary (the header note).
  if (hpf_blocked_by_none_ == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  (void)locomotor;
  return hpf_blocked_by_none_->PathExists(source, target);
}

// ———— PathMightExistForLocomotorBlockedByImmovable(L281-284)————
bool PathFinder::PathMightExistForLocomotorBlockedByImmovable(
    Locomotor* locomotor, CPos source, CPos target) {
  if (hpf_blocked_by_immovable_ == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  (void)locomotor;
  return hpf_blocked_by_immovable_->PathExists(source, target);
}

// ———— GetActorLocomotor(L286-291)————
Locomotor* PathFinder::GetActorLocomotor(Actor& self) const {
  // PERF: This PathFinder trait requires the use of Mobile(上游注释段)
  // Mobile 批落地前以注入解析器承载(缺省 = 世界首个 Locomotor —— 头注)
  // Before the Mobile batch, the injected resolver carries it (the default
  // = the world's first Locomotor — the header note).
  if (fn_actor_locomotor_ != nullptr)
    return fn_actor_locomotor_(self);
  throw std::runtime_error("NullReferenceException");
}

}  // namespace ora::mods::pathfinding
