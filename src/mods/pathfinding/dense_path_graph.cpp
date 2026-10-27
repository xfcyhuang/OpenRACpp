import std;

#include "mods/pathfinding/dense_path_graph.hpp"

#include "core/exts_math.hpp"
#include "sim/actor.hpp"
#include "mods/pathfinding/locomotor.hpp"

namespace ora::mods::pathfinding {

using sim::BlockedByActor;

const std::vector<std::vector<CVec>>& DensePathGraph::DirectedNeighbors() {

  static const std::vector<std::vector<CVec>> vec = {
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}, CVec{-1, 0}, CVec{-1, 1}},
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}},
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}, CVec{1, 0}, CVec{1, 1}},
      {CVec{-1, -1}, CVec{-1, 0}, CVec{-1, 1}},
      {CVec{-1, -1}, CVec{0, -1}, CVec{1, -1}, CVec{-1, 0},
       CVec{1, 0}, CVec{-1, 1}, CVec{0, 1}, CVec{1, 1}},
      {CVec{1, -1}, CVec{1, 0}, CVec{1, 1}},
      {CVec{-1, -1}, CVec{-1, 0}, CVec{-1, 1}, CVec{0, 1},
       CVec{1, 1}},
      {CVec{-1, 1}, CVec{0, 1}, CVec{1, 1}},
      {CVec{1, -1}, CVec{1, 0}, CVec{-1, 1}, CVec{0, 1}, CVec{1, 1}},
  };
  return vec;
}

const std::vector<std::vector<CVec>>&
DensePathGraph::DirectedNeighborsConservative() {

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
        exclude(CVec{1, 1}),
        exclude(CVec{0, 1}),
        exclude(CVec{-1, 1}),
        exclude(CVec{1, 0}),
        directions,
        exclude(CVec{-1, 0}),
        exclude(CVec{1, -1}),
        exclude(CVec{0, -1}),
        exclude(CVec{-1, -1}),
    };
  }();
  return vec;
}

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

bool DensePathGraph::CanEnterNode(
    CPos src_node, CPos dest_node,
    const std::function<bool(CPos)>& target_predicate) const {
  return locomotor_->MovementCostToEnterCell(actor_, src_node, dest_node,
                                             check_, ignore_actor_) !=
             kMovementCostForUnreachableCell ||
         (in_reverse_ && target_predicate(dest_node));
}

int DensePathGraph::GetPathCostToNode(
    CPos src_node, CPos dest_node, CVec direction,
    const std::function<bool(CPos)>& target_predicate) const {
  short movement_cost = locomotor_->MovementCostToEnterCell(
      actor_, src_node, dest_node, check_, ignore_actor_);

  if (movement_cost == kMovementCostForUnreachableCell && in_reverse_ &&
      target_predicate(dest_node))
    movement_cost = 0;

  if (movement_cost != kMovementCostForUnreachableCell)
    return CalculateCellPathCost(dest_node, direction, movement_cost);

  return kPathCostForInvalidPath;
}

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

  pooled_layer_.Dispose();
}

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

}
