// UPSTREAM: OpenRA.Mods.Common/Pathfinder/PathSearch.cs @b6fc03f(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - 开队列 = 上游 PriorityQueue<GraphConnection, GraphConnection.CostComparer>
//    的逐语义移植(core/priority_queue.hpp;"层级加倍"堆序保留 —— 平局
//    弹出序进入寻路结果,OPT-A2/C4 警示的确定性前提)
//    The open queue = the verbatim port of upstream's PriorityQueue
//    (core/priority_queue.hpp; the levels-doubled pop order is kept — the
//    tie pop order enters the path result, the determinism precondition of
//    the OPT-A2/C4 warning).
//  - LayerPoolTable(静态 WeakTable)→ cell_info_layer_pool.hpp 的进程级表
//    The static LayerPoolTable → the process-level table in
//    cell_info_layer_pool.hpp.
//  - PathFinderOverlay 的 recorder 面保留为可空接口(overlay 随调试渲染批)
//    The PathFinderOverlay recorder face stays a nullable interface (the
//    overlay lands with the debug-render batch).
#pragma once
import std;

#include "core/priority_queue.hpp"
#include "mods/pathfinding/dense_path_graph.hpp"

namespace ora::sim {
class World;
class Actor;
enum class BlockedByActor : std::int32_t;
}  // namespace ora::sim

namespace ora::mods::pathfinding {

using sim::Actor;

class Locomotor;
class LocomotorInfo;

/// PathSearch.IRecorder(L23-26) | PathSearch.IRecorder (L23-26).
class IRecorder {
 public:
  virtual ~IRecorder() = default;
  virtual void Add(CPos source, CPos destination, int cost_so_far,
                   int estimated_remaining_cost) = 0;
};

/// PathSearch(PathSearch.cs L21-420) | PathSearch (PathSearch.cs L21-420).
class PathSearch final {
 public:
  /// ToTargetCellByPredicate(L38-52) | ToTargetCellByPredicate (L38-52).
  static std::unique_ptr<PathSearch> ToTargetCellByPredicate(
      sim::World& world, Locomotor* locomotor, sim::Actor* self,
      std::span<const CPos> froms,
      const std::function<bool(CPos)>& target_predicate,
      sim::BlockedByActor check, const std::function<int(CPos)>& custom_cost,
      sim::Actor* ignore_actor = nullptr, bool lane_bias = true,
      IRecorder* recorder = nullptr);

  /// ToTargetCell(L54-77) | ToTargetCell (L54-77).
  static std::unique_ptr<PathSearch> ToTargetCell(
      sim::World& world, Locomotor* locomotor, sim::Actor* self,
      std::span<const CPos> froms, CPos target, sim::BlockedByActor check,
      int heuristic_weight_percentage,
      const std::function<int(CPos)>& custom_cost,
      sim::Actor* ignore_actor = nullptr, bool lane_bias = true,
      bool in_reverse = false,
      const std::function<int(CPos, bool)>& heuristic = {},
      const Grid* grid = nullptr, IRecorder* recorder = nullptr);

  /// CellAllowsMovement(L90-95) | CellAllowsMovement (L90-95).
  static bool CellAllowsMovement(sim::World& world, Locomotor* locomotor,
                                 CPos cell,
                                 const std::function<int(CPos)>& custom_cost);

  /// ToTargetCellOverGraph(L112-122) | ToTargetCellOverGraph (L112-122).
  static std::unique_ptr<PathSearch> ToTargetCellOverGraph(
      SparsePathGraph::EdgesFn edges, Locomotor* locomotor, CPos from,
      CPos target, int estimated_search_size = 0, IRecorder* recorder = nullptr);

  /// DefaultCostEstimator(L132-136) | DefaultCostEstimator (L132-136).
  static std::function<int(CPos, bool)> DefaultCostEstimatorForTarget(
      Locomotor* locomotor, CPos destination);
  /// DefaultCostEstimator(L145-161):对角距离启发 | DefaultCostEstimator
  /// (L145-161): the diagonal-distance heuristic.
  static std::function<int(CPos, CPos)> DefaultCostEstimator(
      Locomotor* locomotor);

  /// ExpandToTarget(L302-309) | ExpandToTarget (L302-309).
  bool ExpandToTarget();
  /// ExpandAll(L315-321) | ExpandAll (L315-321).
  std::vector<CPos> ExpandAll();
  /// FindPath(L327-337) | FindPath (L327-337).
  std::vector<CPos> FindPath();

  /// FindBidiPath(L360-380) | FindBidiPath (L360-380).
  static std::vector<CPos> FindBidiPath(PathSearch& first, PathSearch& second);

  /// Graph/TargetPredicate 面(L163-164) | the Graph/TargetPredicate faces
  /// (L163-164).
  IPathGraph& Graph() { return *graph_; }
  const std::function<bool(CPos)>& TargetPredicate() const {
    return fn_target_predicate_;
  }
  void SetTargetPredicate(std::function<bool(CPos)> fn) {
    fn_target_predicate_ = std::move(fn);
  }

  /// Dispose(L416-419)/RAII | Dispose (L416-419)/RAII.
  void Dispose() {
    if (graph_ != nullptr)
      graph_->Dispose();
    graph_.reset();
  }
  ~PathSearch() { Dispose(); }

 private:
  friend class HierarchicalPathFinder;
  friend void AddInitialCells(sim::World&, Locomotor*, sim::Actor*,
                              std::span<const CPos>, sim::BlockedByActor,
                              const std::function<int(CPos)>&, sim::Actor*,
                              bool, PathSearch&);
  friend void AddInitialCells(sim::World&, Locomotor*, sim::Actor*,
                              std::span<const CPos>, sim::BlockedByActor,
                              const std::function<int(CPos)>&, sim::Actor*,
                              bool, PathSearch&);

  /// ctor(L188-196) | the ctor (L188-196).
  PathSearch(std::unique_ptr<IPathGraph> graph,
             std::function<int(CPos, bool)> heuristic,
             int heuristic_weight_percentage,
             std::function<bool(CPos)> target_predicate, IRecorder* recorder);

  /// AddInitialCell(L198-216) | AddInitialCell (L198-216).
  void AddInitialCell(CPos location, const std::function<int(CPos)>& custom_cost);
  /// CanExpand(L222-240) | CanExpand (L222-240).
  bool CanExpand();
  /// Expand(L247-292) | Expand (L247-292).
  CPos Expand();
  /// MakePath(L341-354) | MakePath (L341-354).
  static std::vector<CPos> MakePath(IPathGraph& graph, CPos destination);
  /// MakeBidiPath(L384-414) | MakeBidiPath (L384-414).
  static std::vector<CPos> MakeBidiPath(PathSearch& first, PathSearch& second,
                                        CPos confluence_node);

  std::unique_ptr<IPathGraph> graph_;                        // L163
  std::function<bool(CPos)> fn_target_predicate_;            // L164
  std::function<int(CPos, bool)> fn_heuristic_;              // L165
  int heuristic_weight_percentage_ = 100;                    // L166
  IRecorder* recorder_ = nullptr;                            // L167
  ora::PriorityQueue<GraphConnection, GraphConnection::CostComparer>
      open_queue_{};                                         // L168
};

}  // namespace ora::mods::pathfinding
