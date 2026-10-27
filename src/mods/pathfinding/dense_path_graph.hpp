#pragma once
import std;

#include "map/map.hpp"
#include "mods/pathfinding/cell_info_layer_pool.hpp"
#include "sim/world.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::pathfinding {

class Locomotor;
class LocomotorInfo;

class DensePathGraph : public IPathGraph {
 public:
  ~DensePathGraph() override { Dispose(); }

  static constexpr int kLaneBiasCost = 1;

  DensePathGraph(Locomotor* locomotor, sim::Actor* actor,
                 sim::World& world, sim::BlockedByActor check,
                 std::function<int(CPos)> custom_cost, sim::Actor* ignore_actor,
                 bool lane_bias, bool in_reverse);

  std::vector<GraphConnection>& GetConnections(
      CPos position,
      const std::function<bool(CPos)>& target_predicate) override;

  void Dispose() override {}

 protected:

  virtual bool IsValidNeighbor(CPos) const { return true; }

  std::span<sim::ICustomMovementLayer* const> custom_movement_layers_;
  int custom_movement_layers_enabled_for_locomotor_ = 0;
  Locomotor* locomotor_;
  sim::Actor* actor_ = nullptr;
  sim::World& world_;
  sim::BlockedByActor check_;
  std::function<int(CPos)> custom_cost_;
  sim::Actor* ignore_actor_ = nullptr;
  bool lane_bias_ = false;
  bool in_reverse_ = false;
  bool check_terrain_height_ = false;

  static const std::vector<std::vector<CVec>>& DirectedNeighbors();
  static const std::vector<std::vector<CVec>>& DirectedNeighborsConservative();

  std::vector<GraphConnection> vec_scratch_;

  bool CanEnterNode(CPos src_node, CPos dest_node,
                    const std::function<bool(CPos)>& target_predicate) const;

  int GetPathCostToNode(CPos src_node, CPos dest_node, CVec direction,
                        const std::function<bool(CPos)>& target_predicate) const;

  int CalculateCellPathCost(CPos neighbor_c_pos, CVec direction,
                            short movement_cost) const;
};

class MapPathGraph final : public DensePathGraph {
 public:
  MapPathGraph(CellInfoLayerPool& layer_pool,
               Locomotor* locomotor, sim::Actor* actor,
               sim::World& world, sim::BlockedByActor check,
               std::function<int(CPos)> custom_cost, sim::Actor* ignore_actor,
               bool lane_bias, bool in_reverse);

  CellInfo At(CPos pos) const override;
  void Set(CPos pos, CellInfo info) override;

  void Dispose() override;

 private:
  CellInfoLayerPool::PooledCellInfoLayer pooled_layer_;
  std::vector<StampedCellInfoLayer*> vec_cell_info_for_layer_;
};

class GridPathGraph final : public DensePathGraph {
 public:
  GridPathGraph(Locomotor* locomotor, sim::Actor* actor,
                sim::World& world, sim::BlockedByActor check,
                std::function<int(CPos)> custom_cost, sim::Actor* ignore_actor,
                bool lane_bias, bool in_reverse, const Grid& grid);

  CellInfo At(CPos pos) const override;
  void Set(CPos pos, CellInfo info) override;

 protected:
  bool IsValidNeighbor(CPos neighbor) const override;

 private:
  int InfoIndex(CPos pos) const;

  std::vector<CellInfo> vec_infos_;
  Grid grid_;
};

class SparsePathGraph final : public IPathGraph {
 public:
  using EdgesFn = std::function<std::vector<GraphConnection>(CPos)>;

  SparsePathGraph(EdgesFn edges, int estimated_search_size = 0)
      : fn_edges_{std::move(edges)} {
    map_info_.reserve(static_cast<std::size_t>(estimated_search_size));
  }

  std::vector<GraphConnection>& GetConnections(
      CPos position,
      const std::function<bool(CPos)>& target_predicate) override {
    vec_scratch_ = fn_edges_(position);
    return vec_scratch_;
  }

  CellInfo At(CPos pos) const override {
    auto it = map_info_.find(pos);
    return it != map_info_.end() ? it->second : CellInfo{};
  }
  void Set(CPos pos, CellInfo info) override { map_info_[pos] = info; }

  void Dispose() override {}

 private:
  EdgesFn fn_edges_;
  std::unordered_map<CPos, CellInfo> map_info_;
  std::vector<GraphConnection> vec_scratch_;
};

}
