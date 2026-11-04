#pragma once

import std;

#include "game/pathfinding/astar.hpp"
#include "game/pathfinding/path_graph.hpp"
#include "game/pathfinding/wangle.hpp"

namespace ora::game::pathfinding {

enum class PartKind : std::uint8_t {
  kNone = 0,
  kTurn,
  kFirstHalf,
  kSecondHalf,
};

struct MovePartState {
  PartKind kind{PartKind::kNone};
  WPos pos_from{};
  WPos pos_to{};
  std::int32_t int4_from_facing{};
  std::int32_t int4_to_facing{};
  std::int32_t int4_progress{};
  std::int32_t int4_distance{};
  bool b_arc{};
  WPos pos_arc_center{};
  std::int32_t int4_arc_from_length{};
  std::int32_t int4_arc_to_length{};
  std::int32_t int4_arc_from_angle{};
  std::int32_t int4_arc_to_angle{};
  std::int32_t int4_desired_facing{};
};

struct MoveSimState {
  bool b_has_activity{};
  bool b_first_run_done{};
  bool b_path_preset{};  // 外部预置路径（FindPathToTargetCells 面）——首 tick 跳过内部寻路
  bool b_already_at_destination{};
  bool b_had_no_path{};
  CPos cell_from{};
  CPos cell_to{};
  std::int32_t int4_facing{};
  WPos pos_center{};
  std::vector<CPos> vec_path;
  CPos cell_destination{};
  bool b_has_destination{};
  std::int32_t int4_carryover{};
  std::int32_t int4_last_part_tick{};
  MovePartState part;
};

struct MoveSimConfig {
  const LocomotorMap* ptr_map{};
  std::int32_t int4_speed{};
  std::int32_t int4_turn_speed{512};
  std::int32_t int4_heuristic_weight{125};
};

void move_queue(MoveSimState& st, CPos cell_destination);
void move_sim_tick(MoveSimState& st, const MoveSimConfig& cfg, std::int32_t int4_world_tick);

[[nodiscard]] WPos center_of_cell(CPos cell);
[[nodiscard]] WPos between_cells(CPos cell_a, CPos cell_b);
[[nodiscard]] std::int32_t facing_between(CPos cell, CPos cell_towards,
                                          std::int32_t int4_fallback);

}  // namespace ora::game::pathfinding
