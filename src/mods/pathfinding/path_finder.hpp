#pragma once
import std;

#include "mods/pathfinding/hierarchical_path_finder.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::pathfinding {

using sim::Actor;
using sim::BlockedByActor;
using sim::IWorldLoaded;
using sim::TraitBase;
using sim::World;

class IPathFinder {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPathFinder;
  virtual ~IPathFinder() = default;
  virtual std::vector<CPos> FindPathToTargetCell(
      Actor* self, std::span<const CPos> sources, CPos target,
      BlockedByActor check, const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) = 0;
  virtual std::vector<CPos> FindPathToTargetCells(
      Actor* self, CPos source, std::span<const CPos> targets,
      BlockedByActor check, const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) = 0;
  virtual std::vector<CPos> FindPathToTargetCellByPredicate(
      Actor* self, std::span<const CPos> sources,
      const std::function<bool(CPos)>& target_predicate, BlockedByActor check,
      const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) = 0;
  virtual bool PathExistsForLocomotor(Locomotor* locomotor, CPos source,
                                      CPos target) = 0;
  virtual bool PathMightExistForLocomotorBlockedByImmovable(
      Locomotor* locomotor, CPos source, CPos target) = 0;
};

class PathFinder final : public TraitBase, public IPathFinder, public IWorldLoaded {
 public:
  PathFinder(Actor& self, int heuristic_weight_percentage);

  ORA_TRAIT_INTERFACES(PathFinder, OpenRA_Mods_Common_Traits_PathFinder,
                       IPathFinder, IWorldLoaded)

  void WorldLoaded(World& w, gfx::WorldRenderer* wr) override;

  std::vector<CPos> FindPathToTargetCell(
      Actor* self, std::span<const CPos> sources, CPos target,
      BlockedByActor check, const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) override;

  std::vector<CPos> FindPathToTargetCells(
      Actor* self, CPos source, std::span<const CPos> targets,
      BlockedByActor check, const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) override;

  std::vector<CPos> FindPathToTargetCellByPredicate(
      Actor* self, std::span<const CPos> sources,
      const std::function<bool(CPos)>& target_predicate, BlockedByActor check,
      const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) override;

  bool PathExistsForLocomotor(Locomotor* locomotor, CPos source,
                              CPos target) override;
  bool PathMightExistForLocomotorBlockedByImmovable(
      Locomotor* locomotor, CPos source, CPos target) override;

  void SetActorLocomotorResolver(
      std::function<Locomotor*(Actor&)> fn) {
    fn_actor_locomotor_ = std::move(fn);
  }

  int HeuristicWeightPercentage() const {
    return std::max(100, heuristic_weight_percentage_);
  }

 private:

  std::vector<CPos> FindPathToTarget(Actor* self, std::span<const CPos> sources,
                                     CPos target, BlockedByActor check,
                                     const std::function<int(CPos)>& custom_cost,
                                     Actor* ignore_actor, bool in_reverse,
                                     bool lane_bias);

  HierarchicalPathFinder& GetHierarchicalPathFinder(Locomotor* locomotor,
                                                    BlockedByActor check,
                                                    Actor* ignore_actor) const;
  Locomotor* GetActorLocomotor(Actor& self) const;

  World& world_;
  int heuristic_weight_percentage_ = 125;
  std::unique_ptr<HierarchicalPathFinder>
      hpf_blocked_by_none_;
  std::unique_ptr<HierarchicalPathFinder>
      hpf_blocked_by_immovable_;
  std::function<Locomotor*(Actor&)> fn_actor_locomotor_;
};

}
