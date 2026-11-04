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

}  // namespace ora::game::pathfinding
