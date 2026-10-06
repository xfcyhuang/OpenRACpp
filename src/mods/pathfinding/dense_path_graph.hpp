// UPSTREAM: OpenRA.Mods.Common/Pathfinder/DensePathGraph.cs @b6fc03f +
//          MapPathGraph.cs + GridPathGraph.cs + SparsePathGraph.cs
//          (逐语义重写 + OPT-A2)
//          Verbatim-semantics rewrites + OPT-A2.
//
// OPT-A2(docs/OPTIMIZATION_TRACKER.md):上游 GetConnections 每节点展开一次
// `new List<GraphConnection>`(DensePathGraph.cs L123 —— 千节点搜索 = 千次
// 分配)。C++ 侧以**成员暂存缓冲**承载返回契约(每次展开先 Clear;消费
// 在单次 Expand 内闭合,无重入)—— 零分配等价;邻居表(DirectedNeighbors/
// DirectedNeighborsConservative)逐项照抄
// OPT-A2 (the optimization tracker): upstream allocates a
// `new List<GraphConnection>` once per node expansion (DensePathGraph.cs
// L123 — a thousand-node search = a thousand allocations). The C++ side
// carries the return contract with a **member scratch buffer** (cleared per
// expansion; consumed inside one Expand, no reentrancy) — the allocation-
// free equivalent; the neighbor tables (DirectedNeighbors /
// DirectedNeighborsConservative) are copied item for item.
#pragma once
import std;

#include "map/map.hpp"
#include "mods/pathfinding/cell_info_layer_pool.hpp"
#include "sim/world.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::pathfinding {

class Locomotor;
class LocomotorInfo;

/// DensePathGraph(DensePathGraph.cs L24-234;abstract 类 → 本类承载)
/// DensePathGraph (DensePathGraph.cs L24-234; the abstract class → this
/// class carries it).
class DensePathGraph : public IPathGraph {
 public:
  ~DensePathGraph() override { Dispose(); }

  static constexpr int kLaneBiasCost = 1;  // L26

  DensePathGraph(Locomotor* locomotor, sim::Actor* actor,
                 sim::World& world, sim::BlockedByActor check,
                 std::function<int(CPos)> custom_cost, sim::Actor* ignore_actor,
                 bool lane_bias, bool in_reverse);

  /// GetConnections(L107-172):返回成员暂存缓冲(见头注)
  /// GetConnections (L107-172): returns the member scratch buffer (see
  /// the header).
  std::vector<GraphConnection>& GetConnections(
      CPos position,
      const std::function<bool(CPos)>& target_predicate) override;

  void Dispose() override {}  // L230-233

 protected:
  /// IsValidNeighbor(L63-66) | IsValidNeighbor (L63-66).
  virtual bool IsValidNeighbor(CPos) const { return true; }

  /// CustomMovementLayers(L28) | CustomMovementLayers (L28).
  std::span<sim::ICustomMovementLayer* const> custom_movement_layers_;
  int custom_movement_layers_enabled_for_locomotor_ = 0;  // L29
  Locomotor* locomotor_;                  // L30
  sim::Actor* actor_ = nullptr;                           // L31
  sim::World& world_;                                     // L32
  sim::BlockedByActor check_;                             // L33
  std::function<int(CPos)> custom_cost_;                  // L34
  sim::Actor* ignore_actor_ = nullptr;                    // L35
  bool lane_bias_ = false;                                // L36
  bool in_reverse_ = false;                               // L37
  bool check_terrain_height_ = false;                     // L38

  /// 邻居表(L75-105;逐项照抄)| the neighbor tables (L75-105; copied
  /// item for item).
  static const std::vector<std::vector<CVec>>& DirectedNeighbors();
  static const std::vector<std::vector<CVec>>& DirectedNeighborsConservative();

  /// OPT-A2 暂存缓冲 | the OPT-A2 scratch buffer.
  std::vector<GraphConnection> vec_scratch_;

  /// CanEnterNode(L174-180) | CanEnterNode (L174-180).
  bool CanEnterNode(CPos src_node, CPos dest_node,
                    const std::function<bool(CPos)>& target_predicate) const;
  /// GetPathCostToNode(L182-195) | GetPathCostToNode (L182-195).
  int GetPathCostToNode(CPos src_node, CPos dest_node, CVec direction,
                        const std::function<bool(CPos)>& target_predicate) const;
  /// CalculateCellPathCost(L197-226) | CalculateCellPathCost (L197-226).
  int CalculateCellPathCost(CPos neighbor_c_pos, CVec direction,
                            short movement_cost) const;
};

/// MapPathGraph(MapPathGraph.cs L21-55) | MapPathGraph (MapPathGraph.cs
/// L21-55).
class MapPathGraph final : public DensePathGraph {
 public:
  MapPathGraph(CellInfoLayerPool& layer_pool,
               Locomotor* locomotor, sim::Actor* actor,
               sim::World& world, sim::BlockedByActor check,
               std::function<int(CPos)> custom_cost, sim::Actor* ignore_actor,
               bool lane_bias, bool in_reverse);

  CellInfo At(CPos pos) const override;
  void Set(CPos pos, CellInfo info) override;

  void Dispose() override;  // L48-54:池句柄归还(RAII 双保险)

 private:
  CellInfoLayerPool::PooledCellInfoLayer pooled_layer_;  // L23
  std::vector<StampedCellInfoLayer*> vec_cell_info_for_layer_;  // L24
};

/// GridPathGraph(GridPathGraph.cs L22-54) | GridPathGraph (GridPathGraph.cs
/// L22-54).
class GridPathGraph final : public DensePathGraph {
 public:
  GridPathGraph(Locomotor* locomotor, sim::Actor* actor,
                sim::World& world, sim::BlockedByActor check,
                std::function<int(CPos)> custom_cost, sim::Actor* ignore_actor,
                bool lane_bias, bool in_reverse, const Grid& grid);

  CellInfo At(CPos pos) const override;
  void Set(CPos pos, CellInfo info) override;

 protected:
  bool IsValidNeighbor(CPos neighbor) const override;  // L35-39

 private:
  int InfoIndex(CPos pos) const;  // L41-46

  std::vector<CellInfo> vec_infos_;  // L24
  Grid grid_;                        // L25
};

/// SparsePathGraph(SparsePathGraph.cs L24-52) | SparsePathGraph
/// (SparsePathGraph.cs L24-52).
class SparsePathGraph final : public IPathGraph {
 public:
  using EdgesFn = std::function<std::vector<GraphConnection>(CPos)>;

  SparsePathGraph(EdgesFn edges, int estimated_search_size = 0)
      : fn_edges_{std::move(edges)} {
    map_info_.reserve(static_cast<std::size_t>(estimated_search_size));
  }

  /// GetConnections(L35-38):null → 空 | GetConnections (L35-38):
  /// null → empty.
  std::vector<GraphConnection>& GetConnections(
      CPos position,
      const std::function<bool(CPos)>& target_predicate) override {
    vec_scratch_ = fn_edges_(position);
    return vec_scratch_;
  }

  /// this[pos](L40-49):缺省 = default CellInfo | the indexer (L40-49):
  /// the default CellInfo when missing.
  CellInfo At(CPos pos) const override {
    auto it = map_info_.find(pos);
    return it != map_info_.end() ? it->second : CellInfo{};
  }
  void Set(CPos pos, CellInfo info) override { map_info_[pos] = info; }

  void Dispose() override {}  // L51

 private:
  EdgesFn fn_edges_;
  std::unordered_map<CPos, CellInfo> map_info_;
  std::vector<GraphConnection> vec_scratch_;
};

}  // namespace ora::mods::pathfinding
