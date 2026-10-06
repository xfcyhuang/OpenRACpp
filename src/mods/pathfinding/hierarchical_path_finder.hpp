// UPSTREAM: OpenRA.Mods.Common/Pathfinder/HierarchicalPathFinder.cs
//          @b6fc03f L97-1284(逐语义重写 + OPT-A2/A10 面)
//          Verbatim-semantics rewrite + the OPT-A2/A10 faces.
//
// 机制对照 / Mechanism mapping:
//  - dirtyGridIndexes(L106:HashSet<int> 稠密小整数)→ 插入序 vector + 位图
//    判重(枚举序 = 上游 HashSet 实现序的无删除形态 = 插入序;A10 的位图
//    只承担成员测试,不承担枚举序 —— 相邻格重建次序可观测,序保真优先)
//    dirtyGridIndexes (L106) → an insertion-ordered vector + a membership
//    bitmap (the enumeration order = upstream's no-deletion HashSet order
//    = insertion order; A10's bitmap only carries membership — the
//    neighbor-rebuild order is observable, order fidelity wins).
//  - cellsWithBlockingActor(L107)→ unordered_set(消费面仅 Contains/Add/
//    Remove,无序依赖)| cellsWithBlockingActor (L107) → an unordered_set
//    (consumed via Contains/Add/Remove only — no order dependence).
//  - PathFinderOverlay 形参(L734 等)随调试渲染批 —— 上游全部 `?.` 调用
//    在无 overlay 时为空操作,删参即无 overlay 语义的等价形态(COVERAGE)
//    The PathFinderOverlay parameters (L734 etc.) land with the debug-
//    render batch — upstream's calls are all `?.` no-ops without an
//    overlay, so dropping the parameter is the equivalent no-overlay form
//    (COVERAGE).
//  - ActorIsBlocking/ActorCellIsBlocking 的 Mobile/Building/ITemporaryBlocker
//    挂点(L658-662/685)同 Locomotor 的部分覆盖装配面(COVERAGE)
//    The ActorIsBlocking/ActorCellIsBlocking Mobile/Building/
//    ITemporaryBlocker hooks (L658-662/685) share Locomotor's
//    partial-coverage assembly face (COVERAGE).
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

/// GridInfo(L136-178;readonly struct) | GridInfo (L136-178).
struct GridInfo {
  /// AbstractCellForLocalCell(L152-168):hpf 空 = 跳过可达性检查
  /// AbstractCellForLocalCell (L152-168): a null hpf skips the cost check.
  std::optional<CPos> AbstractCellForLocalCell(
      CPos local_cell, class HierarchicalPathFinder* hpf) const;

  /// CopyAbstractCellsInto(L170-177) | CopyAbstractCellsInto (L170-177).
  void CopyAbstractCellsInto(std::vector<CPos>& set) const;

  std::vector<std::optional<CPos>> single_abstract_cell_for_layer_;  // L138
  std::unordered_map<CPos, CPos> map_local_cell_to_abstract_cell_;   // L139
};

/// HierarchicalPathFinder(L97-1284) | HierarchicalPathFinder (L97-1284).
class HierarchicalPathFinder final {
  friend struct GridInfo;
 public:
  static constexpr int kGridSize = 10;  // L100

  /// ctor(L246-288) | the ctor (L246-288).
  HierarchicalPathFinder(sim::World& world, Locomotor* locomotor,
                         sim::IActorMap* actor_map, sim::BlockedByActor check);

  /// FindPath 单源(L833-930:双向搜索)| the single-source FindPath
  /// (L833-930: the bidirectional search).
  std::vector<CPos> FindPath(sim::Actor* self, CPos source, CPos target,
                             sim::BlockedByActor check,
                             int heuristic_weight_percentage,
                             const std::function<int(CPos)>& custom_cost,
                             sim::Actor* ignore_actor, bool in_reverse,
                             bool lane_bias);
  /// FindPath 多源(L732-826:单向搜索)| the multi-source FindPath
  /// (L732-826: the unidirectional search).
  std::vector<CPos> FindPath(sim::Actor* self, std::span<const CPos> sources,
                             CPos target, sim::BlockedByActor check,
                             int heuristic_weight_percentage,
                             const std::function<int(CPos)>& custom_cost,
                             sim::Actor* ignore_actor, bool in_reverse,
                             bool lane_bias);

  /// PathExists(L942-984) | PathExists (L942-984).
  bool PathExists(CPos source, CPos target);

  /// GetOverlayData 的数据面(L290-303;调试渲染批消费)
  /// The GetOverlayData faces (L290-303; consumed by the debug-render
  /// batch).
  const std::unordered_map<CPos, std::vector<GraphConnection>>*
  AbstractGraphIfReady() const {
    return fn_cost_estimator_ == nullptr ? nullptr : &map_abstract_graph_;
  }

 private:
  /// AbstractGraphWithInsertedEdges(L185-244):链式延迟求值 → 插入期物化
  /// (基表在图生命期内不变 —— FindPath 的局部使用面;语义逐条等价)
  /// AbstractGraphWithInsertedEdges (L185-244): the lazy LINQ chains →
  /// eager materialization at insert (the base tables don't change within
  /// the graph's lifetime — a per-FindPath local face; item-for-item
  /// equivalence).
  class AbstractGraphWithInsertedEdges {
   public:
    AbstractGraphWithInsertedEdges(
        const std::unordered_map<CPos, std::vector<GraphConnection>>&
            abstract_edges,
        std::span<const GraphEdge> source_edges,
        const GraphEdge* target_edge,
        const std::function<int(CPos, CPos)>& cost_estimator);

    /// GetConnections(L236-243) | GetConnections (L236-243).
    std::vector<GraphConnection> GetConnections(CPos position) const;

   private:
    /// InsertEdgeAsBidirectional/InsertConnections(L204-234)
    /// InsertEdgeAsBidirectional / InsertConnections (L204-234).
    void InsertEdgeAsBidirectional(const GraphEdge& edge,
                                   const std::function<int(CPos, CPos)>& cost_estimator);
    void InsertConnections(CPos local_cell, CPos abstract_cell,
                           const std::function<int(CPos, CPos)>& cost_estimator);

    const std::unordered_map<CPos, std::vector<GraphConnection>>&
        map_abstract_edges_;
    std::unordered_map<CPos, std::vector<GraphConnection>> map_changed_edges_;
  };

  /// BuildGrids(L308-334) | BuildGrids (L308-334).
  void BuildGrids();
  /// BuildGrid(L343-448) | BuildGrid (L343-448).
  GridInfo BuildGrid(int grid_x, int grid_y,
                     std::span<sim::ICustomMovementLayer* const> custom_movement_layers);
  /// BuildCostTable(L454-462) | BuildCostTable (L454-462).
  void BuildCostTable();
  /// GetAbstractEdgesForGrid(L469-577) | GetAbstractEdgesForGrid (L469-577).
  std::vector<std::pair<CPos, std::vector<GraphConnection>>>
  GetAbstractEdgesForGrid(int grid_x, int grid_y,
                          std::span<sim::ICustomMovementLayer* const> custom_movement_layers);
  /// RequireCostRefreshInCell(L582-589) | RequireCostRefreshInCell.
  void RequireCostRefreshInCell(CPos cell, short old_cost, short new_cost);
  /// HashSet<int>.Add 的插入序 + 位图判重形态(头注)| the insertion-order +
  /// bitmap form of HashSet<int>.Add (the header note).
  void MarkGridDirty(int grid_index);
  /// CellIsAccessible(L591-595) | CellIsAccessible (L591-595).
  bool CellIsAccessible(CPos cell) const;
  /// MovementAllowedBetweenCells(L597-602) | (L597-602).
  bool MovementAllowedBetweenCells(CPos accessible_src_cell, CPos dest_cell) const;
  /// RequireBlockingRefreshInCell(L607-630) | (L607-630).
  void RequireBlockingRefreshInCell(CPos cell);
  /// RequireProjectionRefreshInCell(L635-638) | (L635-638).
  void RequireProjectionRefreshInCell(CPos cell);
  /// ActorIsBlocking(L656-671) | ActorIsBlocking (L656-671).
  bool ActorIsBlocking(Actor* actor) const;
  /// ActorCellIsBlocking(L679-690) | ActorCellIsBlocking (L679-690).
  bool ActorCellIsBlocking(Actor* actor, CPos cell) const;
  /// GridIndex(L692-697) | GridIndex (L692-697).
  int GridIndex(CPos cell_in_grid) const;
  /// GetGridTopLeft/GetGrid(L699-725) | (L699-725).
  CPos GetGridTopLeft(int grid_index, std::uint8_t layer) const;
  static CPos GetGridTopLeft(CPos cell_in_grid, const Grid& map_bounds);
  static Grid GetGrid(CPos cell_in_grid, const Grid& map_bounds);
  /// RebuildDirtyGrids(L990-1008) | RebuildDirtyGrids (L990-1008).
  void RebuildDirtyGrids();
  /// RebuildCostTable(L1014-1050) | RebuildCostTable (L1014-1050).
  void RebuildCostTable(int grid_x, int grid_y, const GridInfo& old_grid,
                        std::span<sim::ICustomMovementLayer* const> custom_movement_layers);
  /// RebuildDomains(L1056-1092) | RebuildDomains (L1056-1092).
  void RebuildDomains();
  /// AbstractCellForLocalCell(L1098-1111) | (L1098-1111).
  std::optional<CPos> AbstractCellForLocalCell(CPos local_cell);
  std::optional<CPos> AbstractCellForLocalCellNoAccessibleCheck(CPos local_cell);
  /// EdgeFromLocalToAbstract(L1117-1123) | (L1117-1123).
  std::optional<GraphEdge> EdgeFromLocalToAbstract(CPos local_cell,
                                                   CPos abstract_cell) const;
  /// Heuristic(L1130-1215) | Heuristic (L1130-1215).
  std::function<int(CPos, bool)> MakeHeuristic(
      PathSearch& abstract_search, int estimated_search_size,
      const std::vector<CPos>* sources,
      const std::vector<CPos>* unpathable_nodes);
  /// AbstractNodeForCost(L1223-1270) | AbstractNodeForCost (L1223-1270).
  CPos AbstractNodeForCost(const SparsePathGraph& graph, CPos abstract_cell,
                           CPos abstract_node);
  /// GetLocalPathSearch(L1272-1282) | GetLocalPathSearch (L1272-1282).
  std::unique_ptr<PathSearch> GetLocalPathSearch(
      Actor* self, std::span<const CPos> srcs, CPos dst,
      const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
      BlockedByActor check, bool lane_bias, const Grid* grid,
      int heuristic_weight_percentage,
      const std::function<int(CPos, bool)>& heuristic, bool in_reverse,
      IRecorder* recorder);

  sim::World& world_;                              // L102
  Locomotor* locomotor_ = nullptr;           // L103
  IActorMap* actor_map_ = nullptr;                 // L104
  std::function<int(CPos, CPos)> fn_cost_estimator_;  // L105
  std::vector<int> vec_dirty_grid_indexes_;        // L106(插入序 + 位图)
  std::vector<std::uint8_t> vec_dirty_grid_bitmap_;
  std::unordered_set<CPos> set_cells_with_blocking_actor_;  // L107
  Grid map_bounds_{};                              // L108
  int grid_xs_ = 0;                                // L109
  int grid_ys_ = 0;                                // L110
  std::vector<GridInfo> vec_grid_infos_;           // L115
  std::unordered_map<CPos, std::vector<GraphConnection>>
      map_abstract_graph_;                         // L123
  std::unordered_map<CPos, std::uint32_t> map_abstract_domains_;  // L131
};

}  // namespace ora::mods::pathfinding
