// UPSTREAM: OpenRA.Mods.Common/Pathfinder/DensePathGraph.cs @b6fc03f +
//          MapPathGraph.cs + GridPathGraph.cs + SparsePathGraph.cs
//          (实现部分)
//          The implementation halves.
import std;

#include "mods/pathfinding/dense_path_graph.hpp"

#include "core/exts_math.hpp"
#include "sim/actor.hpp"
#include "mods/pathfinding/locomotor.hpp"

namespace ora::mods::pathfinding {

using sim::BlockedByActor;

// ———— 邻居表(L75-105;逐项照抄)————
const std::vector<std::vector<CVec>>& DensePathGraph::DirectedNeighbors() {
  // Sets of neighbors for each incoming direction. These exclude the
  // neighbors which are guaranteed to be reached more cheaply by a path
  // through our parent cell which does not include the current cell.
  // (上游注释)| (upstream comment)
  static const std::vector<std::vector<CVec>> vec = {
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}, CVec{-1, 0}, CVec{-1, 1}},  // TL
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}},                            // T
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}, CVec{1, 0}, CVec{1, 1}},    // TR
      {CVec{-1, -1}, CVec{-1, 0}, CVec{-1, 1}},                            // L
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}, CVec{-1, 0},
       CVec{1, 0}, CVec{-1, 1}, CVec{0, 1}, CVec{1, 1}},  // CVec.Directions
      {CVec{1, -1}, CVec{1, 0}, CVec{1, 1}},              // R
      {CVec{-1, -1}, CVec{-1, 0}, CVec{-1, 1}, CVec{0, 1},
       CVec{1, 1}},                                        // BL
      {CVec{-1, 1}, CVec{0, 1}, CVec{1, 1}},               // B
      {CVec{1, -1}, CVec{1, 0}, CVec{-1, 1}, CVec{0, 1}, CVec{1, 1}},  // BR
  };
  return vec;
}

const std::vector<std::vector<CVec>>&
DensePathGraph::DirectedNeighborsConservative() {
  // With height discontinuities between the parent and current cell, we
  // cannot optimize the possible neighbors.(上游注释段;仅排除父元方向)
  // With height discontinuities between the parent and current cell, we
  // cannot optimize the possible neighbors. (upstream comment; only the
  // parent directions are excluded)
  static const std::vector<std::vector<CVec>> vec = [] {
    const std::vector<CVec> directions = {
        CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}, CVec{-1, 0},
        CVec{1, 0},   CVec{-1, 1}, CVec{0, 1},  CVec{1, 1}};
    const auto exclude = [&](CVec v) {
      std::vector<CVec> out;
      for (const CVec& d : directions)
        if (!(d == v))
          out.push_back(d);
      return out;
    };
    return std::vector<std::vector<CVec>>{
        exclude(CVec{1, 1}),   // TL
        exclude(CVec{0, 1}),   // T
        exclude(CVec{-1, 1}),  // TR
        exclude(CVec{1, 0}),   // L
        directions,            // (center)
        exclude(CVec{-1, 0}),  // R
        exclude(CVec{1, -1}),  // BL
        exclude(CVec{0, -1}),  // B
        exclude(CVec{-1, -1}),  // BR
    };
  }();
  return vec;
}

// ———— DensePathGraph ctor(L40-54)————
DensePathGraph::DensePathGraph(Locomotor* locomotor, Actor* actor,
                               sim::World& world, BlockedByActor check,
                               std::function<int(CPos)> custom_cost,
                               Actor* ignore_actor, bool lane_bias,
                               bool in_reverse)
    : custom_movement_layers_{world.CustomMovementLayers()},
      locomotor_{locomotor},
      actor_{actor},
      world_{world},
      check_{check},
      custom_cost_{std::move(custom_cost)},
      ignore_actor_{ignore_actor},
      lane_bias_{lane_bias},
      in_reverse_{in_reverse} {
  for (auto* cml : custom_movement_layers_)
    if (cml != nullptr &&
        cml->EnabledForLocomotor(locomotor_->Info()))
      ++custom_movement_layers_enabled_for_locomotor_;
  check_terrain_height_ = world.Map().Grid().MaximumTerrainHeight() > 0;
}

// ———— GetConnections(L107-172)————
std::vector<GraphConnection>& DensePathGraph::GetConnections(
    CPos position, const std::function<bool(CPos)>& target_predicate) {
  const std::uint8_t layer = position.Layer();
  const CellInfo info = At(position);
  const CPos previous_node = info.previous_node;

  const int dx = position.X() - previous_node.X();
  const int dy = position.Y() - previous_node.Y();
  const std::size_t index = static_cast<std::size_t>(dy * 3 + dx + 4);

  const auto& height_layer = world_.Map().Height();
  const std::vector<CVec>& directions =
      (check_terrain_height_ && layer == 0 && previous_node.Layer() == 0 &&
       height_layer.Get(position) != height_layer.Get(previous_node))
          ? DirectedNeighborsConservative()[index]
          : DirectedNeighbors()[index];

  // OPT-A2:成员暂存缓冲替代每展开一次的 List 分配(见头注)
  // OPT-A2: the member scratch buffer replaces the per-expansion List
  // allocation (see the header).
  vec_scratch_.clear();
  vec_scratch_.reserve(directions.size() +
                       static_cast<std::size_t>(
                           layer == 0 ? custom_movement_layers_enabled_for_locomotor_
                                      : 1));
  for (const CVec& dir : directions) {
    const CPos neighbor = position + dir;
    if (!IsValidNeighbor(neighbor))
      continue;

    const int path_cost =
        GetPathCostToNode(position, neighbor, dir, target_predicate);
    if (path_cost != kPathCostForInvalidPath &&
        At(neighbor).status != CellStatus::Closed)
      vec_scratch_.push_back(GraphConnection{neighbor, path_cost});
  }

  if (layer == 0) {
    if (custom_movement_layers_enabled_for_locomotor_ > 0) {
      for (auto* cml : custom_movement_layers_) {
        if (cml == nullptr ||
            !cml->EnabledForLocomotor(locomotor_->Info()))
          continue;

        const CPos layer_position{position.X(), position.Y(), cml->Index()};
        if (!IsValidNeighbor(layer_position))
          continue;

        const short entry_cost = cml->EntryMovementCost(locomotor_->Info(),
                                                        layer_position);
        if (entry_cost != kMovementCostForUnreachableCell &&
            CanEnterNode(position, layer_position, target_predicate) &&
            At(layer_position).status != CellStatus::Closed)
          vec_scratch_.push_back(GraphConnection{layer_position, entry_cost});
      }
    }
  } else {
    const CPos ground_position{position.X(), position.Y(), 0};
    if (IsValidNeighbor(ground_position)) {
      const short exit_cost =
          custom_movement_layers_[layer]->ExitMovementCost(locomotor_->Info(),
                                                           ground_position);
      if (exit_cost != kMovementCostForUnreachableCell &&
          CanEnterNode(position, ground_position, target_predicate) &&
          At(ground_position).status != CellStatus::Closed)
        vec_scratch_.push_back(
            GraphConnection{ground_position, exit_cost});
    }
  }

  return vec_scratch_;
}

// ———— CanEnterNode(L174-180)————
bool DensePathGraph::CanEnterNode(
    CPos src_node, CPos dest_node,
    const std::function<bool(CPos)>& target_predicate) const {
  return locomotor_->MovementCostToEnterCell(actor_, src_node, dest_node,
                                             check_, ignore_actor_) !=
             kMovementCostForUnreachableCell ||
         (in_reverse_ && target_predicate(dest_node));
}

// ———— GetPathCostToNode(L182-195)————
int DensePathGraph::GetPathCostToNode(
    CPos src_node, CPos dest_node, CVec direction,
    const std::function<bool(CPos)>& target_predicate) const {
  short movement_cost = locomotor_->MovementCostToEnterCell(
      actor_, src_node, dest_node, check_, ignore_actor_);

  // When doing searches in reverse, we must allow movement onto an
  // inaccessible target location.(上游注释)| (upstream comment)
  if (movement_cost == kMovementCostForUnreachableCell && in_reverse_ &&
      target_predicate(dest_node))
    movement_cost = 0;

  if (movement_cost != kMovementCostForUnreachableCell)
    return CalculateCellPathCost(dest_node, direction, movement_cost);

  return kPathCostForInvalidPath;
}

// ———— CalculateCellPathCost(L197-226)————
int DensePathGraph::CalculateCellPathCost(CPos neighbor_c_pos, CVec direction,
                                          short movement_cost) const {
  int cell_cost = direction.X * direction.Y != 0
                      ? MultiplyBySqrtTwo(movement_cost)
                      : movement_cost;

  if (custom_cost_ != nullptr) {
    const int custom_cell_cost = custom_cost_(neighbor_c_pos);
    if (custom_cell_cost == kPathCostForInvalidPath)
      return kPathCostForInvalidPath;

    cell_cost += custom_cell_cost;
  }

  // Directional bonuses for smoother flow!(上游注释)| (upstream comment)
  if (lane_bias_) {
    const int ux = (neighbor_c_pos.X() + (in_reverse_ ? 1 : 0)) & 1;
    const int uy = (neighbor_c_pos.Y() + (in_reverse_ ? 1 : 0)) & 1;

    if ((ux == 0 && direction.Y < 0) || (ux == 1 && direction.Y > 0))
      cell_cost += kLaneBiasCost;

    if ((uy == 0 && direction.X < 0) || (uy == 1 && direction.X > 0))
      cell_cost += kLaneBiasCost;
  }

  return cell_cost;
}

// ———— MapPathGraph(MapPathGraph.cs L26-54)————
MapPathGraph::MapPathGraph(CellInfoLayerPool& layer_pool,
                           Locomotor* locomotor, Actor* actor,
                           sim::World& world, BlockedByActor check,
                           std::function<int(CPos)> custom_cost,
                           Actor* ignore_actor, bool lane_bias,
                           bool in_reverse)
    : DensePathGraph{locomotor, actor, world,
                     check,
                     std::move(custom_cost), ignore_actor, lane_bias,
                     in_reverse},
      pooled_layer_{layer_pool.Get()} {
  // As we support a search over the whole map area, use the pool to grab
  // the CellInfos we need to track the graph state.(上游注释)
  vec_cell_info_for_layer_.resize(custom_movement_layers_.size(), nullptr);
  vec_cell_info_for_layer_[0] = &pooled_layer_.GetLayer();
  for (auto* cml : custom_movement_layers_)
    if (cml != nullptr && cml->EnabledForLocomotor(locomotor_->Info()))
      vec_cell_info_for_layer_[cml->Index()] = &pooled_layer_.GetLayer();
}

CellInfo MapPathGraph::At(CPos pos) const {
  return vec_cell_info_for_layer_[pos.Layer()]->Get(pos);
}

void MapPathGraph::Set(CPos pos, CellInfo info) {
  vec_cell_info_for_layer_[pos.Layer()]->Set(pos, std::move(info));
}

void MapPathGraph::Dispose() {
  // L48-54:pooledLayer.Dispose()(RAII 成员再保险)
  // L48-54: pooledLayer.Dispose() (the RAII member double-guards).
  pooled_layer_.Dispose();
}

// ———— GridPathGraph(GridPathGraph.cs L27-53)————
GridPathGraph::GridPathGraph(Locomotor* locomotor, Actor* actor,
                             sim::World& world, BlockedByActor check,
                             std::function<int(CPos)> custom_cost,
                             Actor* ignore_actor, bool lane_bias,
                             bool in_reverse, const Grid& grid)
    : DensePathGraph{locomotor, actor, world,
                     check,     std::move(custom_cost), ignore_actor,
                     lane_bias, in_reverse},
      vec_infos_(static_cast<std::size_t>(grid.Width()) * grid.Height()),
      grid_{grid} {}

bool GridPathGraph::IsValidNeighbor(CPos neighbor) const {
  // Enforce that we only search within the grid bounds.(上游注释)
  return grid_.Contains(neighbor);
}

int GridPathGraph::InfoIndex(CPos pos) const {
  return (pos.Y() - grid_.top_left.Y()) * grid_.Width() +
         (pos.X() - grid_.top_left.X());
}

CellInfo GridPathGraph::At(CPos pos) const {
  return vec_infos_[static_cast<std::size_t>(InfoIndex(pos))];
}

void GridPathGraph::Set(CPos pos, CellInfo info) {
  vec_infos_[static_cast<std::size_t>(InfoIndex(pos))] = std::move(info);
}

}  // namespace ora::mods::pathfinding
