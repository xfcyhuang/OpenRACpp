#pragma once
import std;

#include "mods/pathfinding/dense_path_graph.hpp"
#include "mods/pathfinding/path_search.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::pathfinding {

using sim::Actor;
using sim::BlockedByActor;
using sim::IActorMap;
class Locomotor;

struct GridInfo {

  std::optional<CPos> AbstractCellForLocalCell(
      CPos local_cell, class HierarchicalPathFinder* hpf) const;

  void CopyAbstractCellsInto(std::vector<CPos>& set) const;

  std::vector<std::optional<CPos>> single_abstract_cell_for_layer_;
  std::unordered_map<CPos, CPos> map_local_cell_to_abstract_cell_;
};

class HierarchicalPathFinder final {
  friend struct GridInfo;
 public:
  static constexpr int kGridSize = 10;

  HierarchicalPathFinder(sim::World& world, Locomotor* locomotor,
                         sim::IActorMap* actor_map, sim::BlockedByActor check);

  std::vector<CPos> FindPath(sim::Actor* self, CPos source, CPos target,
                             sim::BlockedByActor check,
                             int heuristic_weight_percentage,
                             const std::function<int(CPos)>& custom_cost,
                             sim::Actor* ignore_actor, bool in_reverse,
                             bool lane_bias);

  std::vector<CPos> FindPath(sim::Actor* self, std::span<const CPos> sources,
                             CPos target, sim::BlockedByActor check,
                             int heuristic_weight_percentage,
                             const std::function<int(CPos)>& custom_cost,
                             sim::Actor* ignore_actor, bool in_reverse,
                             bool lane_bias);

  bool PathExists(CPos source, CPos target);

  const std::unordered_map<CPos, std::vector<GraphConnection>>*
  AbstractGraphIfReady() const {
    return fn_cost_estimator_ == nullptr ? nullptr : &map_abstract_graph_;
  }

 private:

  class AbstractGraphWithInsertedEdges {
   public:
    AbstractGraphWithInsertedEdges(
        const std::unordered_map<CPos, std::vector<GraphConnection>>&
            abstract_edges,
        std::span<const GraphEdge> source_edges,
        const GraphEdge* target_edge,
        const std::function<int(CPos, CPos)>& cost_estimator);

    std::vector<GraphConnection> GetConnections(CPos position) const;

   private:

    void InsertEdgeAsBidirectional(const GraphEdge& edge,
                                   const std::function<int(CPos, CPos)>& cost_estimator);
    void InsertConnections(CPos local_cell, CPos abstract_cell,
                           const std::function<int(CPos, CPos)>& cost_estimator);

    const std::unordered_map<CPos, std::vector<GraphConnection>>&
        map_abstract_edges_;
    std::unordered_map<CPos, std::vector<GraphConnection>> map_changed_edges_;
  };

  void BuildGrids();

  GridInfo BuildGrid(int grid_x, int grid_y,
                     std::span<sim::ICustomMovementLayer* const> custom_movement_layers);

  void BuildCostTable();

  std::vector<std::pair<CPos, std::vector<GraphConnection>>>
  GetAbstractEdgesForGrid(int grid_x, int grid_y,
                          std::span<sim::ICustomMovementLayer* const> custom_movement_layers);

  void RequireCostRefreshInCell(CPos cell, short old_cost, short new_cost);

  void MarkGridDirty(int grid_index);

  bool CellIsAccessible(CPos cell) const;

  bool MovementAllowedBetweenCells(CPos accessible_src_cell, CPos dest_cell) const;

  void RequireBlockingRefreshInCell(CPos cell);

  void RequireProjectionRefreshInCell(CPos cell);

  bool ActorIsBlocking(Actor* actor) const;

  bool ActorCellIsBlocking(Actor* actor, CPos cell) const;

  int GridIndex(CPos cell_in_grid) const;

  CPos GetGridTopLeft(int grid_index, std::uint8_t layer) const;
  static CPos GetGridTopLeft(CPos cell_in_grid, const Grid& map_bounds);
  static Grid GetGrid(CPos cell_in_grid, const Grid& map_bounds);

  void RebuildDirtyGrids();

  void RebuildCostTable(int grid_x, int grid_y, const GridInfo& old_grid,
                        std::span<sim::ICustomMovementLayer* const> custom_movement_layers);

  void RebuildDomains();

  std::optional<CPos> AbstractCellForLocalCell(CPos local_cell);
  std::optional<CPos> AbstractCellForLocalCellNoAccessibleCheck(CPos local_cell);

  std::optional<GraphEdge> EdgeFromLocalToAbstract(CPos local_cell,
                                                   CPos abstract_cell) const;

  std::function<int(CPos, bool)> MakeHeuristic(
      PathSearch& abstract_search, int estimated_search_size,
      const std::vector<CPos>* sources,
      const std::vector<CPos>* unpathable_nodes);

  CPos AbstractNodeForCost(const SparsePathGraph& graph, CPos abstract_cell,
                           CPos abstract_node);

  std::unique_ptr<PathSearch> GetLocalPathSearch(
      Actor* self, std::span<const CPos> srcs, CPos dst,
      const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
      BlockedByActor check, bool lane_bias, const Grid* grid,
      int heuristic_weight_percentage,
      const std::function<int(CPos, bool)>& heuristic, bool in_reverse,
      IRecorder* recorder);

  sim::World& world_;
  Locomotor* locomotor_ = nullptr;
  IActorMap* actor_map_ = nullptr;
  std::function<int(CPos, CPos)> fn_cost_estimator_;
  std::vector<int> vec_dirty_grid_indexes_;
  std::vector<std::uint8_t> vec_dirty_grid_bitmap_;
  std::unordered_set<CPos> set_cells_with_blocking_actor_;
  Grid map_bounds_{};
  int grid_xs_ = 0;
  int grid_ys_ = 0;
  std::vector<GridInfo> vec_grid_infos_;
  std::unordered_map<CPos, std::vector<GraphConnection>>
      map_abstract_graph_;
  std::unordered_map<CPos, std::uint32_t> map_abstract_domains_;
};

}
