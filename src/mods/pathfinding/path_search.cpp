// UPSTREAM: OpenRA.Mods.Common/Pathfinder/PathSearch.cs @b6fc03f(实现部分)
//          The implementation half of PathSearch.cs.
import std;

#include "mods/pathfinding/path_search.hpp"

#include "sim/actor.hpp"
#include "sim/world.hpp"
#include "mods/pathfinding/locomotor.hpp"

namespace ora::mods::pathfinding {

using sim::Actor;
using sim::BlockedByActor;
using sim::World;

/// AddInitialCells(L97-110;PathSearch 的友元 —— 不入匿名命名空间,链路
/// 连接性须与友元声明一致)| AddInitialCells (L97-110; a PathSearch
/// friend — kept out of the anonymous namespace so its linkage matches the
/// friend declaration).
void AddInitialCells(World& world, Locomotor* locomotor, Actor* self,
                     std::span<const CPos> froms, BlockedByActor check,
                     const std::function<int(CPos)>& custom_cost,
                     Actor* ignore_actor, bool in_reverse, PathSearch& search) {
  // A source cell is allowed to have an unreachable movement cost.
  // (上游注释段)| (upstream comment)
  for (const CPos& sl : froms)
    if (PathSearch::CellAllowsMovement(world, locomotor, sl, custom_cost) &&
        (!in_reverse ||
         locomotor->MovementCostToEnterCell(self, sl, check, ignore_actor,
                                            true) !=
             kMovementCostForUnreachableCell))
      search.AddInitialCell(sl, custom_cost);
}

// ———— ToTargetCellByPredicate(L38-52)————
std::unique_ptr<PathSearch> PathSearch::ToTargetCellByPredicate(
    World& world, Locomotor* locomotor, Actor* self,
    std::span<const CPos> froms,
    const std::function<bool(CPos)>& target_predicate, BlockedByActor check,
    const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
    bool lane_bias, IRecorder* recorder) {
  auto graph = std::make_unique<MapPathGraph>(
      CellInfoLayerPool::LayerPoolForWorld(&world, &world.Map()), locomotor,
      self, world, check, custom_cost, ignore_actor, lane_bias, false);
  auto search = std::unique_ptr<PathSearch>(new PathSearch{
      std::move(graph), [](CPos, bool) { return 0; }, 0, target_predicate,
      recorder});

  AddInitialCells(world, locomotor, self, froms, check, custom_cost,
                  ignore_actor, false, *search);

  return search;
}

// ———— ToTargetCell(L54-77)————
std::unique_ptr<PathSearch> PathSearch::ToTargetCell(
    World& world, Locomotor* locomotor, Actor* self,
    std::span<const CPos> froms, CPos target, BlockedByActor check,
    int heuristic_weight_percentage,
    const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
    bool lane_bias, bool in_reverse,
    const std::function<int(CPos, bool)>& heuristic, const Grid* grid,
    IRecorder* recorder) {
  std::unique_ptr<IPathGraph> graph;
  if (grid != nullptr)
    graph = std::make_unique<GridPathGraph>(locomotor, self, world, check,
                                            custom_cost, ignore_actor,
                                            lane_bias, in_reverse, *grid);
  else
    graph = std::make_unique<MapPathGraph>(
        CellInfoLayerPool::LayerPoolForWorld(&world, &world.Map()), locomotor,
        self, world, check, custom_cost, ignore_actor, lane_bias, in_reverse);

  // heuristic ??= DefaultCostEstimator(locomotor, target)(L71)
  std::function<int(CPos, bool)> fn_heuristic = heuristic;
  if (fn_heuristic == nullptr)
    fn_heuristic = DefaultCostEstimatorForTarget(locomotor, target);

  auto search = std::unique_ptr<PathSearch>(
      new PathSearch{std::move(graph), std::move(fn_heuristic),
                     heuristic_weight_percentage,
                     [target](CPos loc) { return loc == target; }, recorder});

  AddInitialCells(world, locomotor, self, froms, check, custom_cost,
                  ignore_actor, in_reverse, *search);

  return search;
}

// ———— CellAllowsMovement(L90-95)————
bool PathSearch::CellAllowsMovement(World& world, Locomotor* locomotor,
                                    CPos cell,
                                    const std::function<int(CPos)>& custom_cost) {
  const std::span<sim::ICustomMovementLayer* const> cmls =
      world.CustomMovementLayers();
  return world.Map().Contains(cell) &&
         (cell.Layer() == 0 ||
          (cell.Layer() < cmls.size() && cmls[cell.Layer()] != nullptr &&
           cmls[cell.Layer()]->EnabledForLocomotor(locomotor->Info()))) &&
         (custom_cost == nullptr ||
          custom_cost(cell) != kPathCostForInvalidPath);
}

// ———— ToTargetCellOverGraph(L112-122)————
std::unique_ptr<PathSearch> PathSearch::ToTargetCellOverGraph(
    SparsePathGraph::EdgesFn edges, Locomotor* locomotor, CPos from,
    CPos target, int estimated_search_size, IRecorder* recorder) {
  auto graph =
      std::make_unique<SparsePathGraph>(std::move(edges), estimated_search_size);
  auto search = std::unique_ptr<PathSearch>(
      new PathSearch{std::move(graph),
                     [estimator = DefaultCostEstimator(locomotor),
                      target](CPos here, bool) {
                       return estimator(here, target);
                     },
                     100, [target](CPos loc) { return loc == target; },
                     recorder});

  search->AddInitialCell(from, nullptr);

  return search;
}

// ———— DefaultCostEstimator(L132-161)————
std::function<int(CPos, bool)> PathSearch::DefaultCostEstimatorForTarget(
    Locomotor* locomotor, CPos destination) {
  auto estimator = DefaultCostEstimator(locomotor);
  return [estimator, destination](CPos here, bool) {
    return estimator(here, destination);
  };
}

std::function<int(CPos, CPos)> PathSearch::DefaultCostEstimator(
    Locomotor* locomotor) {
  // Determine the minimum possible cost for moving horizontally between
  // cells based on terrain speeds.(上游注释段)
  // TerrainSpeeds.Values.Min(ti => ti.Cost)(L149):空集 = Min() 等价抛
  const auto& speeds = locomotor->Info().TerrainSpeeds();
  if (speeds.empty())
    throw std::runtime_error("Sequence contains no elements");
  int cell_cost = std::numeric_limits<int>::max();
  for (const auto& [key, ti] : speeds)
    cell_cost = std::min(cell_cost, static_cast<int>(ti.Cost()));
  const int diagonal_cell_cost = MultiplyBySqrtTwo(cell_cost);
  return [cell_cost, diagonal_cell_cost](CPos here, CPos destination) {
    const int diag = std::min(std::abs(here.X() - destination.X()),
                              std::abs(here.Y() - destination.Y()));
    const int straight = std::abs(here.X() - destination.X()) +
                         std::abs(here.Y() - destination.Y());

    // According to the information link, this is the shape of the
    // function.(上游注释)| (upstream comment)
    return cell_cost * straight + (diagonal_cell_cost - 2 * cell_cost) * diag;
  };
}

// ———— ctor(L188-196)————
PathSearch::PathSearch(std::unique_ptr<IPathGraph> graph,
                       std::function<int(CPos, bool)> heuristic,
                       int heuristic_weight_percentage,
                       std::function<bool(CPos)> target_predicate,
                       IRecorder* recorder)
    : graph_{std::move(graph)},
      fn_target_predicate_{std::move(target_predicate)},
      fn_heuristic_{std::move(heuristic)},
      heuristic_weight_percentage_{heuristic_weight_percentage},
      recorder_{recorder} {}

// ———— AddInitialCell(L198-216)————
void PathSearch::AddInitialCell(CPos location,
                                const std::function<int(CPos)>& custom_cost) {
  int initial_cost = 0;
  if (custom_cost != nullptr) {
    initial_cost = custom_cost(location);
    if (initial_cost == kPathCostForInvalidPath)
      return;
  }

  const int heuristic_cost = fn_heuristic_(location, false);
  if (heuristic_cost == kPathCostForInvalidPath)
    return;

  const int estimated_cost = heuristic_cost * heuristic_weight_percentage_ / 100;
  graph_->Set(location,
              CellInfo{CellStatus::Open, initial_cost,
                       initial_cost + estimated_cost, location});
  const GraphConnection connection{location, estimated_cost};
  open_queue_.Add(connection);
}

// ———— CanExpand(L222-240)————
bool PathSearch::CanExpand() {
  // Connections to a cell can appear more than once if a search discovers
  // a lower cost route to the cell.(上游注释段)
  CellStatus status;
  do {
    if (open_queue_.Empty())
      return false;

    status = graph_->At(open_queue_.Peek().destination).status;
    if (status == CellStatus::Closed)
      open_queue_.Pop();
  } while (status == CellStatus::Closed);

  return true;
}

// ———— Expand(L247-292)————
CPos PathSearch::Expand() {
  const CPos current_min_node = open_queue_.Pop().destination;

  const CellInfo current_info = graph_->At(current_min_node);
  graph_->Set(current_min_node,
              CellInfo{CellStatus::Closed, current_info.cost_so_far,
                       current_info.estimated_total_cost,
                       current_info.previous_node});

  auto& connections =
      graph_->GetConnections(current_min_node, fn_target_predicate_);
  for (const GraphConnection& connection : connections) {
    // Calculate the cost up to that point(L256)| (L256).
    const int cost_so_far_to_neighbor =
        current_info.cost_so_far + connection.cost;

    const CPos neighbor = connection.destination;
    const CellInfo neighbor_info = graph_->At(neighbor);

    // Cost is even higher; next direction:(上游注释)
    if (neighbor_info.status == CellStatus::Closed ||
        (neighbor_info.status == CellStatus::Open &&
         cost_so_far_to_neighbor >= neighbor_info.cost_so_far))
      continue;

    // Now we may seriously consider this direction using heuristics.
    // (上游注释)
    int estimated_remaining_cost_to_target;
    if (neighbor_info.status == CellStatus::Open) {
      // If the cell has already been processed, we can reuse the result
      // (upstream comment)| (upstream comment).
      estimated_remaining_cost_to_target =
          neighbor_info.estimated_total_cost - neighbor_info.cost_so_far;
    } else {
      // If the heuristic reports the cell is unreachable, we won't
      // consider it.(上游注释)| (upstream comment)
      const int heuristic_cost = fn_heuristic_(neighbor, true);
      if (heuristic_cost == kPathCostForInvalidPath)
        continue;
      estimated_remaining_cost_to_target =
          heuristic_cost * heuristic_weight_percentage_ / 100;
    }

    if (recorder_ != nullptr)
      recorder_->Add(current_min_node, neighbor, cost_so_far_to_neighbor,
                     estimated_remaining_cost_to_target);

    const int estimated_total_cost_to_target =
        cost_so_far_to_neighbor + estimated_remaining_cost_to_target;
    graph_->Set(neighbor,
                CellInfo{CellStatus::Open, cost_so_far_to_neighbor,
                         estimated_total_cost_to_target, current_min_node});
    open_queue_.Add(GraphConnection{neighbor, estimated_total_cost_to_target});
  }

  return current_min_node;
}

// ———— ExpandToTarget(L302-309)————
bool PathSearch::ExpandToTarget() {
  while (CanExpand())
    if (fn_target_predicate_(Expand()))
      return true;
  return false;
}

// ———— ExpandAll(L315-321)————
std::vector<CPos> PathSearch::ExpandAll() {
  std::vector<CPos> considered_cells;
  while (CanExpand())
    considered_cells.push_back(Expand());
  return considered_cells;
}

// ———— FindPath(L327-337)————
std::vector<CPos> PathSearch::FindPath() {
  while (CanExpand()) {
    const CPos p = Expand();
    if (fn_target_predicate_(p))
      return MakePath(*graph_, p);
  }
  return NoPathVector();
}

// ———— MakePath(L341-354)————
std::vector<CPos> PathSearch::MakePath(IPathGraph& graph, CPos destination) {
  // Build the path from the destination.(上游注释)| (upstream comment)
  std::vector<CPos> ret;
  CPos current_node = destination;

  while (!(graph.At(current_node).previous_node == current_node)) {
    ret.push_back(current_node);
    current_node = graph.At(current_node).previous_node;
  }

  ret.push_back(current_node);
  return ret;
}

// ———— FindBidiPath(L360-380)————
std::vector<CPos> PathSearch::FindBidiPath(PathSearch& first,
                                           PathSearch& second) {
  // Expands both path searches until they intersect, and returns the
  // path.(上游注释)| (upstream comment)
  while (first.CanExpand() && second.CanExpand()) {
    // make some progress on the first search(上游注释)
    const CPos p = first.Expand();
    const CellInfo p_info = second.graph_->At(p);
    if (p_info.status == CellStatus::Closed &&
        p_info.cost_so_far != kPathCostForInvalidPath)
      return MakeBidiPath(first, second, p);

    // make some progress on the second search(上游注释)
    const CPos q = second.Expand();
    const CellInfo q_info = first.graph_->At(q);
    if (q_info.status == CellStatus::Closed &&
        q_info.cost_so_far != kPathCostForInvalidPath)
      return MakeBidiPath(first, second, q);
  }

  return NoPathVector();
}

// ———— MakeBidiPath(L384-414)————
std::vector<CPos> PathSearch::MakeBidiPath(PathSearch& first,
                                           PathSearch& second,
                                           CPos confluence_node) {
  // Build the path from the destination of each search.(上游注释)
  IPathGraph& ca = *first.graph_;
  IPathGraph& cb = *second.graph_;

  std::vector<CPos> ret;

  CPos q = confluence_node;
  CPos previous = ca.At(q).previous_node;
  while (!(previous == q)) {
    ret.push_back(q);
    q = previous;
    previous = ca.At(q).previous_node;
  }

  ret.push_back(q);
  std::ranges::reverse(ret);

  q = confluence_node;
  previous = cb.At(q).previous_node;
  while (!(previous == q)) {
    q = previous;
    previous = cb.At(q).previous_node;
    ret.push_back(q);
  }

  return ret;
}

}  // namespace ora::mods::pathfinding
