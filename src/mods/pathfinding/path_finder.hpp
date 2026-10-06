// UPSTREAM: OpenRA.Mods.Common/Traits/World/PathFinder.cs @b6fc03f L22-295
//          + Mods.Common/TraitsInterfaces.cs L930-960 的 IPathFinder 接口面
//          (逐语义重写 + Mobile 挂点)
//          Verbatim-semantics rewrite + the Mobile hook face.
//
// 机制对照 / Mechanism mapping:
//  - GetActorLocomotor(L286-291:`(Mobile)self.OccupiesSpace).Locomotor`):
//    Mobile 随下一批 —— 部分覆盖装配面以注入的解析器承载(缺省 = 世界首
//    个 Locomotor;Mobile 批换实 —— COVERAGE 登记)
//    GetActorLocomotor (L286-291): Mobile lands next batch — the
//    partial-coverage assembly face carries an injected resolver (the
//    default = the world's first Locomotor; replaced in the Mobile batch —
//    in COVERAGE).
//  - pathFinderOverlay 的 `?.` 面随调试渲染批(全部为空操作等价)
//    The pathFinderOverlay `?.` faces land with the debug-render batch
//    (all equivalent no-ops).
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

/// IPathFinder(Mods.Common/TraitsInterfaces.cs L930-960)
/// IPathFinder (Mods.Common/TraitsInterfaces.cs L930-960).
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

/// PathFinder(PathFinder.cs L41-294) | PathFinder (PathFinder.cs L41-294).
class PathFinder final : public TraitBase, public IPathFinder, public IWorldLoaded {
 public:
  PathFinder(Actor& self, int heuristic_weight_percentage);  // L51-55

  ORA_TRAIT_INTERFACES(PathFinder, OpenRA_Mods_Common_Traits_PathFinder,
                       IPathFinder, IWorldLoaded)

  void WorldLoaded(World& w, gfx::WorldRenderer* wr) override;  // L65-77

  std::vector<CPos> FindPathToTargetCell(
      Actor* self, std::span<const CPos> sources, CPos target,
      BlockedByActor check, const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) override;  // L97-104

  std::vector<CPos> FindPathToTargetCells(
      Actor* self, CPos source, std::span<const CPos> targets,
      BlockedByActor check, const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) override;  // L124-176

  std::vector<CPos> FindPathToTargetCellByPredicate(
      Actor* self, std::span<const CPos> sources,
      const std::function<bool(CPos)>& target_predicate, BlockedByActor check,
      const std::function<int(CPos)>& custom_cost = {},
      Actor* ignore_actor = nullptr, bool lane_bias = true) override;  // L237-249

  bool PathExistsForLocomotor(Locomotor* locomotor, CPos source,
                              CPos target) override;  // L262-265
  bool PathMightExistForLocomotorBlockedByImmovable(
      Locomotor* locomotor, CPos source, CPos target) override;  // L281-284

  /// GetActorLocomotor 的注入面(见头注)| the GetActorLocomotor injection
  /// face (see the header).
  void SetActorLocomotorResolver(
      std::function<Locomotor*(Actor&)> fn) {
    fn_actor_locomotor_ = std::move(fn);
  }

  int HeuristicWeightPercentage() const {
    return std::max(100, heuristic_weight_percentage_);  // L293
  }

 private:
  /// FindPathToTarget(L178-216) | FindPathToTarget (L178-216).
  std::vector<CPos> FindPathToTarget(Actor* self, std::span<const CPos> sources,
                                     CPos target, BlockedByActor check,
                                     const std::function<int(CPos)>& custom_cost,
                                     Actor* ignore_actor, bool in_reverse,
                                     bool lane_bias);
  /// GetHierarchicalPathFinder(L218-226) | (L218-226).
  HierarchicalPathFinder& GetHierarchicalPathFinder(Locomotor* locomotor,
                                                    BlockedByActor check,
                                                    Actor* ignore_actor) const;
  Locomotor* GetActorLocomotor(Actor& self) const;  // L286-291

  World& world_;                              // L45
  int heuristic_weight_percentage_ = 125;     // L33(Info 面)
  std::unique_ptr<HierarchicalPathFinder>
      hpf_blocked_by_none_;                   // L48 的值形态
  std::unique_ptr<HierarchicalPathFinder>
      hpf_blocked_by_immovable_;              // L49 的值形态
  std::function<Locomotor*(Actor&)> fn_actor_locomotor_;
};

}  // namespace ora::mods::pathfinding
