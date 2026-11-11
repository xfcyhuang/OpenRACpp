module ora.game.pathfinding;

import std;
import ora.game;
import ora.game.miniyaml;

namespace ora::game::pathfinding {

namespace {

constexpr std::int32_t kGridSize{10};
constexpr std::int32_t kCloseGridDistance{2};
constexpr std::int32_t kLaneBiasCost{1};
constexpr std::int32_t kSqrtTwoNum{46341};
constexpr std::int32_t kSqrtTwoDen{32768};
constexpr std::int32_t kInvalidPath{std::numeric_limits<std::int32_t>::max()};
constexpr std::uint8_t kStatusOpen{1};
constexpr std::uint8_t kStatusClosed{2};

struct CellVec {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
};

constexpr CellVec kDirections8[]{
    CellVec{-1, -1}, CellVec{-1, 0}, CellVec{-1, 1}, CellVec{0, -1},
    CellVec{0, 1},   CellVec{1, -1}, CellVec{1, 0},  CellVec{1, 1},
};

struct NeighborSet {
  const CellVec* ptr_dirs;
  std::size_t sz_count;
};

constexpr CellVec kDirectedNeighbors[][5]{
    {CellVec{-1, -1}, CellVec{0, -1}, CellVec{1, -1}, CellVec{-1, 0}, CellVec{-1, 1}},
    {CellVec{-1, -1}, CellVec{0, -1}, CellVec{1, -1}, CellVec{0, 0}, CellVec{0, 0}},
    {CellVec{-1, -1}, CellVec{0, -1}, CellVec{1, -1}, CellVec{1, 0}, CellVec{1, 1}},
    {CellVec{-1, -1}, CellVec{-1, 0}, CellVec{-1, 1}, CellVec{0, 0}, CellVec{0, 0}},
    {CellVec{0, 0}, CellVec{0, 0}, CellVec{0, 0}, CellVec{0, 0}, CellVec{0, 0}},
    {CellVec{1, -1}, CellVec{1, 0}, CellVec{1, 1}, CellVec{0, 0}, CellVec{0, 0}},
    {CellVec{-1, -1}, CellVec{-1, 0}, CellVec{-1, 1}, CellVec{0, 1}, CellVec{1, 1}},
    {CellVec{-1, 1}, CellVec{0, 1}, CellVec{1, 1}, CellVec{0, 0}, CellVec{0, 0}},
    {CellVec{1, -1}, CellVec{1, 0}, CellVec{-1, 1}, CellVec{0, 1}, CellVec{1, 1}},
};

constexpr std::size_t kDirectedNeighborsCount[]{5, 3, 5, 3, 0, 3, 5, 3, 5};

using CellKey = std::pair<std::int32_t, std::int32_t>;

CellKey key_of(CPos cell) { return CellKey{cell.int4_x, cell.int4_y}; }

std::int32_t multiply_by_sqrt_two(std::int32_t int4_number) {
  return int4_number * kSqrtTwoNum / kSqrtTwoDen;
}

std::int32_t estimator_between(const HpfGraph& graph, CPos cell_here, CPos cell_dst) {
  const std::int32_t int4_dx{cell_here.int4_x > cell_dst.int4_x
                                 ? cell_here.int4_x - cell_dst.int4_x
                                 : cell_dst.int4_x - cell_here.int4_x};
  const std::int32_t int4_dy{cell_here.int4_y > cell_dst.int4_y
                                 ? cell_here.int4_y - cell_dst.int4_y
                                 : cell_dst.int4_y - cell_here.int4_y};
  const std::int32_t int4_diag{int4_dx < int4_dy ? int4_dx : int4_dy};
  return graph.int4_min_cost * (int4_dx + int4_dy) +
         (graph.int4_diag_cost - 2 * graph.int4_min_cost) * int4_diag;
}

struct GridBounds {
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};

  [[nodiscard]] bool contains(CPos cell) const {
    return cell.int4_x >= int4_left && cell.int4_x < int4_right &&
           cell.int4_y >= int4_top && cell.int4_y < int4_bottom;
  }
};

std::int32_t winding_test(std::int32_t int4_v0x, std::int32_t int4_v0y, std::int32_t int4_v1x,
                          std::int32_t int4_v1y, std::int32_t int4_px, std::int32_t int4_py) {
  const std::int64_t int8_cross{static_cast<std::int64_t>(int4_v1x - int4_v0x) * (int4_py - int4_v0y) -
                                static_cast<std::int64_t>(int4_px - int4_v0x) * (int4_v1y - int4_v0y)};
  return int8_cross > 0 ? 1 : int8_cross < 0 ? -1 : 0;
}

bool lines_intersect(std::int32_t int4_ax, std::int32_t int4_ay, std::int32_t int4_bx,
                     std::int32_t int4_by, std::int32_t int4_cx, std::int32_t int4_cy,
                     std::int32_t int4_dx, std::int32_t int4_dy) {
  return winding_test(int4_cx, int4_cy, int4_dx, int4_dy, int4_ax, int4_ay) !=
             winding_test(int4_cx, int4_cy, int4_dx, int4_dy, int4_bx, int4_by) &&
         winding_test(int4_ax, int4_ay, int4_bx, int4_by, int4_cx, int4_cy) !=
             winding_test(int4_ax, int4_ay, int4_bx, int4_by, int4_dx, int4_dy);
}

bool bounds_intersect_line(const GridBounds& bounds, CPos cell_start, CPos cell_end) {
  const std::int32_t int4_ax{cell_start.int4_x}, int4_ay{cell_start.int4_y};
  const std::int32_t int4_bx{cell_end.int4_x}, int4_by{cell_end.int4_y};
  const std::int32_t int4_tl_x{bounds.int4_left}, int4_tl_y{bounds.int4_top};
  const std::int32_t int4_tr_x{bounds.int4_right}, int4_br_y{bounds.int4_bottom};
  return lines_intersect(int4_ax, int4_ay, int4_bx, int4_by, int4_tl_x, int4_tl_y, int4_tr_x,
                         int4_tl_y) ||
         lines_intersect(int4_ax, int4_ay, int4_bx, int4_by, int4_tl_x, int4_tl_y, int4_tl_x,
                         int4_br_y) ||
         lines_intersect(int4_ax, int4_ay, int4_bx, int4_by, int4_tl_x, int4_br_y, int4_tr_x,
                         int4_br_y) ||
         lines_intersect(int4_ax, int4_ay, int4_bx, int4_by, int4_tr_x, int4_tl_y, int4_tr_x,
                         int4_br_y);
}

struct ChangedEdges {
  const HpfGraph* ptr_graph{};
  std::map<CellKey, std::vector<GraphConnection>> map_changed;

  [[nodiscard]] const std::vector<GraphConnection>* abstract_edges_at(CPos cell) const {
    const auto it{ptr_graph->map_abstract_edges.find(key_of(cell))};
    return it == ptr_graph->map_abstract_edges.end() ? nullptr : &it->second;
  }

  [[nodiscard]] std::vector<GraphConnection> get(CPos cell) const {
    const auto it{map_changed.find(key_of(cell))};
    if (it != map_changed.end()) return it->second;
    if (const std::vector<GraphConnection>* ptr_edges{abstract_edges_at(cell)})
      return *ptr_edges;
    return {};
  }
};

struct AbstractSearch {
  const HpfGraph* ptr_graph{};
  const ChangedEdges* ptr_edges{};
  CPos cell_heuristic_dst{};
  CPos cell_predicate_goal{};
  std::vector<CellInfoNode> vec_cells;
  LevelOrderQueue queue;

  void init() {
    vec_cells.assign(static_cast<std::size_t>(ptr_graph->ptr_map->int4_width) *
                         ptr_graph->ptr_map->int4_height,
                     CellInfoNode{});
  }

  CellInfoNode& info(CPos cell) {
    return vec_cells[static_cast<std::size_t>(cell.int4_x) * ptr_graph->ptr_map->int4_height +
                     cell.int4_y];
  }

  [[nodiscard]] std::int32_t heuristic(CPos cell) const {
    return estimator_between(*ptr_graph, cell, cell_heuristic_dst);
  }

  void add_initial_cell(CPos cell) {
    const std::int32_t int4_heuristic{heuristic(cell)};
    if (int4_heuristic == kInvalidPath) return;
    CellInfoNode& node{info(cell)};
    node.u1_status = kStatusOpen;
    node.int4_cost_so_far = 0;
    node.int4_est_total = int4_heuristic;
    node.cell_prev = cell;
    queue_add(queue, GraphConnection{cell, int4_heuristic});
  }

  bool can_expand() {
    while (true) {
      if (queue_empty(queue)) return false;
      if (info(queue.vec_items[0].cell).u1_status == kStatusClosed) {
        queue_pop(queue);
        continue;
      }
      return true;
    }
  }

  CPos expand() {
    const CPos cell_current{queue_pop(queue).cell};
    CellInfoNode& info_current{info(cell_current)};
    const std::int32_t int4_cost_so_far{info_current.int4_cost_so_far};
    info_current.u1_status = kStatusClosed;

    for (const GraphConnection& conn : ptr_edges->get(cell_current)) {
      const std::int32_t int4_cost_to{int4_cost_so_far + conn.int4_cost};
      CellInfoNode& node{info(conn.cell)};

      if (node.u1_status == kStatusClosed ||
          (node.u1_status == kStatusOpen && int4_cost_to >= node.int4_cost_so_far))
        continue;

      std::int32_t int4_remaining{};
      if (node.u1_status == kStatusOpen)
        int4_remaining = node.int4_est_total - node.int4_cost_so_far;
      else
        int4_remaining = heuristic(conn.cell);

      node.u1_status = kStatusOpen;
      node.int4_cost_so_far = int4_cost_to;
      node.int4_est_total = int4_cost_to + int4_remaining;
      node.cell_prev = cell_current;
      queue_add(queue, GraphConnection{conn.cell, int4_cost_to + int4_remaining});
    }

    return cell_current;
  }

  bool expand_to_target() {
    while (can_expand())
      if (expand() == cell_predicate_goal) return true;
    return false;
  }

  std::vector<CPos> expand_all() {
    std::vector<CPos> vec_closed;
    while (can_expand()) vec_closed.push_back(expand());
    return vec_closed;
  }
};

struct AbstractHeuristic {
  AbstractSearch* ptr_abstract{};
  const HpfGraph* ptr_graph{};
  std::vector<CPos> vec_unpathable;
  bool b_has_known_source{};
  CPos cell_known_source;
  mutable std::map<CellKey, CPos> map_node_for_cost;

  [[nodiscard]] bool unpathable_contains(CPos cell) const {
    for (const CPos& known : vec_unpathable)
      if (known == cell) return true;
    return false;
  }

  std::int32_t operator()(CPos cell, bool b_known_accessible) const;
};

bool cell_is_accessible(const HpfGraph& graph, CPos cell) {
  return movement_cost_for_cell(*graph.ptr_map, cell) != kCostUnreachable;
}

GridBounds grid_bounds_for(const HpfGraph& graph, std::int32_t int4_grid_x,
                           std::int32_t int4_grid_y) {
  return GridBounds{int4_grid_x, int4_grid_y,
                    std::min(int4_grid_x + kGridSize, graph.ptr_map->int4_width),
                    std::min(int4_grid_y + kGridSize, graph.ptr_map->int4_height)};
}

std::optional<CPos> grid_abstract_cell(const HpfGridInfo& grid, CPos cell,
                                       bool b_check_accessible, const HpfGraph& graph) {
  if (b_check_accessible && !cell_is_accessible(graph, cell)) return std::nullopt;
  if (grid.b_has_single) return grid.cell_single;
  const auto it{grid.map_local_to_abstract.find(key_of(cell))};
  if (it != grid.map_local_to_abstract.end()) return it->second;
  return std::nullopt;
}

std::optional<CPos> abstract_cell_for(const HpfGraph& graph, CPos cell) {
  const HpfGridInfo& grid{graph.vec_grids[static_cast<std::size_t>(
      cell.int4_y / kGridSize * graph.int4_grid_xs + cell.int4_x / kGridSize)]};
  return grid_abstract_cell(grid, cell, true, graph);
}

std::optional<CPos> abstract_cell_for_no_check(const HpfGraph& graph, CPos cell) {
  const HpfGridInfo& grid{graph.vec_grids[static_cast<std::size_t>(
      cell.int4_y / kGridSize * graph.int4_grid_xs + cell.int4_x / kGridSize)]};
  return grid_abstract_cell(grid, cell, false, graph);
}

CPos abstract_node_for_cost(AbstractSearch& search, const HpfGraph& graph,
                            CPos cell_abstract, CPos cell_node) {
  std::vector<CPos> vec_along;
  while (true) {
    const CPos cell_previous{search.info(cell_node).cell_prev};
    if (cell_previous == cell_node) break;
    vec_along.push_back(cell_node);
    bool b_intersects_all{true};
    for (const CPos& cell_check : vec_along) {
      const GridBounds bounds_node{grid_bounds_for(
          graph, cell_check.int4_x / kGridSize * kGridSize, cell_check.int4_y / kGridSize * kGridSize)};
      if (!bounds_intersect_line(bounds_node, cell_abstract, cell_previous)) {
        b_intersects_all = false;
        break;
      }
    }
    if (!b_intersects_all) break;
    cell_node = cell_previous;
  }
  return cell_node;
}

std::int32_t AbstractHeuristic::operator()(CPos cell, bool b_known_accessible) const {
  if (unpathable_contains(cell)) return kInvalidPath;
  std::optional<CPos> opt_abstract{b_known_accessible
                                       ? abstract_cell_for_no_check(*ptr_graph, cell)
                                       : abstract_cell_for(*ptr_graph, cell)};
  if (!opt_abstract.has_value() && b_has_known_source && cell == cell_known_source) {
    for (const CellVec& vec_dir : kDirections8) {
      const CPos cell_adjacent{cell.int4_x + vec_dir.int4_x, cell.int4_y + vec_dir.int4_y};
      if (movement_cost_for_cell(*ptr_graph->ptr_map, cell_adjacent) == kCostUnreachable ||
          unpathable_contains(cell_adjacent))
        continue;
      opt_abstract = abstract_cell_for(*ptr_graph, cell_adjacent);
      if (opt_abstract.has_value()) break;
    }
  }
  if (!opt_abstract.has_value()) return kInvalidPath;

  if (ptr_abstract->info(*opt_abstract).u1_status != kStatusClosed) {
    ptr_abstract->cell_predicate_goal = *opt_abstract;
    if (!ptr_abstract->expand_to_target()) return kInvalidPath;
  }

  CPos cell_node{ptr_abstract->info(*opt_abstract).cell_prev};
  const CellKey key_node{key_of(cell_node)};
  CPos cell_for_cost;
  const auto it{map_node_for_cost.find(key_node)};
  if (it != map_node_for_cost.end()) {
    cell_for_cost = it->second;
  } else {
    cell_for_cost = abstract_node_for_cost(*ptr_abstract, *ptr_graph, *opt_abstract, cell_node);
    map_node_for_cost[key_node] = cell_for_cost;
  }
  return ptr_abstract->info(cell_for_cost).int4_cost_so_far +
         estimator_between(*ptr_graph, cell, cell_for_cost);
}

struct LocalSearch {
  const LocomotorMap* ptr_map{};
  const HpfGraph* ptr_graph{};
  std::int32_t int4_weight{100};
  bool b_lane_bias{};
  bool b_reverse{};
  CPos cell_goal{};
  CPos cell_heuristic_dst{};
  bool b_bounded{};
  GridBounds bounds_grid;
  const AbstractHeuristic* ptr_custom_heuristic{};
  std::int32_t int4_cell_cost{};
  std::int32_t int4_diag_cost{};
  std::vector<CellInfoNode> vec_cells;
  LevelOrderQueue queue;

  void init() {
    vec_cells.assign(static_cast<std::size_t>(ptr_map->int4_width) * ptr_map->int4_height,
                     CellInfoNode{});
  }

  CellInfoNode& info(CPos cell) {
    return vec_cells[static_cast<std::size_t>(cell.int4_x) * ptr_map->int4_height + cell.int4_y];
  }

  [[nodiscard]] std::int32_t default_heuristic(CPos cell) const {
    const std::int32_t int4_dx{cell.int4_x > cell_heuristic_dst.int4_x
                                   ? cell.int4_x - cell_heuristic_dst.int4_x
                                   : cell_heuristic_dst.int4_x - cell.int4_x};
    const std::int32_t int4_dy{cell.int4_y > cell_heuristic_dst.int4_y
                                   ? cell.int4_y - cell_heuristic_dst.int4_y
                                   : cell_heuristic_dst.int4_y - cell.int4_y};
    const std::int32_t int4_diag{int4_dx < int4_dy ? int4_dx : int4_dy};
    return int4_cell_cost * (int4_dx + int4_dy) + (int4_diag_cost - 2 * int4_cell_cost) * int4_diag;
  }

  [[nodiscard]] std::int32_t heuristic(CPos cell, bool b_known_accessible) const {
    if (ptr_custom_heuristic != nullptr)
      return (*ptr_custom_heuristic)(cell, b_known_accessible);
    return default_heuristic(cell);
  }

  [[nodiscard]] std::int32_t path_cost_to_node(CPos cell_dest, CellVec vec_dir) const {
    if (b_reverse && cell_dest == cell_goal) return 0;
    const std::int16_t int2_cost{movement_cost_for_cell(*ptr_map, cell_dest)};
    if (int2_cost == kCostUnreachable) return kInvalidPath;
    std::int32_t int4_cost{vec_dir.int4_x * vec_dir.int4_y != 0
                               ? multiply_by_sqrt_two(int2_cost)
                               : int2_cost};
    if (b_lane_bias) {
      const std::int32_t int4_ux{(cell_dest.int4_x + (b_reverse ? 1 : 0)) & 1};
      const std::int32_t int4_uy{(cell_dest.int4_y + (b_reverse ? 1 : 0)) & 1};
      if ((int4_ux == 0 && vec_dir.int4_y < 0) || (int4_ux == 1 && vec_dir.int4_y > 0))
        int4_cost += kLaneBiasCost;
      if ((int4_uy == 0 && vec_dir.int4_x < 0) || (int4_uy == 1 && vec_dir.int4_x > 0))
        int4_cost += kLaneBiasCost;
    }
    return int4_cost;
  }

  NeighborSet neighbor_set(CPos cell_position) {
    const CellInfoNode& info_prev{info(cell_position)};
    const std::int32_t int4_dx{cell_position.int4_x - info_prev.cell_prev.int4_x};
    const std::int32_t int4_dy{cell_position.int4_y - info_prev.cell_prev.int4_y};
    const std::int32_t int4_index{int4_dy * 3 + int4_dx + 4};
    if (int4_index == 4) return NeighborSet{kDirections8, 8};
    return NeighborSet{kDirectedNeighbors[int4_index], kDirectedNeighborsCount[int4_index]};
  }

  std::vector<GraphConnection> get_connections(CPos cell_position) {
    const NeighborSet set_dirs{neighbor_set(cell_position)};
    std::vector<GraphConnection> vec_connections;
    vec_connections.reserve(set_dirs.sz_count);
    for (std::size_t idx{0}; idx < set_dirs.sz_count; ++idx) {
      const CellVec vec_dir{set_dirs.ptr_dirs[idx]};
      CPos cell_neighbor{cell_position.int4_x + vec_dir.int4_x,
                         cell_position.int4_y + vec_dir.int4_y};
      if (b_bounded && !bounds_grid.contains(cell_neighbor)) continue;
      const std::int32_t int4_cost{path_cost_to_node(cell_neighbor, vec_dir)};
      if (int4_cost != kInvalidPath && info(cell_neighbor).u1_status != kStatusClosed)
        vec_connections.push_back(GraphConnection{cell_neighbor, int4_cost});
    }
    return vec_connections;
  }

  void add_initial_cell(CPos cell) {
    const std::int32_t int4_heuristic{heuristic(cell, false)};
    if (int4_heuristic == kInvalidPath) return;
    const std::int32_t int4_estimated{int4_heuristic * int4_weight / 100};
    CellInfoNode& node{info(cell)};
    node.u1_status = kStatusOpen;
    node.int4_cost_so_far = 0;
    node.int4_est_total = int4_estimated;
    node.cell_prev = cell;
    queue_add(queue, GraphConnection{cell, int4_estimated});
  }

  bool can_expand() {
    while (true) {
      if (queue_empty(queue)) return false;
      if (info(queue.vec_items[0].cell).u1_status == kStatusClosed) {
        queue_pop(queue);
        continue;
      }
      return true;
    }
  }

  CPos expand() {
    const CPos cell_current{queue_pop(queue).cell};
    CellInfoNode& info_current{info(cell_current)};
    const std::int32_t int4_cost_so_far{info_current.int4_cost_so_far};
    info_current.u1_status = kStatusClosed;

    for (const GraphConnection& conn : get_connections(cell_current)) {
      const std::int32_t int4_cost_to{int4_cost_so_far + conn.int4_cost};
      CellInfoNode& node{info(conn.cell)};

      if (node.u1_status == kStatusClosed ||
          (node.u1_status == kStatusOpen && int4_cost_to >= node.int4_cost_so_far))
        continue;

      std::int32_t int4_remaining{};
      if (node.u1_status == kStatusOpen)
        int4_remaining = node.int4_est_total - node.int4_cost_so_far;
      else
        int4_remaining = heuristic(conn.cell, true) * int4_weight / 100;

      node.u1_status = kStatusOpen;
      node.int4_cost_so_far = int4_cost_to;
      node.int4_est_total = int4_cost_to + int4_remaining;
      node.cell_prev = cell_current;
      queue_add(queue, GraphConnection{conn.cell, int4_cost_to + int4_remaining});
    }

    return cell_current;
  }

  std::vector<CPos> find_path() {
    while (can_expand()) {
      const CPos cell{expand()};
      if (cell == cell_goal) {
        std::vector<CPos> vec_path;
        CPos cell_node{cell};
        while (true) {
          vec_path.push_back(cell_node);
          const CellInfoNode& node{info(cell_node)};
          if (node.cell_prev == cell_node) break;
          cell_node = node.cell_prev;
        }
        return vec_path;
      }
    }
    return {};
  }

  std::vector<CPos> expand_all() {
    std::vector<CPos> vec_closed;
    while (can_expand()) vec_closed.push_back(expand());
    return vec_closed;
  }
};

CPos abstract_cell_for_local_cells(const std::vector<CPos>& vec_cells) {
  std::int32_t int4_min_x{std::numeric_limits<std::int32_t>::max()};
  std::int32_t int4_min_y{std::numeric_limits<std::int32_t>::max()};
  std::int32_t int4_max_x{std::numeric_limits<std::int32_t>::min()};
  std::int32_t int4_max_y{std::numeric_limits<std::int32_t>::min()};
  for (const CPos& cell : vec_cells) {
    int4_min_x = std::min(int4_min_x, cell.int4_x);
    int4_min_y = std::min(int4_min_y, cell.int4_y);
    int4_max_x = std::max(int4_max_x, cell.int4_x);
    int4_max_y = std::max(int4_max_y, cell.int4_y);
  }
  const CPos cell_desired{int4_min_x + (int4_max_x - int4_min_x) / 2,
                          int4_min_y + (int4_max_y - int4_min_y) / 2};
  CPos cell_abstract{cell_desired};
  std::int64_t int8_distance{std::numeric_limits<std::int64_t>::max()};
  for (const CPos& cell : vec_cells) {
    const std::int64_t int8_dx{static_cast<std::int64_t>(cell.int4_x) - cell_desired.int4_x};
    const std::int64_t int8_dy{static_cast<std::int64_t>(cell.int4_y) - cell_desired.int4_y};
    const std::int64_t int8_new{int8_dx * int8_dx + int8_dy * int8_dy};
    if (int8_distance > int8_new ||
        (int8_distance == int8_new && cell_abstract.int4_x > cell.int4_x) ||
        (int8_distance == int8_new && cell_abstract.int4_x == cell.int4_x &&
         cell_abstract.int4_y > cell.int4_y)) {
      int8_distance = int8_new;
      cell_abstract = cell;
    }
  }
  return cell_abstract;
}

HpfGridInfo build_grid(HpfGraph& graph, std::int32_t int4_grid_x, std::int32_t int4_grid_y) {
  HpfGridInfo grid;
  const GridBounds bounds{grid_bounds_for(graph, int4_grid_x, int4_grid_y)};
  std::set<CellKey> set_accessible;
  for (std::int32_t int4_y{bounds.int4_top}; int4_y < bounds.int4_bottom; ++int4_y)
    for (std::int32_t int4_x{bounds.int4_left}; int4_x < bounds.int4_right; ++int4_x)
      if (cell_is_accessible(graph, CPos{int4_x, int4_y}))
        set_accessible.insert(CellKey{int4_x, int4_y});

  bool b_has_populated{false};
  while (!set_accessible.empty()) {
    const CellKey key_source{*set_accessible.begin()};
    const CPos cell_source{key_source.first, key_source.second};
    LocalSearch search;
    search.ptr_map = graph.ptr_map;
    search.ptr_graph = &graph;
    search.int4_weight = 100;
    search.cell_goal = cell_source;
    search.cell_heuristic_dst = cell_source;
    search.b_bounded = true;
    search.bounds_grid = bounds;
    search.int4_cell_cost = graph.int4_min_cost;
    search.int4_diag_cost = graph.int4_diag_cost;
    search.init();
    search.add_initial_cell(cell_source);
    const std::vector<CPos> vec_region{search.expand_all()};
    const CPos cell_abstract{abstract_cell_for_local_cells(vec_region)};
    for (const CPos& cell : vec_region) set_accessible.erase(key_of(cell));
    if (!b_has_populated && set_accessible.empty()) {
      grid.b_has_single = true;
      grid.cell_single = cell_abstract;
    } else {
      b_has_populated = true;
      for (const CPos& cell : vec_region) grid.map_local_to_abstract[key_of(cell)] = cell_abstract;
    }
  }
  return grid;
}

void collect_abstract_edge(const HpfGraph& graph, std::vector<std::pair<CPos, CPos>>& vec_edges,
                           CPos cell, CPos cell_candidate) {
  if (movement_cost_for_cell(*graph.ptr_map, cell_candidate) == kCostUnreachable) return;
  const HpfGridInfo& grid_src{graph.vec_grids[static_cast<std::size_t>(
      cell.int4_y / kGridSize * graph.int4_grid_xs + cell.int4_x / kGridSize)]};
  const HpfGridInfo& grid_dst{graph.vec_grids[static_cast<std::size_t>(
      cell_candidate.int4_y / kGridSize * graph.int4_grid_xs +
      cell_candidate.int4_x / kGridSize)]};
  const std::optional<CPos> opt_src{grid_abstract_cell(grid_src, cell, false, graph)};
  if (!opt_src.has_value()) return;
  const std::optional<CPos> opt_dst{grid_abstract_cell(grid_dst, cell_candidate, false, graph)};
  if (!opt_dst.has_value()) return;
  for (const auto& edge : vec_edges)
    if (edge.first == *opt_src && edge.second == *opt_dst) return;
  vec_edges.push_back({*opt_src, *opt_dst});
}

void add_abstract_edges(const HpfGraph& graph, std::vector<std::pair<CPos, CPos>>& vec_edges,
                        std::int32_t int4_x_increment, std::int32_t int4_y_increment,
                        CellVec vec_adjacent, std::int32_t int4_offset_x,
                        std::int32_t int4_offset_y, std::int32_t int4_grid_x,
                        std::int32_t int4_grid_y) {
  const std::int32_t int4_start_y{int4_grid_y + int4_offset_y};
  const std::int32_t int4_start_x{int4_grid_x + int4_offset_x};
  for (std::int32_t int4_y{int4_start_y}; int4_y < int4_start_y + kGridSize;
       int4_y += int4_y_increment)
    for (std::int32_t int4_x{int4_start_x}; int4_x < int4_start_x + kGridSize;
         int4_x += int4_x_increment) {
      const CPos cell{int4_x, int4_y};
      if (!cell_is_accessible(graph, cell)) continue;
      const CPos cell_adjacent{int4_x + vec_adjacent.int4_x, int4_y + vec_adjacent.int4_y};
      for (std::int32_t int4_i{-1}; int4_i <= 1; ++int4_i) {
        const CPos cell_candidate{cell_adjacent.int4_x + int4_i * vec_adjacent.int4_y,
                                  cell_adjacent.int4_y + int4_i * vec_adjacent.int4_x};
        collect_abstract_edge(graph, vec_edges, cell, cell_candidate);
      }
    }
}

std::vector<std::pair<CPos, CPos>> abstract_edges_for_grid(const HpfGraph& graph,
                                                           std::int32_t int4_grid_x,
                                                           std::int32_t int4_grid_y) {
  std::vector<std::pair<CPos, CPos>> vec_edges;
  add_abstract_edges(graph, vec_edges, 1, kGridSize, CellVec{0, -1}, 0, 0, int4_grid_x,
                     int4_grid_y);
  add_abstract_edges(graph, vec_edges, kGridSize, 1, CellVec{-1, 0}, 0, 0, int4_grid_x,
                     int4_grid_y);
  add_abstract_edges(graph, vec_edges, 1, kGridSize, CellVec{0, 1}, 0, kGridSize - 1,
                     int4_grid_x, int4_grid_y);
  add_abstract_edges(graph, vec_edges, kGridSize, 1, CellVec{1, 0}, kGridSize - 1, 0,
                     int4_grid_x, int4_grid_y);
  return vec_edges;
}

void insert_connections(ChangedEdges& edges, CPos cell_local, CPos cell_abstract) {
  const HpfGraph& graph{*edges.ptr_graph};
  const std::vector<GraphConnection> vec_abstract{
      edges.abstract_edges_at(cell_abstract) != nullptr
          ? *edges.abstract_edges_at(cell_abstract)
          : std::vector<GraphConnection>{}};

  std::vector<GraphConnection> vec_local;
  for (const GraphConnection& conn : vec_abstract)
    vec_local.push_back(
        GraphConnection{conn.cell, estimator_between(graph, cell_local, conn.cell)});
  vec_local.push_back(
      GraphConnection{cell_abstract, estimator_between(graph, cell_local, cell_abstract)});
  edges.map_changed[key_of(cell_local)] = std::move(vec_local);

  std::vector<GraphConnection> vec_abstract_changed;
  const auto it_abstract{edges.map_changed.find(key_of(cell_abstract))};
  if (it_abstract != edges.map_changed.end())
    vec_abstract_changed = it_abstract->second;
  else
    vec_abstract_changed = vec_abstract;
  vec_abstract_changed.push_back(
      GraphConnection{cell_local, estimator_between(graph, cell_abstract, cell_local)});
  edges.map_changed[key_of(cell_abstract)] = std::move(vec_abstract_changed);

  for (const GraphConnection& conn : vec_abstract) {
    std::vector<GraphConnection> vec_conn;
    const auto it_conn{edges.map_changed.find(key_of(conn.cell))};
    if (it_conn != edges.map_changed.end())
      vec_conn = it_conn->second;
    else if (const std::vector<GraphConnection>* ptr_conn{edges.abstract_edges_at(conn.cell)})
      vec_conn = *ptr_conn;
    vec_conn.push_back(
        GraphConnection{cell_local, estimator_between(graph, conn.cell, cell_local)});
    edges.map_changed[key_of(conn.cell)] = std::move(vec_conn);
  }
}

struct LocalEdge {
  CPos cell_local;
  CPos cell_abstract;
};

std::optional<LocalEdge> edge_from_local_to_abstract(CPos cell_local, CPos cell_abstract) {
  if (cell_local == cell_abstract) return std::nullopt;
  return LocalEdge{cell_local, cell_abstract};
}

std::vector<CPos> make_bidi_path(LocalSearch& search_first, LocalSearch& search_second,
                                 CPos cell_confluence) {
  std::vector<CPos> vec_path;
  CPos cell_node{cell_confluence};
  CPos cell_previous{search_first.info(cell_node).cell_prev};
  while (!(cell_previous == cell_node)) {
    vec_path.push_back(cell_node);
    cell_node = cell_previous;
    cell_previous = search_first.info(cell_node).cell_prev;
  }
  vec_path.push_back(cell_node);
  std::reverse(vec_path.begin(), vec_path.end());

  cell_node = cell_confluence;
  cell_previous = search_second.info(cell_node).cell_prev;
  while (!(cell_previous == cell_node)) {
    cell_node = cell_previous;
    cell_previous = search_second.info(cell_node).cell_prev;
    vec_path.push_back(cell_node);
  }
  return vec_path;
}

std::vector<CPos> find_bidi_path(LocalSearch& search_first, LocalSearch& search_second) {
  while (search_first.can_expand() && search_second.can_expand()) {
    const CPos cell_p{search_first.expand()};
    if (search_second.info(cell_p).u1_status == kStatusClosed)
      return make_bidi_path(search_first, search_second, cell_p);
    const CPos cell_q{search_second.expand()};
    if (search_first.info(cell_q).u1_status == kStatusClosed)
      return make_bidi_path(search_first, search_second, cell_q);
  }
  return {};
}

std::int64_t distance_squared(CPos cell_a, CPos cell_b) {
  const std::int64_t int8_dx{static_cast<std::int64_t>(cell_a.int4_x) - cell_b.int4_x};
  const std::int64_t int8_dy{static_cast<std::int64_t>(cell_a.int4_y) - cell_b.int4_y};
  return int8_dx * int8_dx + int8_dy * int8_dy;
}

std::vector<CPos> find_path_multi_source(const HpfGraph& graph, CPos cell_source,
                                         CPos cell_target, std::int32_t int4_weight,
                                         CPos cell_target_abstract);

std::vector<CPos> find_path_single_source(const HpfGraph& graph, CPos cell_source,
                                          CPos cell_target, std::int32_t int4_weight) {
  if (distance_squared(cell_source, cell_target) <
      static_cast<std::int64_t>(kGridSize) * kGridSize * kCloseGridDistance * kCloseGridDistance) {
    LocalSearch search_near;
    search_near.ptr_map = graph.ptr_map;
    search_near.ptr_graph = &graph;
    search_near.int4_weight = 100;
    search_near.b_lane_bias = true;
    search_near.cell_goal = cell_target;
    search_near.cell_heuristic_dst = cell_target;
    search_near.b_bounded = true;
    search_near.bounds_grid = GridBounds{
        std::min(cell_source.int4_x, cell_target.int4_x) - kGridSize / 2,
        std::min(cell_source.int4_y, cell_target.int4_y) - kGridSize / 2,
        std::max(cell_source.int4_x, cell_target.int4_x) + kGridSize / 2,
        std::max(cell_source.int4_y, cell_target.int4_y) + kGridSize / 2};
    search_near.int4_cell_cost = graph.int4_min_cost;
    search_near.int4_diag_cost = graph.int4_diag_cost;
    search_near.init();
    search_near.add_initial_cell(cell_source);
    const std::vector<CPos> vec_path{search_near.find_path()};
    if (!vec_path.empty()) return vec_path;
  }

  const std::optional<CPos> opt_target_abstract{abstract_cell_for(graph, cell_target)};
  if (!opt_target_abstract.has_value()) return {};
  const std::optional<CPos> opt_source_abstract{abstract_cell_for(graph, cell_source)};
  if (!opt_source_abstract.has_value())
    return find_path_multi_source(graph, cell_source, cell_target, int4_weight,
                                  *opt_target_abstract);

  const auto it_domain_target{graph.map_domains.find(key_of(*opt_target_abstract))};
  const auto it_domain_source{graph.map_domains.find(key_of(*opt_source_abstract))};
  if (it_domain_target == graph.map_domains.end() || it_domain_source == graph.map_domains.end())
    return {};
  if (it_domain_target->second != it_domain_source->second) return {};

  ChangedEdges edges;
  edges.ptr_graph = &graph;
  const std::optional<LocalEdge> opt_source_edge{
      edge_from_local_to_abstract(cell_source, *opt_source_abstract)};
  const std::optional<LocalEdge> opt_target_edge{
      edge_from_local_to_abstract(cell_target, *opt_target_abstract)};
  if (opt_source_edge.has_value())
    insert_connections(edges, opt_source_edge->cell_local, opt_source_edge->cell_abstract);
  if (opt_target_edge.has_value())
    insert_connections(edges, opt_target_edge->cell_local, opt_target_edge->cell_abstract);

  AbstractSearch search_forward;
  search_forward.ptr_graph = &graph;
  search_forward.ptr_edges = &edges;
  search_forward.cell_heuristic_dst = cell_target;
  search_forward.cell_predicate_goal = cell_target;
  search_forward.init();
  search_forward.add_initial_cell(cell_source);
  if (!search_forward.expand_to_target()) return {};

  AbstractSearch search_reverse;
  search_reverse.ptr_graph = &graph;
  search_reverse.ptr_edges = &edges;
  search_reverse.cell_heuristic_dst = cell_source;
  search_reverse.cell_predicate_goal = cell_source;
  search_reverse.init();
  search_reverse.add_initial_cell(cell_target);
  search_reverse.expand_to_target();

  AbstractHeuristic heuristic_from_src;
  heuristic_from_src.ptr_abstract = &search_reverse;
  heuristic_from_src.ptr_graph = &graph;

  AbstractHeuristic heuristic_from_dest;
  heuristic_from_dest.ptr_abstract = &search_forward;
  heuristic_from_dest.ptr_graph = &graph;

  LocalSearch search_from_src;
  search_from_src.ptr_map = graph.ptr_map;
  search_from_src.ptr_graph = &graph;
  search_from_src.int4_weight = int4_weight;
  search_from_src.b_lane_bias = true;
  search_from_src.cell_goal = cell_target;
  search_from_src.cell_heuristic_dst = cell_target;
  search_from_src.ptr_custom_heuristic = &heuristic_from_src;
  search_from_src.int4_cell_cost = graph.int4_min_cost;
  search_from_src.int4_diag_cost = graph.int4_diag_cost;
  search_from_src.init();
  search_from_src.add_initial_cell(cell_source);

  LocalSearch search_from_dest;
  search_from_dest.ptr_map = graph.ptr_map;
  search_from_dest.ptr_graph = &graph;
  search_from_dest.int4_weight = int4_weight;
  search_from_dest.b_lane_bias = true;
  search_from_dest.b_reverse = true;
  search_from_dest.cell_goal = cell_source;
  search_from_dest.cell_heuristic_dst = cell_source;
  search_from_dest.ptr_custom_heuristic = &heuristic_from_dest;
  search_from_dest.int4_cell_cost = graph.int4_min_cost;
  search_from_dest.int4_diag_cost = graph.int4_diag_cost;
  search_from_dest.init();
  search_from_dest.add_initial_cell(cell_target);

  return find_bidi_path(search_from_dest, search_from_src);
}

std::vector<CPos> find_path_multi_source(const HpfGraph& graph, CPos cell_source,
                                         CPos cell_target, std::int32_t int4_weight,
                                         CPos cell_target_abstract) {
  const auto it_domain_target{graph.map_domains.find(key_of(cell_target_abstract))};
  if (it_domain_target == graph.map_domains.end()) return {};

  std::vector<CPos> vec_unpathable;
  std::vector<LocalEdge> vec_source_edges;
  bool b_has_source{false};
  for (const CellVec& vec_dir : kDirections8) {
    const CPos cell_adjacent{cell_source.int4_x + vec_dir.int4_x,
                             cell_source.int4_y + vec_dir.int4_y};
    if (movement_cost_for_cell(*graph.ptr_map, cell_adjacent) == kCostUnreachable) continue;
    const std::optional<CPos> opt_adjacent{abstract_cell_for(graph, cell_adjacent)};
    if (!opt_adjacent.has_value()) continue;
    const auto it_domain{graph.map_domains.find(key_of(*opt_adjacent))};
    if (it_domain == graph.map_domains.end() || it_domain->second != it_domain_target->second) {
      vec_unpathable.push_back(cell_adjacent);
      continue;
    }
    b_has_source = true;
    const std::optional<LocalEdge> opt_edge{
        edge_from_local_to_abstract(cell_adjacent, *opt_adjacent)};
    if (opt_edge.has_value()) vec_source_edges.push_back(*opt_edge);
  }
  if (!b_has_source) return {};

  ChangedEdges edges;
  edges.ptr_graph = &graph;
  for (const LocalEdge& edge : vec_source_edges)
    insert_connections(edges, edge.cell_local, edge.cell_abstract);
  const std::optional<LocalEdge> opt_target_edge{
      edge_from_local_to_abstract(cell_target, cell_target_abstract)};
  if (opt_target_edge.has_value())
    insert_connections(edges, opt_target_edge->cell_local, opt_target_edge->cell_abstract);

  AbstractSearch search_reverse;
  search_reverse.ptr_graph = &graph;
  search_reverse.ptr_edges = &edges;
  search_reverse.cell_heuristic_dst = cell_target;
  search_reverse.cell_predicate_goal = cell_target;
  search_reverse.init();
  search_reverse.add_initial_cell(cell_target);

  AbstractHeuristic heuristic_custom;
  heuristic_custom.ptr_abstract = &search_reverse;
  heuristic_custom.ptr_graph = &graph;
  heuristic_custom.vec_unpathable = std::move(vec_unpathable);
  heuristic_custom.b_has_known_source = true;
  heuristic_custom.cell_known_source = cell_source;

  LocalSearch search;
  search.ptr_map = graph.ptr_map;
  search.ptr_graph = &graph;
  search.int4_weight = int4_weight;
  search.b_lane_bias = true;
  search.cell_goal = cell_target;
  search.cell_heuristic_dst = cell_target;
  search.ptr_custom_heuristic = &heuristic_custom;
  search.int4_cell_cost = graph.int4_min_cost;
  search.int4_diag_cost = graph.int4_diag_cost;
  search.init();
  search.add_initial_cell(cell_source);
  return search.find_path();
}

}

HpfGraph hpf_build(const LocomotorMap& map) {
  HpfGraph graph;
  graph.ptr_map = &map;
  graph.int4_min_cost = map.int4_min_cost;
  graph.int4_diag_cost = multiply_by_sqrt_two(map.int4_min_cost);

  const std::int32_t int4_away_x{map.int4_width % kGridSize == 0
                                     ? map.int4_width / kGridSize
                                     : map.int4_width / kGridSize + 1};
  const std::int32_t int4_away_y{map.int4_height % kGridSize == 0
                                     ? map.int4_height / kGridSize
                                     : map.int4_height / kGridSize + 1};
  graph.int4_grid_xs = int4_away_x;
  graph.int4_grid_ys = int4_away_y;

  graph.vec_grids.resize(static_cast<std::size_t>(int4_away_x) * int4_away_y);
  for (std::int32_t int4_grid_x{0}; int4_grid_x < map.int4_width; int4_grid_x += kGridSize)
    for (std::int32_t int4_grid_y{0}; int4_grid_y < map.int4_height; int4_grid_y += kGridSize)
      graph.vec_grids[static_cast<std::size_t>(int4_grid_y / kGridSize * int4_away_x +
                                               int4_grid_x / kGridSize)] =
          build_grid(graph, int4_grid_x, int4_grid_y);

  for (std::int32_t int4_grid_x{0}; int4_grid_x < map.int4_width; int4_grid_x += kGridSize)
    for (std::int32_t int4_grid_y{0}; int4_grid_y < map.int4_height; int4_grid_y += kGridSize)
      for (const auto& [cell_src, cell_dst] :
           abstract_edges_for_grid(graph, int4_grid_x, int4_grid_y)) {
        std::vector<GraphConnection>& vec_connections{
            graph.map_abstract_edges[key_of(cell_src)]};
        vec_connections.push_back(
            GraphConnection{cell_dst, estimator_between(graph, cell_src, cell_dst)});
      }

  std::set<CellKey> set_abstract;
  for (const HpfGridInfo& grid : graph.vec_grids) {
    if (grid.b_has_single) set_abstract.insert(key_of(grid.cell_single));
    for (const auto& [key_local, cell_abstract] : grid.map_local_to_abstract)
      set_abstract.insert(key_of(cell_abstract));
  }

  ChangedEdges edges;
  edges.ptr_graph = &graph;
  std::uint32_t u4_domain{0};
  while (!set_abstract.empty()) {
    const CPos cell_seed{set_abstract.begin()->first, set_abstract.begin()->second};
    AbstractSearch search;
    search.ptr_graph = &graph;
    search.ptr_edges = &edges;
    search.cell_heuristic_dst = cell_seed;
    search.cell_predicate_goal = cell_seed;
    search.init();
    search.add_initial_cell(cell_seed);
    const std::vector<CPos> vec_domain{search.expand_all()};
    for (const CPos& cell : vec_domain) {
      graph.map_domains[key_of(cell)] = u4_domain;
      set_abstract.erase(key_of(cell));
    }
    ++u4_domain;
  }

  return graph;
}

std::vector<CPos> hpf_find_path_to_target_cell(const HpfGraph& graph, CPos cell_from,
                                               CPos cell_target, std::int32_t int4_weight) {
  const LocomotorMap& map{*graph.ptr_map};
  if (!map_contains(map, cell_target) ||
      movement_cost_for_cell(map, cell_target) == kCostUnreachable)
    return {};
  if (distance_squared(cell_from, cell_target) < 3) return {cell_target, cell_from};
  return find_path_single_source(graph, cell_from, cell_target, std::max(100, int4_weight));
}

}
