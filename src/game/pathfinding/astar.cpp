#include "game/pathfinding/astar.hpp"

namespace ora::game::pathfinding {

namespace {

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

constexpr std::int32_t kSqrtTwoNum{46341};
constexpr std::int32_t kSqrtTwoDen{32768};
constexpr std::int32_t kLaneBiasCost{1};

std::int32_t multiply_by_sqrt_two(std::int32_t int4_number) {
  return int4_number * kSqrtTwoNum / kSqrtTwoDen;
}

struct SearchState {
  const LocomotorMap* ptr_map;
  CPos cell_target;
  std::int32_t int4_weight;
  bool b_lane_bias;
  bool b_lane_bias_reverse{};  // DensePathGraph inReverse：lane 偏置奇偶翻转（x+1/y+1）
  std::int32_t int4_cell_cost;
  std::int32_t int4_diag_cost;
  std::vector<CellInfoNode> vec_cells;
  LevelOrderQueue queue;
};

std::size_t cell_index(const SearchState& state, CPos cell) {
  return static_cast<std::size_t>(cell.int4_x) * state.ptr_map->int4_height + cell.int4_y;
}

std::int32_t heuristic_cost(const SearchState& state, CPos cell) {
  const std::int32_t int4_dx{cell.int4_x > state.cell_target.int4_x
                                 ? cell.int4_x - state.cell_target.int4_x
                                 : state.cell_target.int4_x - cell.int4_x};
  const std::int32_t int4_dy{cell.int4_y > state.cell_target.int4_y
                                 ? cell.int4_y - state.cell_target.int4_y
                                 : state.cell_target.int4_y - cell.int4_y};
  const std::int32_t int4_diag{int4_dx < int4_dy ? int4_dx : int4_dy};
  const std::int32_t int4_straight{int4_dx + int4_dy};
  return state.int4_cell_cost * int4_straight +
         (state.int4_diag_cost - 2 * state.int4_cell_cost) * int4_diag;
}

std::int32_t path_cost_to_node(SearchState& state, CPos cell_dest, CellVec vec_dir) {
  const std::int16_t int2_cost{movement_cost_for_cell(*state.ptr_map, cell_dest)};
  if (int2_cost == kCostUnreachable) return std::numeric_limits<std::int32_t>::max();

  std::int32_t int4_cost{vec_dir.int4_x * vec_dir.int4_y != 0 ? multiply_by_sqrt_two(int2_cost)
                                                              : int2_cost};

  if (state.b_lane_bias) {
    // DensePathGraph.GetConnections（:215-216）：inReverse 时奇偶翻转
    const std::int32_t int4_ux{(cell_dest.int4_x + (state.b_lane_bias_reverse ? 1 : 0)) & 1};
    const std::int32_t int4_uy{(cell_dest.int4_y + (state.b_lane_bias_reverse ? 1 : 0)) & 1};
    if ((int4_ux == 0 && vec_dir.int4_y < 0) || (int4_ux == 1 && vec_dir.int4_y > 0))
      int4_cost += kLaneBiasCost;
    if ((int4_uy == 0 && vec_dir.int4_x < 0) || (int4_uy == 1 && vec_dir.int4_x > 0))
      int4_cost += kLaneBiasCost;
  }

  return int4_cost;
}

NeighborSet neighbor_set(const SearchState& state, CPos cell_position) {
  const CellInfoNode& info{state.vec_cells[cell_index(state, cell_position)]};
  const std::int32_t int4_dx{cell_position.int4_x - info.cell_prev.int4_x};
  const std::int32_t int4_dy{cell_position.int4_y - info.cell_prev.int4_y};
  const std::int32_t int4_index{int4_dy * 3 + int4_dx + 4};
  if (int4_index == 4) return NeighborSet{kDirections8, 8};
  return NeighborSet{kDirectedNeighbors[int4_index], kDirectedNeighborsCount[int4_index]};
}

std::vector<GraphConnection> get_connections(SearchState& state, CPos cell_position) {
  const NeighborSet set_dirs{neighbor_set(state, cell_position)};
  std::vector<GraphConnection> vec_connections;
  vec_connections.reserve(set_dirs.sz_count);
  for (std::size_t idx{0}; idx < set_dirs.sz_count; ++idx) {
    const CellVec vec_dir{set_dirs.ptr_dirs[idx]};
    CPos cell_neighbor{cell_position.int4_x + vec_dir.int4_x, cell_position.int4_y + vec_dir.int4_y};
    const std::int32_t int4_cost{path_cost_to_node(state, cell_neighbor, vec_dir)};
    if (int4_cost != std::numeric_limits<std::int32_t>::max() &&
        state.vec_cells[cell_index(state, cell_neighbor)].u1_status != kStatusClosed)
      vec_connections.push_back(GraphConnection{cell_neighbor, int4_cost});
  }
  return vec_connections;
}

void add_initial_cell(SearchState& state, CPos cell) {
  const std::int32_t int4_heuristic{heuristic_cost(state, cell)};
  const std::int32_t int4_estimated{int4_heuristic * state.int4_weight / 100};
  CellInfoNode& info{state.vec_cells[cell_index(state, cell)]};
  info.u1_status = kStatusOpen;
  info.int4_cost_so_far = 0;
  info.int4_est_total = int4_estimated;
  info.cell_prev = cell;
  queue_add(state.queue, GraphConnection{cell, int4_estimated});
}

bool can_expand(SearchState& state) {
  while (true) {
    if (queue_empty(state.queue)) return false;
    if (state.vec_cells[cell_index(state, state.queue.vec_items[0].cell)].u1_status == kStatusClosed) {
      queue_pop(state.queue);
      continue;
    }
    return true;
  }
}

CPos expand(SearchState& state) {
  const CPos cell_current{queue_pop(state.queue).cell};
  CellInfoNode& info_current{state.vec_cells[cell_index(state, cell_current)]};
  const std::int32_t int4_cost_so_far{info_current.int4_cost_so_far};
  info_current.u1_status = kStatusClosed;

  for (const GraphConnection& conn : get_connections(state, cell_current)) {
    const std::int32_t int4_cost_to{int4_cost_so_far + conn.int4_cost};
    CellInfoNode& info{state.vec_cells[cell_index(state, conn.cell)]};

    if (info.u1_status == kStatusClosed ||
        (info.u1_status == kStatusOpen && int4_cost_to >= info.int4_cost_so_far))
      continue;

    std::int32_t int4_remaining{};
    if (info.u1_status == kStatusOpen) {
      int4_remaining = info.int4_est_total - info.int4_cost_so_far;
    } else {
      int4_remaining = heuristic_cost(state, conn.cell) * state.int4_weight / 100;
    }

    info.u1_status = kStatusOpen;
    info.int4_cost_so_far = int4_cost_to;
    info.int4_est_total = int4_cost_to + int4_remaining;
    info.cell_prev = cell_current;
    queue_add(state.queue, GraphConnection{conn.cell, int4_cost_to + int4_remaining});
  }

  return cell_current;
}

}  // namespace

std::vector<CPos> astar_to_target_cell(const LocomotorMap& map, CPos cell_from, CPos cell_target,
                                       std::int32_t int4_weight, bool b_lane_bias) {
  SearchState state;
  state.ptr_map = &map;
  state.cell_target = cell_target;
  state.int4_weight = int4_weight;
  state.b_lane_bias = b_lane_bias;
  state.int4_cell_cost = map.int4_min_cost;
  state.int4_diag_cost = multiply_by_sqrt_two(map.int4_min_cost);
  state.vec_cells.assign(static_cast<std::size_t>(map.int4_width) * map.int4_height, CellInfoNode{});

  if (map_contains(map, cell_from)) add_initial_cell(state, cell_from);

  while (can_expand(state)) {
    const CPos cell{expand(state)};
    if (cell.int4_x == cell_target.int4_x && cell.int4_y == cell_target.int4_y) {
      std::vector<CPos> vec_path;
      CPos cell_node{cell};
      while (true) {
        vec_path.push_back(cell_node);
        const CellInfoNode& info{state.vec_cells[cell_index(state, cell_node)]};
        if (info.cell_prev.int4_x == cell_node.int4_x && info.cell_prev.int4_y == cell_node.int4_y)
          break;
        cell_node = info.cell_prev;
      }
      return vec_path;
    }
  }

  return {};
}

namespace {

// Grid.TilesByDistance（MapGrid.cs）：圈 = ISqrt(i²+j², Ceiling)，圈序 (len², hash, x, y)
struct AnnulusOffset {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_len_sq{};
};

const std::vector<std::vector<AnnulusOffset>>& tiles_by_distance() {
  constexpr std::int32_t kMaxRange{50};  // MapGrid.MaximumTileSearchRange
  static const std::vector<std::vector<AnnulusOffset>> vec_table{[] {
    std::vector<std::vector<AnnulusOffset>> vec_dist(static_cast<std::size_t>(kMaxRange) + 1);
    for (std::int32_t int4_j{-kMaxRange}; int4_j <= kMaxRange; ++int4_j)
      for (std::int32_t int4_i{-kMaxRange}; int4_i <= kMaxRange; ++int4_i) {
        const std::int32_t int4_sq{int4_i * int4_i + int4_j * int4_j};
        if (kMaxRange * kMaxRange < int4_sq) continue;
        // Exts.ISqrt(Ceiling)：最小 s 使 s² >= v
        std::int32_t int4_root{0};
        while (int4_root * int4_root < int4_sq) ++int4_root;
        vec_dist[static_cast<std::size_t>(int4_root)].push_back(
            AnnulusOffset{int4_i, int4_j, int4_sq});
      }
    for (std::vector<AnnulusOffset>& vec_ring : vec_dist)
      std::sort(vec_ring.begin(), vec_ring.end(), [](const AnnulusOffset& a,
                                                     const AnnulusOffset& b) {
        if (a.int4_len_sq != b.int4_len_sq) return a.int4_len_sq < b.int4_len_sq;
        const std::int32_t int4_hash_a{a.int4_x ^ a.int4_y};  // CVec.GetHashCode
        const std::int32_t int4_hash_b{b.int4_x ^ b.int4_y};
        if (int4_hash_a != int4_hash_b) return int4_hash_a < int4_hash_b;
        if (a.int4_x != b.int4_x) return a.int4_x < b.int4_x;
        return a.int4_y < b.int4_y;
      });
    return vec_dist;
  }()};
  return vec_table;
}

}  // namespace

std::vector<CPos> find_tiles_in_annulus(const LocomotorMap& map, CPos cell_center,
                                        std::int32_t int4_min_range, std::int32_t int4_max_range) {
  std::vector<CPos> vec_out;
  const auto& vec_table{tiles_by_distance()};
  for (std::int32_t int4_ring{int4_min_range}; int4_ring <= int4_max_range; ++int4_ring) {
    if (int4_ring < 0 || static_cast<std::size_t>(int4_ring) >= vec_table.size()) break;
    for (const AnnulusOffset& offset : vec_table[static_cast<std::size_t>(int4_ring)]) {
      const CPos cell{cell_center.int4_x + offset.int4_x, cell_center.int4_y + offset.int4_y};
      if (map_contains(map, cell)) vec_out.push_back(cell);
    }
  }
  return vec_out;
}

std::vector<CPos> astar_to_target_cell_multi(const LocomotorMap& map,
                                             const std::vector<CPos>& vec_cells_from,
                                             CPos cell_target, std::int32_t int4_weight,
                                             bool b_lane_bias, bool b_lane_bias_reverse) {
  SearchState state;
  state.ptr_map = &map;
  state.cell_target = cell_target;
  state.int4_weight = int4_weight;
  state.b_lane_bias = b_lane_bias;
  state.b_lane_bias_reverse = b_lane_bias_reverse;
  state.int4_cell_cost = map.int4_min_cost;
  state.int4_diag_cost = multiply_by_sqrt_two(map.int4_min_cost);
  state.vec_cells.assign(static_cast<std::size_t>(map.int4_width) * map.int4_height, CellInfoNode{});

  // AddInitialCells（PathSearch.cs:98-110）：多源各 cost 0 加入（可达过滤在调用方）
  for (const CPos cell : vec_cells_from)
    if (map_contains(map, cell)) add_initial_cell(state, cell);

  while (can_expand(state)) {
    const CPos cell{expand(state)};
    if (cell.int4_x == cell_target.int4_x && cell.int4_y == cell_target.int4_y) {
      std::vector<CPos> vec_path;
      CPos cell_node{cell};
      while (true) {
        vec_path.push_back(cell_node);
        const CellInfoNode& info{state.vec_cells[cell_index(state, cell_node)]};
        if (info.cell_prev.int4_x == cell_node.int4_x && info.cell_prev.int4_y == cell_node.int4_y)
          break;
        cell_node = info.cell_prev;
      }
      return vec_path;  // target→source（上游 ToTargetCell 返回序）
    }
  }

  return {};
}

}  // namespace ora::game::pathfinding
