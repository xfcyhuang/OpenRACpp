// UPSTREAM: OpenRA.Mods.Common/Pathfinder/HierarchicalPathFinder.cs
//          @b6fc03f(实现部分)
//          The implementation half of HierarchicalPathFinder.cs.
import std;

#include "mods/pathfinding/hierarchical_path_finder.hpp"

#include "map/map.hpp"
#include "mods/pathfinding/locomotor.hpp"
#include "mods/pathfinding/path_search.hpp"
#include "sim/actor.hpp"
#include "sim/world.hpp"

namespace ora::mods::pathfinding {

using sim::BlockedByActor;
using sim::ICrushable;

// ———— GridInfo(L136-178)————
std::optional<CPos> GridInfo::AbstractCellForLocalCell(
    CPos local_cell, HierarchicalPathFinder* hpf) const {
  const std::optional<CPos>& abstract_cell =
      single_abstract_cell_for_layer_[local_cell.Layer()];
  if (abstract_cell.has_value()) {
    // All reachable cells in the grid are joined together so only a single
    // abstract cell was needed(上游注释段)| (upstream comment)
    if (hpf != nullptr && !hpf->CellIsAccessible(local_cell))
      return std::nullopt;
    return abstract_cell;
  }

  // Only reachable cells would be populated in the lookup(上游注释)
  auto it = map_local_cell_to_abstract_cell_.find(local_cell);
  if (it != map_local_cell_to_abstract_cell_.end())
    return it->second;
  return std::nullopt;
}

void GridInfo::CopyAbstractCellsInto(std::vector<CPos>& set) const {
  for (const std::optional<CPos>& single : single_abstract_cell_for_layer_)
    if (single.has_value())
      set.push_back(*single);
  for (const auto& [cell, abstract_cell] : map_local_cell_to_abstract_cell_)
    set.push_back(abstract_cell);
}

namespace {

/// 去重的插入序 CPos 集(HashSet 的插入序承载;ExceptWith = 标记剔除)
/// The deduplicated insertion-ordered CPos set (the HashSet insertion-order
/// carrier; ExceptWith = mark-removal).
class CPosWorkSet {
 public:
  bool Add(CPos cell) {
    if (set_.contains(cell))
      return false;
    set_.insert(cell);
    vec_.push_back(cell);
    return true;
  }
  bool Contains(CPos cell) const { return set_.contains(cell); }
  void ExceptWith(const std::vector<CPos>& other) {
    for (const CPos& c : other)
      if (set_.erase(c) > 0)
        std::erase(vec_, c);
  }
  bool Empty() const { return vec_.empty(); }
  std::size_t Count() const { return vec_.size(); }
  CPos First() const { return vec_.front(); }
  const std::vector<CPos>& Vec() const { return vec_; }
  void Clear() {
    set_.clear();
    vec_.clear();
  }

 private:
  std::unordered_set<CPos> set_;
  std::vector<CPos> vec_;
};

}  // namespace

// ———— AbstractGraphWithInsertedEdges(L185-244)————
HierarchicalPathFinder::AbstractGraphWithInsertedEdges::
    AbstractGraphWithInsertedEdges(
        const std::unordered_map<CPos, std::vector<GraphConnection>>&
            abstract_edges,
        std::span<const GraphEdge> source_edges, const GraphEdge* target_edge,
        const std::function<int(CPos, CPos)>& cost_estimator)
    : map_abstract_edges_{abstract_edges} {
  map_changed_edges_.reserve(source_edges.size() * 9 +
                             (target_edge != nullptr ? 9 : 0));
  for (const GraphEdge& source_edge : source_edges)
    InsertEdgeAsBidirectional(source_edge, cost_estimator);
  if (target_edge != nullptr)
    InsertEdgeAsBidirectional(*target_edge, cost_estimator);
}

void HierarchicalPathFinder::AbstractGraphWithInsertedEdges::
    InsertEdgeAsBidirectional(const GraphEdge& edge,
                              const std::function<int(CPos, CPos)>& cost_estimator) {
  InsertConnections(edge.source, edge.destination, cost_estimator);
}

void HierarchicalPathFinder::AbstractGraphWithInsertedEdges::
    InsertConnections(CPos local_cell, CPos abstract_cell,
                      const std::function<int(CPos, CPos)>& cost_estimator) {
  // 上游 LINQ 链(Select+Append)的插入期物化形态(头注;条目序逐条等价)
  // The eager-materialization form of upstream's LINQ chains
  // (Select+Append) (the header note; item order kept).
  std::vector<GraphConnection> edges;
  if (auto it = map_abstract_edges_.find(abstract_cell);
      it != map_abstract_edges_.end())
    edges = it->second;

  {
    std::vector<GraphConnection> local_connections;
    local_connections.reserve(edges.size() + 1);
    for (const GraphConnection& e : edges)
      local_connections.push_back(
          GraphConnection{e.destination, cost_estimator(local_cell, e.destination)});
    local_connections.push_back(
        GraphConnection{abstract_cell, cost_estimator(local_cell, abstract_cell)});
    map_changed_edges_[local_cell] = std::move(local_connections);
  }

  {
    std::vector<GraphConnection> abstract_changed;
    if (auto it = map_changed_edges_.find(abstract_cell);
        it != map_changed_edges_.end())
      abstract_changed = it->second;
    abstract_changed.push_back(
        GraphConnection{local_cell, cost_estimator(abstract_cell, local_cell)});
    map_changed_edges_[abstract_cell] = std::move(abstract_changed);
  }

  for (const GraphConnection& conn : edges) {
    std::vector<GraphConnection> conn_changed;
    if (auto it = map_changed_edges_.find(conn.destination);
        it != map_changed_edges_.end())
      conn_changed = it->second;
    else if (auto base = map_abstract_edges_.find(conn.destination);
             base != map_abstract_edges_.end())
      conn_changed = base->second;

    conn_changed.push_back(GraphConnection{
        local_cell, cost_estimator(conn.destination, local_cell)});
    map_changed_edges_[conn.destination] = std::move(conn_changed);
  }
}

std::vector<GraphConnection>
HierarchicalPathFinder::AbstractGraphWithInsertedEdges::GetConnections(
    CPos position) const {
  if (auto it = map_changed_edges_.find(position);
      it != map_changed_edges_.end())
    return it->second;
  if (auto it = map_abstract_edges_.find(position);
      it != map_abstract_edges_.end())
    return it->second;
  return {};
}

// ———— ctor(L246-288)————
HierarchicalPathFinder::HierarchicalPathFinder(sim::World& world,
                                               Locomotor* locomotor,
                                               IActorMap* actor_map,
                                               BlockedByActor check)
    : world_{world}, locomotor_{locomotor}, actor_map_{actor_map} {
  if (locomotor->Info().TerrainSpeeds().empty())
    return;

  if (check == BlockedByActor::Immovable) {
    // When we account for immovable actors, it depends on the actors on
    // the map.(上游注释段)| (upstream comment)
    actor_map_->AddCellUpdatedListener(
        [this](CPos cell) { RequireBlockingRefreshInCell(cell); });

    // Determine immovable cells from actors already on the map.(上游注释)
    for (Actor* a : actor_map_->AllActors()) {
      if (!ActorIsBlocking(a))
        continue;
      for (const auto& [cell, sub_cell] :
           a->OccupiesSpace()->OccupiedCells()) {
        if (ActorCellIsBlocking(a, cell))
          set_cells_with_blocking_actor_.insert(cell);
      }
    }
  } else if (check != BlockedByActor::None) {
    // InvalidEnumArgumentException 文本等价(枚举名面)
    throw std::runtime_error(
        std::format("{} supports {} and {} only for {}",
                    "HierarchicalPathFinder", "None", "Immovable", "check"));
  }

  fn_cost_estimator_ = PathSearch::DefaultCostEstimator(locomotor);

  BuildGrids();
  BuildCostTable();
  RebuildDomains();

  // When we build the cost table, it depends on the movement costs of the
  // cells at that time.(上游注释段)| (upstream comment)
  locomotor_->AddCellCostChangedListener(
      [this](CPos cell, short old_cost, short new_cost) {
        RequireCostRefreshInCell(cell, old_cost, new_cost);
      });

  // If the map projection changes, the result of Map.Contains(CPos) may
  // change.(上游注释段)| (upstream comment)
  world.Map().AddCellProjectionChangedListener(
      [this](CPos cell) { RequireProjectionRefreshInCell(cell); });
}

// ———— BuildGrids(L308-334)————
void HierarchicalPathFinder::BuildGrids() {
  const auto get_cpos_bounds = [](const map::Map& m) {
    if (m.Grid().Type == MapGridType::RectangularIsometric) {
      const CPos bottom_right = m.AllCells().BottomRight();
      const int bottom_right_u = bottom_right.ToMPos(m.Grid().Type).U;
      return Grid{CPos{0, -bottom_right_u},
                  CPos{bottom_right.X() + 1,
                       bottom_right.Y() + bottom_right_u + 1},
                  false};
    }

    return Grid{CPos::Zero(),
                CPos{m.MapSize().Width, m.MapSize().Height}, false};
  };

  map_bounds_ = get_cpos_bounds(world_.Map());
  grid_xs_ = sim::IntegerDivisionRoundingAwayFromZero(map_bounds_.Width(), kGridSize);
  grid_ys_ = sim::IntegerDivisionRoundingAwayFromZero(map_bounds_.Height(), kGridSize);

  const auto custom_movement_layers = world_.CustomMovementLayers();
  vec_grid_infos_.assign(static_cast<std::size_t>(grid_xs_) * grid_ys_, GridInfo{});
  for (int grid_x = map_bounds_.top_left.X();
       grid_x < map_bounds_.bottom_right.X(); grid_x += kGridSize)
    for (int grid_y = map_bounds_.top_left.Y();
         grid_y < map_bounds_.bottom_right.Y(); grid_y += kGridSize)
      vec_grid_infos_[static_cast<std::size_t>(GridIndex(CPos{grid_x, grid_y}))] =
          BuildGrid(grid_x, grid_y, custom_movement_layers);
}

// ———— BuildGrid(L343-448)————
GridInfo HierarchicalPathFinder::BuildGrid(
    int grid_x, int grid_y,
    std::span<sim::ICustomMovementLayer* const> custom_movement_layers) {
  GridInfo info;
  info.single_abstract_cell_for_layer_.resize(custom_movement_layers.size());

  // When accounting for immovable actors, use a custom cost so those cells
  // become invalid paths.(上游注释)| (upstream comment)
  std::function<int(CPos)> custom_cost;
  if (!set_cells_with_blocking_actor_.empty())
    custom_cost = [this](CPos c) {
      return set_cells_with_blocking_actor_.contains(c)
                 ? kPathCostForInvalidPath
                 : 0;
    };

  CPosWorkSet accessible_cells;
  for (std::uint8_t grid_layer = 0; grid_layer < custom_movement_layers.size();
       grid_layer++) {
    if (grid_layer != 0 &&
        (custom_movement_layers[grid_layer] == nullptr ||
         !custom_movement_layers[grid_layer]->EnabledForLocomotor(
             locomotor_->Info())))
      continue;

    const Grid grid = GetGrid(CPos{grid_x, grid_y, grid_layer}, map_bounds_);
    for (int y = grid_y; y < grid.bottom_right.Y(); y++) {
      for (int x = grid_x; x < grid.bottom_right.X(); x++) {
        const CPos cell{x, y, grid_layer};
        if (CellIsAccessible(cell))
          accessible_cells.Add(cell);
      }
    }

    // AbstractCellForLocalCells(L372-410) | AbstractCellForLocalCells
    // (L372-410).
    const auto abstract_cell_for_local_cells =
        [](std::span<const CPos> cells, std::uint8_t layer) {
          int min_x = std::numeric_limits<int>::max();
          int min_y = std::numeric_limits<int>::max();
          int max_x = std::numeric_limits<int>::min();
          int max_y = std::numeric_limits<int>::min();
          for (const CPos& cell : cells) {
            min_x = std::min(cell.X(), min_x);
            min_y = std::min(cell.Y(), min_y);
            max_x = std::max(cell.X(), max_x);
            max_y = std::max(cell.Y(), max_y);
          }

          const int region_width = max_x - min_x;
          const int region_height = max_y - min_y;
          const CPos desired{min_x + region_width / 2,
                             min_y + region_height / 2, layer};

          // Make sure the abstract cell is one of the available local
          // cells.(上游注释段)| (upstream comment)
          CPos abstract_cell = desired;
          int distance = std::numeric_limits<int>::max();
          for (const CPos& cell : cells) {
            const int new_distance = (cell - desired).LengthSquared();
            if (distance > new_distance ||
                (distance == new_distance && abstract_cell.X() > cell.X()) ||
                (distance == new_distance && abstract_cell.X() == cell.X() &&
                 abstract_cell.Y() > cell.Y())) {
              distance = new_distance;
              abstract_cell = cell;
            }
          }

          return abstract_cell;
        };

    // Flood fill the search area from one of the accessible cells.
    // (上游注释段)| (upstream comment)
    bool has_populated_abstract_cell_for_layer = false;
    while (accessible_cells.Count() > 0) {
      const CPos src = accessible_cells.First();
      std::unique_ptr<PathSearch> search = GetLocalPathSearch(
          nullptr, std::span{&src, 1}, src, custom_cost, nullptr,
          BlockedByActor::None, false, &grid, 100, nullptr, false, nullptr);

      // Determinism: The order of visited cells from ExpandAll can be
      // perturbed by cell cost changes.(上游注释)| (upstream comment)
      const std::vector<CPos> local_cells_in_region = search->ExpandAll();
      const CPos abstract_cell =
          abstract_cell_for_local_cells(local_cells_in_region, grid_layer);
      accessible_cells.ExceptWith(local_cells_in_region);

      // PERF: If there is only one distinct region of cells(上游注释段)
      if (!has_populated_abstract_cell_for_layer && accessible_cells.Count() == 0)
        info.single_abstract_cell_for_layer_[grid_layer] = abstract_cell;
      else {
        // When there is more than one region within the grid(上游注释段)
        has_populated_abstract_cell_for_layer = true;
        for (const CPos& local_cell : local_cells_in_region)
          info.map_local_cell_to_abstract_cell_.emplace(local_cell,
                                                        abstract_cell);
      }
    }
  }

  return info;
}

// ———— BuildCostTable(L454-462)————
void HierarchicalPathFinder::BuildCostTable() {
  map_abstract_graph_.clear();
  map_abstract_graph_.reserve(static_cast<std::size_t>(grid_xs_) * grid_ys_);
  const auto custom_movement_layers = world_.CustomMovementLayers();
  for (int grid_x = map_bounds_.top_left.X();
       grid_x < map_bounds_.bottom_right.X(); grid_x += kGridSize)
    for (int grid_y = map_bounds_.top_left.Y();
         grid_y < map_bounds_.bottom_right.Y(); grid_y += kGridSize)
      for (auto& [key, edges] :
           GetAbstractEdgesForGrid(grid_x, grid_y, custom_movement_layers)) {
        // abstractGraph.Add:同键重复 = ArgumentException 等价
        // abstractGraph.Add: a duplicate key is the ArgumentException
        // equivalent.
        if (!map_abstract_graph_.emplace(key, std::move(edges)).second)
          throw std::invalid_argument(
              "An item with the same key has already been added");
      }
}

// ———— GetAbstractEdgesForGrid(L469-577)————
std::vector<std::pair<CPos, std::vector<GraphConnection>>>
HierarchicalPathFinder::GetAbstractEdgesForGrid(
    int grid_x, int grid_y,
    std::span<sim::ICustomMovementLayer* const> custom_movement_layers) {
  // abstractEdges 集合:(Src, Dst) 对;插入序 + 判重(HashSet 承载)
  std::vector<std::pair<CPos, CPos>> vec_abstract_edges;
  auto add_abstract_edge = [&](CPos src, CPos dst) {
    for (const auto& [s, d] : vec_abstract_edges)
      if (s == src && d == dst)
        return;
    vec_abstract_edges.emplace_back(src, dst);
  };

  const auto movement_allowed_between_cells = [&](CPos cell,
                                                  CPos candidate_cell) {
    return MovementAllowedBetweenCells(cell, candidate_cell);
  };

  for (std::uint8_t grid_layer = 0; grid_layer < custom_movement_layers.size();
       grid_layer++) {
    if (grid_layer != 0 &&
        (custom_movement_layers[grid_layer] == nullptr ||
         !custom_movement_layers[grid_layer]->EnabledForLocomotor(
             locomotor_->Info())))
      continue;

    /// AddEdgesIfMovementAllowedBetweenCells(L479-493)
    const auto add_edges_if_movement_allowed =
        [&](CPos cell, CPos candidate_cell) {
          if (!movement_allowed_between_cells(cell, candidate_cell))
            return;

          const std::optional<CPos> abstract_cell =
              AbstractCellForLocalCellNoAccessibleCheck(cell);
          if (!abstract_cell.has_value())
            return;

          const std::optional<CPos> abstract_cell_adjacent =
              AbstractCellForLocalCellNoAccessibleCheck(candidate_cell);
          if (!abstract_cell_adjacent.has_value())
            return;

          add_abstract_edge(*abstract_cell, *abstract_cell_adjacent);
        };

    /// AddAbstractEdges(L498-518)
    const auto add_abstract_edges = [&](int x_increment, int y_increment,
                                        CVec adjacent_vec, int2 offset) {
      const int start_y = grid_y + offset.Y;
      const int start_x = grid_x + offset.X;
      for (int y = start_y; y < start_y + kGridSize; y += y_increment) {
        for (int x = start_x; x < start_x + kGridSize; x += x_increment) {
          const CPos cell{x, y, grid_layer};
          if (!CellIsAccessible(cell))
            continue;

          const CPos adjacent_cell = cell + adjacent_vec;
          const CVec step{adjacent_vec.Y, adjacent_vec.X};
          for (int i = -1; i <= 1; i++) {
            const CPos candidate_cell = adjacent_cell + step * i;
            add_edges_if_movement_allowed(cell, candidate_cell);
          }
        }
      }
    };

    /// AddAbstractCustomLayerEdges(L523-561)
    const auto add_abstract_custom_layer_edges = [&] {
      sim::ICustomMovementLayer* grid_cml = custom_movement_layers[grid_layer];
      for (std::uint8_t candidate_layer = 0;
           candidate_layer < custom_movement_layers.size(); candidate_layer++) {
        if (grid_layer == candidate_layer)
          continue;

        sim::ICustomMovementLayer* candidate_cml =
            custom_movement_layers[candidate_layer];
        if (candidate_layer != 0 &&
            (candidate_cml == nullptr ||
             !candidate_cml->EnabledForLocomotor(locomotor_->Info())))
          continue;

        for (int y = grid_y; y < grid_y + kGridSize; y++) {
          for (int x = grid_x; x < grid_x + kGridSize; x++) {
            const CPos cell{x, y, grid_layer};
            if (!CellIsAccessible(cell))
              continue;

            CPos candidate_cell;
            if (grid_layer == 0) {
              candidate_cell = CPos{cell.X(), cell.Y(), candidate_layer};
              if (candidate_cml->EntryMovementCost(locomotor_->Info(),
                                                   candidate_cell) ==
                  kMovementCostForUnreachableCell)
                continue;
            } else {
              candidate_cell = CPos{cell.X(), cell.Y(), 0};
              if (grid_cml->ExitMovementCost(locomotor_->Info(),
                                             candidate_cell) ==
                  kMovementCostForUnreachableCell)
                continue;
            }

            add_edges_if_movement_allowed(cell, candidate_cell);
          }
        }
      }
    };

    // Top, Left, Bottom, Right(上游注释)| (upstream comment)
    add_abstract_edges(1, kGridSize, CVec{0, -1}, int2{0, 0});
    add_abstract_edges(kGridSize, 1, CVec{-1, 0}, int2{0, 0});
    add_abstract_edges(1, kGridSize, CVec{0, 1}, int2{0, kGridSize - 1});
    add_abstract_edges(kGridSize, 1, CVec{1, 0}, int2{kGridSize - 1, 0});

    add_abstract_custom_layer_edges();
  }

  // GroupBy(edge => edge.Src) 的两阶稳定序:首遇 Src 序 + 段内收集序
  // (OPT-A7 的计数分段等价 | the OPT-A7-style segmentation equivalence).
  std::vector<std::pair<CPos, std::vector<GraphConnection>>> out;
  std::unordered_map<CPos, std::size_t> map_group_index;
  for (const auto& [src, dst] : vec_abstract_edges) {
    auto it = map_group_index.find(src);
    if (it == map_group_index.end()) {
      it = map_group_index.emplace(src, out.size()).first;
      out.emplace_back(src, std::vector<GraphConnection>{});
    }
    out[it->second].second.push_back(
        GraphConnection{dst, fn_cost_estimator_(src, dst)});
  }
  return out;
}

// ———— RequireCostRefreshInCell(L582-589)————
void HierarchicalPathFinder::RequireCostRefreshInCell(CPos cell, short old_cost,
                                                      short new_cost) {
  // We don't care about the specific cost of the cell(上游注释段)
  const bool old_unreachable = old_cost == kMovementCostForUnreachableCell;
  const bool new_unreachable = new_cost == kMovementCostForUnreachableCell;
  if (old_unreachable != new_unreachable)
    MarkGridDirty(GridIndex(cell));
}

// ———— MarkGridDirty(HashSet<int>.Add 的插入序 + 位图判重形态,见头注)
// ———— MarkGridDirty (the insertion-order + bitmap form of HashSet<int>.
// Add — see the header).
void HierarchicalPathFinder::MarkGridDirty(int grid_index) {
  const std::size_t idx = static_cast<std::size_t>(grid_index);
  if (vec_dirty_grid_bitmap_.size() <= idx)
    vec_dirty_grid_bitmap_.resize(idx + 1, 0);
  if (!vec_dirty_grid_bitmap_[idx]) {
    vec_dirty_grid_bitmap_[idx] = 1;
    vec_dirty_grid_indexes_.push_back(grid_index);
  }
}

// ———— CellIsAccessible(L591-595)————
bool HierarchicalPathFinder::CellIsAccessible(CPos cell) const {
  return locomotor_->MovementCostForCell(cell) !=
             kMovementCostForUnreachableCell &&
         (set_cells_with_blocking_actor_.empty() ||
          !set_cells_with_blocking_actor_.contains(cell));
}

// ———— MovementAllowedBetweenCells(L597-602)————
bool HierarchicalPathFinder::MovementAllowedBetweenCells(
    CPos accessible_src_cell, CPos dest_cell) const {
  return locomotor_->MovementCostToEnterCell(nullptr, accessible_src_cell,
                                             dest_cell, BlockedByActor::None,
                                             nullptr) !=
             kMovementCostForUnreachableCell &&
         (set_cells_with_blocking_actor_.empty() ||
          !set_cells_with_blocking_actor_.contains(dest_cell));
}

// ———— RequireBlockingRefreshInCell(L607-630)————
void HierarchicalPathFinder::RequireBlockingRefreshInCell(CPos cell) {
  bool cell_has_blocking_actor = false;
  for (Actor* actor : actor_map_->GetActorsAt(cell)) {
    if (ActorIsBlocking(actor) && ActorCellIsBlocking(actor, cell)) {
      cell_has_blocking_actor = true;
      break;
    }
  }

  if (cell_has_blocking_actor) {
    if (set_cells_with_blocking_actor_.insert(cell).second)
      MarkGridDirty(GridIndex(cell));
  } else {
    if (set_cells_with_blocking_actor_.erase(cell) > 0)
      MarkGridDirty(GridIndex(cell));
  }
}

// ———— RequireProjectionRefreshInCell(L635-638)————
void HierarchicalPathFinder::RequireProjectionRefreshInCell(CPos cell) {
  MarkGridDirty(GridIndex(cell));
}

// ———— ActorIsBlocking(L656-671)————
bool HierarchicalPathFinder::ActorIsBlocking(Actor* actor) const {
  // Mobile 挂点(L658):同 Locomotor 的部分覆盖装配面
  const bool is_movable = false;
  if (is_movable)
    return false;

  const bool is_temporary_blocker =
      world_.RulesContainTemporaryBlocker();  // && TraitOrDefault<ITemporaryBlocker>() != null
  if (is_temporary_blocker)
    return false;

  for (auto* crushable : actor->TraitsImplementing<ICrushable>())
    if (!world_.NoPlayersMask.SetEquals(
            crushable->CrushableByMask(*actor, locomotor_->Info().Crushes())))
      return false;

  return true;
}

// ———— ActorCellIsBlocking(L679-690)————
bool HierarchicalPathFinder::ActorCellIsBlocking(Actor* actor, CPos cell) const {
  (void)actor;
  const bool can_share_cell =
      locomotor_->Info().SharesCell() && actor_map_->HasFreeSubCell(cell);
  if (can_share_cell)
    return false;

  // Building 的 TransitOnlyCells 挂点(L685):同部分覆盖面
  const bool is_transit_only = false;
  if (is_transit_only)
    return false;

  return true;
}

// ———— GridIndex/GetGridTopLeft/GetGrid(L692-725)————
int HierarchicalPathFinder::GridIndex(CPos cell_in_grid) const {
  return (cell_in_grid.Y() - map_bounds_.top_left.Y()) / kGridSize * grid_xs_ +
         (cell_in_grid.X() - map_bounds_.top_left.X()) / kGridSize;
}

CPos HierarchicalPathFinder::GetGridTopLeft(int grid_index,
                                            std::uint8_t layer) const {
  return CPos{grid_index % grid_xs_ * kGridSize + map_bounds_.top_left.X(),
              grid_index / grid_xs_ * kGridSize + map_bounds_.top_left.Y(),
              layer};
}

CPos HierarchicalPathFinder::GetGridTopLeft(CPos cell_in_grid,
                                            const Grid& map_bounds) {
  return CPos{
      (cell_in_grid.X() - map_bounds.top_left.X()) / kGridSize * kGridSize +
          map_bounds.top_left.X(),
      (cell_in_grid.Y() - map_bounds.top_left.Y()) / kGridSize * kGridSize +
          map_bounds.top_left.Y(),
      cell_in_grid.Layer()};
}

Grid HierarchicalPathFinder::GetGrid(CPos cell_in_grid, const Grid& map_bounds) {
  const CPos grid_top_left = GetGridTopLeft(cell_in_grid, map_bounds);
  const int width = std::min(map_bounds.bottom_right.X() - grid_top_left.X(),
                             kGridSize);
  const int height = std::min(map_bounds.bottom_right.Y() - grid_top_left.Y(),
                              kGridSize);

  return Grid{grid_top_left,
              CPos{grid_top_left.X() + width, grid_top_left.Y() + height,
                   grid_top_left.Layer()},
              true};
}

// ———— FindPath 单源(L833-930)————
std::vector<CPos> HierarchicalPathFinder::FindPath(
    Actor* self, CPos source, CPos target, BlockedByActor check,
    int heuristic_weight_percentage, const std::function<int(CPos)>& custom_cost,
    Actor* ignore_actor, bool in_reverse, bool lane_bias) {
  if (fn_cost_estimator_ == nullptr)
    return NoPathVector();

  // If the source and target are close, see if they can be reached
  // locally.(上游注释段)| (upstream comment)
  constexpr int kCloseGridDistance = 2;
  if ((target - source).LengthSquared() <
          kGridSize * kGridSize * kCloseGridDistance * kCloseGridDistance &&
      source.Layer() == target.Layer()) {
    const Grid grid_to_search{
        CPos{std::min(source.X(), target.X()) - kGridSize / 2,
             std::min(source.Y(), target.Y()) - kGridSize / 2, source.Layer()},
        CPos{std::max(source.X(), target.X()) + kGridSize / 2,
             std::max(source.Y(), target.Y()) + kGridSize / 2, source.Layer()},
        false};

    // For paths over a short distance, use a heuristic weight of 100%
    // (上游注释段)| (upstream comment)
    std::vector<CPos> local_path;
    {
      std::unique_ptr<PathSearch> search = GetLocalPathSearch(
          self, std::span{&source, 1}, target, custom_cost, ignore_actor,
          check, lane_bias, &grid_to_search, 100, nullptr, in_reverse,
          nullptr);
      local_path = search->FindPath();
    }

    if (!local_path.empty())
      return local_path;
  }

  RebuildDirtyGrids();

  // If the target cell is unreachable, there is no path.(上游注释)
  const std::optional<CPos> target_abstract_cell =
      AbstractCellForLocalCell(target);
  if (!target_abstract_cell.has_value())
    return NoPathVector();

  // If the source cell is unreachable, there may still be a path.(上游注释段)
  const std::optional<CPos> source_abstract_cell =
      AbstractCellForLocalCell(source);
  if (!source_abstract_cell.has_value())
    return FindPath(self, std::span{&source, 1}, target, check,
                    heuristic_weight_percentage, custom_cost, ignore_actor,
                    in_reverse, lane_bias);

  // If the source and target belong to different domains, there is no
  // path.(上游注释)| (upstream comment)
  RebuildDomains();
  const std::uint32_t target_domain = map_abstract_domains_[*target_abstract_cell];
  const std::uint32_t source_domain = map_abstract_domains_[*source_abstract_cell];
  if (source_domain != target_domain)
    return NoPathVector();

  const std::optional<GraphEdge> target_edge =
      EdgeFromLocalToAbstract(target, *target_abstract_cell);
  const std::optional<GraphEdge> source_edge =
      EdgeFromLocalToAbstract(source, *source_abstract_cell);

  // The new edges will be treated as bi-directional.(上游注释)
  std::vector<GraphEdge> source_edges;
  if (source_edge.has_value())
    source_edges.push_back(*source_edge);
  AbstractGraphWithInsertedEdges full_graph{map_abstract_graph_, source_edges,
                                           target_edge.has_value()
                                               ? &*target_edge
                                               : nullptr,
                                           fn_cost_estimator_};

  // Determine an abstract path in both directions(上游注释段)
  const int estimated_search_size =
      static_cast<int>((map_abstract_graph_.size() + 2) / 8);
  SparsePathGraph::EdgesFn connections_fn =
      [&full_graph](CPos position) {
        return full_graph.GetConnections(position);
      };
  auto forward_abstract_search = PathSearch::ToTargetCellOverGraph(
      connections_fn, locomotor_, source, target, estimated_search_size);
  if (!forward_abstract_search->ExpandToTarget())
    return NoPathVector();

  auto reverse_abstract_search = PathSearch::ToTargetCellOverGraph(
      connections_fn, locomotor_, target, source, estimated_search_size);
  reverse_abstract_search->ExpandToTarget();

  auto from_src = GetLocalPathSearch(
      self, std::span{&source, 1}, target, custom_cost, ignore_actor, check,
      lane_bias, nullptr, heuristic_weight_percentage,
      MakeHeuristic(*reverse_abstract_search, estimated_search_size, nullptr,
                    nullptr),
      in_reverse, nullptr);
  auto from_dest = GetLocalPathSearch(
      self, std::span{&target, 1}, source, custom_cost, ignore_actor, check,
      lane_bias, nullptr, heuristic_weight_percentage,
      MakeHeuristic(*forward_abstract_search, estimated_search_size, nullptr,
                    nullptr),
      !in_reverse, nullptr);
  return PathSearch::FindBidiPath(*from_dest, *from_src);
}

// ———— FindPath 多源(L732-826)————
std::vector<CPos> HierarchicalPathFinder::FindPath(
    Actor* self, std::span<const CPos> sources, CPos target,
    BlockedByActor check, int heuristic_weight_percentage,
    const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
    bool in_reverse, bool lane_bias) {
  if (fn_cost_estimator_ == nullptr)
    return NoPathVector();

  if (!world_.Map().Contains(target))
    return NoPathVector();

  RebuildDirtyGrids();

  const std::optional<CPos> target_abstract_cell =
      AbstractCellForLocalCell(target);
  if (!target_abstract_cell.has_value())
    return NoPathVector();

  RebuildDomains();
  const std::uint32_t target_domain = map_abstract_domains_[*target_abstract_cell];

  // Unlike the target cell, the source cell is allowed to be an
  // unreachable location.(上游注释段)| (upstream comment)
  CPosWorkSet sources_with_pathable_nodes;
  std::vector<GraphEdge> source_edges;
  std::optional<std::vector<CPos>> unpathable_nodes;
  for (const CPos& source : sources) {
    if (!world_.Map().Contains(source))
      continue;

    // The source cell is reachable, we can add an edge from there(上游注释)
    const std::optional<CPos> source_abstract_cell =
        AbstractCellForLocalCell(source);
    if (source_abstract_cell.has_value()) {
      // If the source and target belong to different domains(上游注释)
      const std::uint32_t source_domain =
          map_abstract_domains_[*source_abstract_cell];
      if (source_domain != target_domain)
        continue;

      sources_with_pathable_nodes.Add(source);
      const std::optional<GraphEdge> source_edge =
          EdgeFromLocalToAbstract(source, *source_abstract_cell);
      if (source_edge.has_value())
        source_edges.push_back(*source_edge);
      continue;
    }

    // If the source cell is unreachable, we must add edges from any
    // adjacent cells that are reachable instead.(上游注释段)
    for (const CVec& dir : CVec::Directions()) {
      const CPos adjacent_source = source + dir;
      if (!MovementAllowedBetweenCells(source, adjacent_source))
        continue;

      const std::optional<CPos> adjacent_source_abstract_cell =
          AbstractCellForLocalCell(adjacent_source);
      if (!adjacent_source_abstract_cell.has_value())
        continue;

      // If the source and target belong to different domains(上游注释)
      const std::uint32_t adjacent_source_domain =
          map_abstract_domains_[*adjacent_source_abstract_cell];
      if (adjacent_source_domain != target_domain) {
        if (!unpathable_nodes.has_value())
          unpathable_nodes.emplace();
        unpathable_nodes->push_back(adjacent_source);
        continue;
      }

      sources_with_pathable_nodes.Add(source);
      const std::optional<GraphEdge> source_edge = EdgeFromLocalToAbstract(
          adjacent_source, *adjacent_source_abstract_cell);
      if (source_edge.has_value())
        source_edges.push_back(*source_edge);
    }
  }

  if (sources_with_pathable_nodes.Count() == 0)
    return NoPathVector();

  const std::optional<GraphEdge> target_edge =
      EdgeFromLocalToAbstract(target, *target_abstract_cell);

  // The new edges will be treated as bi-directional.(上游注释)
  AbstractGraphWithInsertedEdges full_graph{map_abstract_graph_, source_edges,
                                            target_edge.has_value()
                                                ? &*target_edge
                                                : nullptr,
                                            fn_cost_estimator_};

  // Determine an abstract path to all sources(上游注释段)
  const int estimated_search_size =
      static_cast<int>((map_abstract_graph_.size() + 2) / 8);
  SparsePathGraph::EdgesFn connections_fn =
      [&full_graph](CPos position) {
        return full_graph.GetConnections(position);
      };
  auto reverse_abstract_search = PathSearch::ToTargetCellOverGraph(
      connections_fn, locomotor_, target, target, estimated_search_size);

  auto from_src = GetLocalPathSearch(
      self, sources_with_pathable_nodes.Vec(), target, custom_cost,
      ignore_actor, check, lane_bias, nullptr, heuristic_weight_percentage,
      MakeHeuristic(*reverse_abstract_search, estimated_search_size,
                    &sources_with_pathable_nodes.Vec(),
                    unpathable_nodes.has_value() ? &*unpathable_nodes : nullptr),
      in_reverse, nullptr);
  return from_src->FindPath();
}

// ———— PathExists(L942-984)————
bool HierarchicalPathFinder::PathExists(CPos source, CPos target) {
  if (fn_cost_estimator_ == nullptr)
    return false;

  if (!world_.Map().Contains(source) || !world_.Map().Contains(target))
    return false;

  RebuildDomains();

  const std::optional<CPos> abstract_target = AbstractCellForLocalCell(target);
  if (!abstract_target.has_value())
    return false;
  const std::uint32_t target_domain = map_abstract_domains_[*abstract_target];

  // The source cell is reachable, we can compare the domains directly.
  // (上游注释)| (upstream comment)
  const std::optional<CPos> abstract_source = AbstractCellForLocalCell(source);
  if (abstract_source.has_value()) {
    const std::uint32_t source_domain = map_abstract_domains_[*abstract_source];
    return source_domain == target_domain;
  }

  // Unlike the target cell, the source cell is allowed to be an
  // unreachable location.(上游注释段)| (upstream comment)
  for (const CVec& dir : CVec::Directions()) {
    const CPos adjacent_source = source + dir;
    if (!MovementAllowedBetweenCells(source, adjacent_source))
      continue;

    const std::optional<CPos> abstract_adjacent_source =
        AbstractCellForLocalCell(adjacent_source);
    if (!abstract_adjacent_source.has_value())
      continue;

    const std::uint32_t adjacent_source_domain =
        map_abstract_domains_[*abstract_adjacent_source];
    if (adjacent_source_domain == target_domain)
      return true;
  }

  return false;
}

// ———— RebuildDirtyGrids(L990-1008)————
void HierarchicalPathFinder::RebuildDirtyGrids() {
  if (vec_dirty_grid_indexes_.empty())
    return;

  // An empty domain indicates it is out of date(上游注释)
  map_abstract_domains_.clear();

  const auto custom_movement_layers = world_.CustomMovementLayers();
  for (const int grid_index : vec_dirty_grid_indexes_) {
    const GridInfo old_grid = vec_grid_infos_[static_cast<std::size_t>(grid_index)];
    const CPos grid_top_left = GetGridTopLeft(grid_index, 0);
    vec_grid_infos_[static_cast<std::size_t>(grid_index)] =
        BuildGrid(grid_top_left.X(), grid_top_left.Y(), custom_movement_layers);
    RebuildCostTable(grid_top_left.X(), grid_top_left.Y(), old_grid,
                     custom_movement_layers);
  }

  vec_dirty_grid_indexes_.clear();
  std::fill(vec_dirty_grid_bitmap_.begin(), vec_dirty_grid_bitmap_.end(), 0);
}

// ———— RebuildCostTable(L1014-1050)————
void HierarchicalPathFinder::RebuildCostTable(
    int grid_x, int grid_y, const GridInfo& old_grid,
    std::span<sim::ICustomMovementLayer* const> custom_movement_layers) {
  // For this grid, it is possible the abstract nodes have changed.
  // (上游注释段)| (upstream comment)
  std::vector<CPos> abstract_nodes;
  old_grid.CopyAbstractCellsInto(abstract_nodes);
  for (const CPos& old_abstract_node : abstract_nodes)
    map_abstract_graph_.erase(old_abstract_node);
  abstract_nodes.clear();

  // Add new abstract edges for this grid(上游注释)
  for (auto& [key, edges] :
       GetAbstractEdgesForGrid(grid_x, grid_y, custom_movement_layers))
    map_abstract_graph_.insert_or_assign(key, std::move(edges));

  for (const CVec& direction : CVec::Directions()) {
    const CPos adjacent_grid =
        CPos{grid_x, grid_y} +
        CVec{direction.X * kGridSize, direction.Y * kGridSize};
    if (!map_bounds_.Contains(adjacent_grid))
      continue;

    // For all adjacent grids, their abstract nodes will not have changed.
    // (上游注释段)| (upstream comment)
    abstract_nodes.clear();
    vec_grid_infos_[static_cast<std::size_t>(GridIndex(adjacent_grid))]
        .CopyAbstractCellsInto(abstract_nodes);
    std::unordered_set<CPos> node_set(abstract_nodes.begin(),
                                      abstract_nodes.end());
    for (auto& [key, edges] : GetAbstractEdgesForGrid(
             adjacent_grid.X(), adjacent_grid.Y(), custom_movement_layers)) {
      map_abstract_graph_.insert_or_assign(key, std::move(edges));
      node_set.erase(key);
    }

    // If any nodes were left over they have no connections now(上游注释)
    for (const CPos& unconnected_node : node_set)
      map_abstract_graph_.erase(unconnected_node);
  }
}

// ———— RebuildDomains(L1056-1092)————
void HierarchicalPathFinder::RebuildDomains() {
  // First, rebuild the abstract graph if it is out of date.(上游注释)
  RebuildDirtyGrids();

  // Check if our domain cache is empty(上游注释)
  if (!map_abstract_domains_.empty())
    return;

  const auto abstract_edge = [this](CPos abstract_cell)
      -> const std::vector<GraphConnection>* {
    auto it = map_abstract_graph_.find(abstract_cell);
    return it != map_abstract_graph_.end() ? &it->second : nullptr;
  };

  // As in BuildGrid, flood fill the search graph until all disjoint
  // domains are discovered.(上游注释)| (upstream comment)
  std::uint32_t domain = 0;
  CPosWorkSet abstract_cells;
  for (const GridInfo& grid : vec_grid_infos_) {
    std::vector<CPos> tmp;
    grid.CopyAbstractCellsInto(tmp);
    for (const CPos& c : tmp)
      abstract_cells.Add(c);  // 去重(CopyAbstractCellsInto 的 set 语义)
                              // dedup (the set semantics).
  }
  while (abstract_cells.Count() > 0) {
    const CPos search_cell = abstract_cells.First();
    std::unique_ptr<PathSearch> search = PathSearch::ToTargetCellOverGraph(
        [abstract_edge](CPos cell) {
          const auto* e = abstract_edge(cell);
          return e != nullptr ? *e : std::vector<GraphConnection>{};
        },
        locomotor_, search_cell, search_cell,
        static_cast<int>(map_abstract_graph_.size() / 8));
    const std::vector<CPos> searched = search->ExpandAll();
    for (const CPos& abstract_cell : searched)
      map_abstract_domains_.emplace(abstract_cell, domain);
    abstract_cells.ExceptWith(searched);
    domain++;
  }
}

// ———— AbstractCellForLocalCell(L1098-1111)————
std::optional<CPos> HierarchicalPathFinder::AbstractCellForLocalCell(
    CPos local_cell) {
  return vec_grid_infos_[static_cast<std::size_t>(GridIndex(local_cell))]
      .AbstractCellForLocalCell(local_cell, this);
}

std::optional<CPos> HierarchicalPathFinder::AbstractCellForLocalCellNoAccessibleCheck(
    CPos local_cell) {
  return vec_grid_infos_[static_cast<std::size_t>(GridIndex(local_cell))]
      .AbstractCellForLocalCell(local_cell, nullptr);
}

// ———— EdgeFromLocalToAbstract(L1117-1123)————
std::optional<GraphEdge> HierarchicalPathFinder::EdgeFromLocalToAbstract(
    CPos local_cell, CPos abstract_cell) const {
  if (local_cell == abstract_cell)
    return std::nullopt;

  return GraphEdge{local_cell, abstract_cell,
                   fn_cost_estimator_(local_cell, abstract_cell)};
}

// ———— MakeHeuristic(L1130-1215)————
std::function<int(CPos, bool)> HierarchicalPathFinder::MakeHeuristic(
    PathSearch& abstract_search, int estimated_search_size,
    const std::vector<CPos>* sources,
    const std::vector<CPos>* unpathable_nodes) {
  auto node_for_cost_lookup = std::make_shared<
      std::unordered_map<CPos, CPos>>(
      static_cast<std::size_t>(estimated_search_size));
  // graph = (SparsePathGraph)abstractSearch.Graph(L1134)
  SparsePathGraph* graph = static_cast<SparsePathGraph*>(&abstract_search.Graph());

  return [this, &abstract_search, graph, node_for_cost_lookup, sources,
          unpathable_nodes](CPos cell, bool known_accessible) {
    // When dealing with an unreachable source cell(上游注释段)
    if (unpathable_nodes != nullptr &&
        std::find(unpathable_nodes->begin(), unpathable_nodes->end(), cell) !=
            unpathable_nodes->end())
      return kPathCostForInvalidPath;

    // During a search, all other cells searched by the heuristic are
    // guaranteed to be reachable.(上游注释段)| (upstream comment)
    std::optional<CPos> maybe_abstract_cell;
    if (known_accessible)
      maybe_abstract_cell = AbstractCellForLocalCellNoAccessibleCheck(cell);
    else
      maybe_abstract_cell = AbstractCellForLocalCell(cell);

    if (!maybe_abstract_cell.has_value()) {
      // If the source cell is unreachable, use one of the adjacent
      // reachable cells instead.(上游注释段)| (upstream comment)
      if (sources != nullptr &&
          std::find(sources->begin(), sources->end(), cell) != sources->end()) {
        for (const CVec& dir : CVec::Directions()) {
          const CPos adjacent_cell = cell + dir;
          if (!MovementAllowedBetweenCells(cell, adjacent_cell) ||
              (unpathable_nodes != nullptr &&
               std::find(unpathable_nodes->begin(), unpathable_nodes->end(),
                         adjacent_cell) != unpathable_nodes->end()))
            continue;

          // Ideally we'd choose the cheapest cell rather than just any one
          // of them(上游注释)| (upstream comment)
          maybe_abstract_cell = AbstractCellForLocalCell(adjacent_cell);
          if (maybe_abstract_cell.has_value())
            break;
        }
      }

      if (!maybe_abstract_cell.has_value())
        throw std::runtime_error(
            std::format("The abstract path should never be searched for an "
                        "unreachable point. Cell ({},{},{}) failed lookup for "
                        "an abstract cell.",
                        cell.X(), cell.Y(), cell.Layer()));
    }

    const CPos abstract_cell = *maybe_abstract_cell;
    CellInfo info = graph->At(abstract_cell);

    // Expand the abstract search only if we have yet to get a route to the
    // abstract cell.(上游注释段)| (upstream comment)
    if (info.status != CellStatus::Closed) {
      abstract_search.SetTargetPredicate(
          [abstract_cell](CPos c) { return c == abstract_cell; });
      if (!abstract_search.ExpandToTarget())
        throw std::runtime_error(
            std::format("The abstract path should never be searched for an "
                        "unreachable point. Abstract cell ({},{},{}) failed "
                        "to route to abstract cell.",
                        abstract_cell.X(), abstract_cell.Y(),
                        abstract_cell.Layer()));
      info = graph->At(abstract_cell);
    }

    CPos abstract_node = info.previous_node;

    // When transitioning between layers, the XY will be the same(上游注释段)
    if (abstract_cell.Layer() != abstract_node.Layer())
      abstract_node = graph->At(abstract_node).previous_node;

    // Now we have an abstract node to target, determine if there is one
    // further along the path we can use.(上游注释段)| (upstream comment)
    auto it = node_for_cost_lookup->find(abstract_node);
    if (it == node_for_cost_lookup->end()) {
      const CPos abstract_node_for_cost =
          AbstractNodeForCost(*graph, abstract_cell, abstract_node);
      it = node_for_cost_lookup->emplace(abstract_node, abstract_node_for_cost)
               .first;
    }
    const CPos abstract_node_for_cost = it->second;

    const int cost = graph->At(abstract_node_for_cost).cost_so_far +
                     fn_cost_estimator_(cell, abstract_node_for_cost);
    return cost;
  };
}

// ———— AbstractNodeForCost(L1223-1270)————
CPos HierarchicalPathFinder::AbstractNodeForCost(const SparsePathGraph& graph,
                                                 CPos abstract_cell,
                                                 CPos abstract_node) {
  // We currently have the next abstract node along our path.(上游注释段)
  std::vector<CPos> abstract_nodes_along_path;
  while (true) {
    const CPos previous_abstract_node = graph.At(abstract_node).previous_node;

    // The whole abstract path has been travelled, can't go further.
    // (上游注释)| (upstream comment)
    if (previous_abstract_node == abstract_node)
      break;

    // Check if we can move directly to the new node whilst staying
    // within the boundary of the abstract path so far.(上游注释段)
    bool intersects_all_nodes = true;
    abstract_nodes_along_path.push_back(abstract_node);
    for (const CPos& node : abstract_nodes_along_path) {
      if (!GetGrid(node, map_bounds_).IntersectsLine(abstract_cell,
                                                     previous_abstract_node)) {
        intersects_all_nodes = false;
        break;
      }
    }

    if (!intersects_all_nodes)
      break;

    abstract_node = previous_abstract_node;
  }

  return abstract_node;
}

// ———— GetLocalPathSearch(L1272-1282)————
std::unique_ptr<PathSearch> HierarchicalPathFinder::GetLocalPathSearch(
    Actor* self, std::span<const CPos> srcs, CPos dst,
    const std::function<int(CPos)>& custom_cost, Actor* ignore_actor,
    BlockedByActor check, bool lane_bias, const Grid* grid,
    int heuristic_weight_percentage,
    const std::function<int(CPos, bool)>& heuristic, bool in_reverse,
    IRecorder* recorder) {
  return PathSearch::ToTargetCell(world_, locomotor_, self, srcs, dst, check,
                                  heuristic_weight_percentage, custom_cost,
                                  ignore_actor, lane_bias, in_reverse,
                                  heuristic, grid, recorder);
}

}  // namespace ora::mods::pathfinding
