#pragma once

import std;

#include "game/pathfinding/path_graph.hpp"

namespace ora::game::pathfinding {

struct CellInfoNode {
  std::uint8_t u1_status{};
  std::int32_t int4_cost_so_far{};
  std::int32_t int4_est_total{};
  CPos cell_prev;
};

[[nodiscard]] std::vector<CPos> astar_to_target_cell(const LocomotorMap& map, CPos cell_from,
                                                     CPos cell_target, std::int32_t int4_weight,
                                                     bool b_lane_bias);

// 多源单目标 A*（PathSearch.ToTargetCell 多 froms 形态；T12/duelD 的
// PathFinder.FindPathToTargetCells swap 路径：annulus 候选=源、原点=目标、inReverse）。
// b_lane_bias_reverse：DensePathGraph 的反向 lane 偏置奇偶翻转（x+1 / y+1）。
// 返回路径 target→source（调用方 reverse 换回正向，与上游语义一致）。
[[nodiscard]] std::vector<CPos> astar_to_target_cell_multi(
    const LocomotorMap& map, const std::vector<CPos>& vec_cells_from, CPos cell_target,
    std::int32_t int4_weight, bool b_lane_bias, bool b_lane_bias_reverse);

// MapGrid.TilesByDistance 序（len² → hash(x^y) → x → y，MapGrid.cs CreateTilesByDistance）
// 的 annulus 偏移表照抄 + Map.Contains 过滤（FindTilesInAnnulus，Map.cs）。
[[nodiscard]] std::vector<CPos> find_tiles_in_annulus(const LocomotorMap& map, CPos cell_center,
                                                      std::int32_t int4_min_range,
                                                      std::int32_t int4_max_range);

}  // namespace ora::game::pathfinding
