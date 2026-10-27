#pragma once
import std;

#include "core/priority_queue.hpp"
#include "mods/pathfinding/dense_path_graph.hpp"

namespace ora::sim {
class World;
class Actor;
enum class BlockedByActor : std::int32_t;
}

namespace ora::mods::pathfinding {

using sim::Actor;

class Locomotor;
class LocomotorInfo;

class IRecorder {
 public:
  virtual ~IRecorder() = default;
  virtual void Add(CPos source, CPos destination, int cost_so_far,
                   int estimated_remaining_cost) = 0;
};

class PathSearch final {
 public:

  static std::unique_ptr<PathSearch> ToTargetCellByPredicate(
      sim::World& world, Locomotor* locomotor, sim::Actor* self,
      std::span<const CPos> froms,
      const std::function<bool(CPos)>& target_predicate,
      sim::BlockedByActor check, const std::function<int(CPos)>& custom_cost,
      sim::Actor* ignore_actor = nullptr, bool lane_bias = true,
      IRecorder* recorder = nullptr);

  static std::unique_ptr<PathSearch> ToTargetCell(
      sim::World& world, Locomotor* locomotor, sim::Actor* self,
      std::span<const CPos> froms, CPos target, sim::BlockedByActor check,
      int heuristic_weight_percentage,
      const std::function<int(CPos)>& custom_cost,
      sim::Actor* ignore_actor = nullptr, bool lane_bias = true,
      bool in_reverse = false,
      const std::function<int(CPos, bool)>& heuristic = {},
      const Grid* grid = nullptr, IRecorder* recorder = nullptr);

  static bool CellAllowsMovement(sim::World& world, Locomotor* locomotor,
                                 CPos cell,
                                 const std::function<int(CPos)>& custom_cost);

  static std::unique_ptr<PathSearch> ToTargetCellOverGraph(
      SparsePathGraph::EdgesFn edges, Locomotor* locomotor, CPos from,
      CPos target, int estimated_search_size = 0, IRecorder* recorder = nullptr);

  static std::function<int(CPos, bool)> DefaultCostEstimatorForTarget(
      Locomotor* locomotor, CPos destination);

  static std::function<int(CPos, CPos)> DefaultCostEstimator(
      Locomotor* locomotor);

  bool ExpandToTarget();

  std::vector<CPos> ExpandAll();

  std::vector<CPos> FindPath();

  static std::vector<CPos> FindBidiPath(PathSearch& first, PathSearch& second);

  IPathGraph& Graph() { return *graph_; }
  const std::function<bool(CPos)>& TargetPredicate() const {
    return fn_target_predicate_;
  }
  void SetTargetPredicate(std::function<bool(CPos)> fn) {
    fn_target_predicate_ = std::move(fn);
  }

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

  PathSearch(std::unique_ptr<IPathGraph> graph,
             std::function<int(CPos, bool)> heuristic,
             int heuristic_weight_percentage,
             std::function<bool(CPos)> target_predicate, IRecorder* recorder);

  void AddInitialCell(CPos location, const std::function<int(CPos)>& custom_cost);

  bool CanExpand();

  CPos Expand();

  static std::vector<CPos> MakePath(IPathGraph& graph, CPos destination);

  static std::vector<CPos> MakeBidiPath(PathSearch& first, PathSearch& second,
                                        CPos confluence_node);

  std::unique_ptr<IPathGraph> graph_;
  std::function<bool(CPos)> fn_target_predicate_;
  std::function<int(CPos, bool)> fn_heuristic_;
  int heuristic_weight_percentage_ = 100;
  IRecorder* recorder_ = nullptr;
  ora::PriorityQueue<GraphConnection, GraphConnection::CostComparer>
      open_queue_{};
};

}
