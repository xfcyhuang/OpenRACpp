module ora.game.pathfinding;

import std;
import ora.game;
import ora.game.miniyaml;

namespace ora::game::pathfinding {

namespace {

std::int32_t movement_speed_for_cell(const MoveSimConfig& cfg, CPos cell) {
  const std::int32_t int4_terrain{terrain_speed_for_cell(*cfg.ptr_map, cell)};
  return static_cast<std::int32_t>(static_cast<std::int64_t>(cfg.int4_speed) * int4_terrain /
                                   cfg.int4_speed_den);
}

CPos pop_path(MoveSimState& st) {
  const CPos cell_next{st.vec_path.back()};
  st.vec_path.pop_back();
  return cell_next;
}

void set_location(MoveSimState& st, CPos cell_from, CPos cell_to) {
  st.cell_from = cell_from;
  st.cell_to = cell_to;
}

std::int32_t part_distance(WPos pos_from, WPos pos_to) {
  const std::int64_t int8_dx{pos_to.int4_x - pos_from.int4_x};
  const std::int64_t int8_dy{pos_to.int4_y - pos_from.int4_y};
  const std::int64_t int8_dz{pos_to.int4_z - pos_from.int4_z};
  return static_cast<std::int32_t>(isqrt64(int8_dx * int8_dx + int8_dy * int8_dy + int8_dz * int8_dz));
}

MovePartState make_move_part(WPos pos_from, WPos pos_to, std::int32_t int4_from_facing,
                             std::int32_t int4_to_facing, std::int32_t int4_carryover,
                             PartKind kind, bool b_should_arc) {
  MovePartState part;
  part.kind = kind;
  part.pos_from = pos_from;
  part.pos_to = pos_to;
  part.int4_from_facing = int4_from_facing;
  part.int4_to_facing = int4_to_facing;
  part.int4_progress = int4_carryover;
  part.int4_distance = part_distance(pos_from, pos_to);

  if (b_should_arc) {
    const WVec vec_u{wvec_rotate_yaw(WVec{1024, 0, 0}, int4_from_facing)};
    const WVec vec_v{wvec_rotate_yaw(WVec{1024, 0, 0}, int4_to_facing)};
    if (vec_v.int4_x * vec_u.int4_y != vec_v.int4_y * vec_u.int4_x) {
      const WVec vec_w{wvec_sub(pos_from, pos_to)};
      const std::int32_t int4_s{(vec_v.int4_y * vec_w.int4_x - vec_v.int4_x * vec_w.int4_y) * 1024 /
                                (vec_v.int4_x * vec_u.int4_y - vec_v.int4_y * vec_u.int4_x)};
      const std::int32_t int4_cx{pos_from.int4_x + int4_s * vec_u.int4_x / 1024};
      const std::int32_t int4_cy{pos_from.int4_y + int4_s * vec_u.int4_y / 1024};

      part.pos_arc_center = WPos{int4_cx, int4_cy, 0};
      const WVec vec_from{wvec_sub(part.pos_arc_center, pos_from)};
      const WVec vec_to{wvec_sub(part.pos_arc_center, pos_to)};
      part.int4_arc_from_length = wvec_horizontal_length(vec_from);
      part.int4_arc_from_angle = wvec_yaw(vec_from);
      part.int4_arc_to_length = wvec_horizontal_length(vec_to);
      part.int4_arc_to_angle = wvec_yaw(vec_to);
      part.b_arc = true;
    }
  }

  return part;
}

bool first_half_is_turn(const MoveSimState& st, CPos cell_next) {
  const std::int32_t int4_next_facing{facing_between(cell_next, st.cell_to, st.int4_facing)};
  const std::int32_t int4_current_facing{facing_between(st.cell_to, st.cell_from, st.int4_facing)};
  const std::int32_t int4_delta{wangle_wrap(int4_next_facing - int4_current_facing)};
  return int4_delta != 0 && (int4_delta < 384 || int4_delta > 640);
}

void on_complete_first(MoveSimState& st, std::int32_t int4_overrun) {
  if (!st.vec_path.empty()) {
    const CPos cell_next{pop_path(st)};
    if (first_half_is_turn(st, cell_next)) {
      const WPos pos_from{between_cells(st.cell_from, st.cell_to)};
      const WPos pos_to{between_cells(st.cell_to, cell_next)};
      const std::int32_t int4_to_facing{facing_between(st.cell_to, cell_next, st.int4_facing)};
      MovePartState part{make_move_part(pos_from, pos_to, st.int4_facing, int4_to_facing,
                                        int4_overrun, PartKind::kFirstHalf, true)};
      set_location(st, st.cell_to, cell_next);
      st.part = std::move(part);
      return;
    }
    st.vec_path.push_back(cell_next);
  }

  const WPos pos_from{between_cells(st.cell_from, st.cell_to)};
  const WPos pos_to{center_of_cell(st.cell_to)};
  MovePartState part{make_move_part(pos_from, pos_to, st.int4_facing, st.int4_facing,
                                    int4_overrun, PartKind::kSecondHalf, false)};
  set_location(st, st.cell_to, st.cell_to);
  st.part = std::move(part);
}

void on_complete_second(MoveSimState& st, std::int32_t int4_overrun) {
  set_location(st, st.cell_to, st.cell_to);
  st.pos_center = center_of_cell(st.cell_to);
  st.int4_carryover = int4_overrun;
  st.part = MovePartState{};
}

bool move_part_tick(MoveSimState& st, const MoveSimConfig& cfg, std::int32_t int4_world_tick) {
  MovePartState& part{st.part};
  if (part.kind == PartKind::kTurn) {
    if (part.int4_desired_facing == st.int4_facing) {
      st.part = MovePartState{};
      return true;
    }
    st.int4_facing = tick_facing(st.int4_facing, part.int4_desired_facing, cfg.int4_turn_speed);
    return false;
  }

  if (st.int4_last_part_tick < int4_world_tick)
    part.int4_progress += movement_speed_for_cell(cfg, st.cell_to);

  if (part.int4_progress >= part.int4_distance) {
    st.pos_center = part.pos_to;
    st.int4_facing = part.int4_to_facing;
    st.int4_last_part_tick = int4_world_tick;
    const std::int32_t int4_overrun{part.int4_progress - part.int4_distance};
    if (part.kind == PartKind::kFirstHalf)
      on_complete_first(st, int4_overrun);
    else
      on_complete_second(st, int4_overrun);
    return true;
  }

  WPos pos{};
  if (part.b_arc) {
    const std::int32_t int4_angle{wangle_lerp(part.int4_arc_from_angle, part.int4_arc_to_angle,
                                              part.int4_progress, part.int4_distance)};
    const std::int32_t int4_length{int2_lerp(part.int4_arc_from_length, part.int4_arc_to_length,
                                             part.int4_progress, part.int4_distance)};
    const std::int32_t int4_height{int2_lerp(part.pos_from.int4_z, part.pos_to.int4_z,
                                             part.int4_progress, part.int4_distance)};
    const WVec vec_rotated{wvec_rotate_yaw(WVec{0, int4_length, int4_height}, int4_angle)};
    pos = WPos{part.pos_arc_center.int4_x + vec_rotated.int4_x,
               part.pos_arc_center.int4_y + vec_rotated.int4_y,
               part.pos_arc_center.int4_z + vec_rotated.int4_z};
  } else {
    pos = wpos_lerp(part.pos_from, part.pos_to, part.int4_progress, part.int4_distance);
  }
  pos.int4_z = 0;
  st.pos_center = pos;
  st.int4_facing = wangle_lerp(part.int4_from_facing, part.int4_to_facing, part.int4_progress,
                               part.int4_distance);
  return false;
}

bool move_part_chain(MoveSimState& st, const MoveSimConfig& cfg, std::int32_t int4_world_tick) {
  while (st.part.kind != PartKind::kNone) {
    if (!move_part_tick(st, cfg, int4_world_tick)) return false;
  }
  return true;
}

void move_on_first_run(MoveSimState& st, const MoveSimConfig& cfg) {
  if (st.cell_to == st.cell_destination) {
    st.b_already_at_destination = true;
    return;
  }

  if (st.b_path_preset) return;

  std::vector<CPos> vec_path{astar_to_target_cell(*cfg.ptr_map, st.cell_to, st.cell_destination,
                                                  cfg.int4_heuristic_weight, true, cfg.ptr_set_blocked,
                                                  cfg.b_reverse_path)};
  while (!vec_path.empty() && vec_path.back() == st.cell_to)
    vec_path.pop_back();
  st.vec_path = std::move(vec_path);
}

bool move_root_tick(MoveSimState& st, const MoveSimConfig& cfg) {
  if (!st.b_first_run_done) {
    st.b_first_run_done = true;
    move_on_first_run(st, cfg);
  }

  if (st.b_already_at_destination) return true;

  if (st.b_has_destination && st.cell_destination == st.cell_to) return true;

  if (st.vec_path.empty()) {
    st.b_had_no_path = true;
    st.cell_destination = st.cell_to;
    st.b_has_destination = true;
    return false;
  }

  st.cell_destination = st.vec_path.front();
  st.b_has_destination = true;
  const CPos cell_next{pop_path(st)};
  const std::int32_t int4_first_facing{facing_between(st.cell_from, cell_next, st.int4_facing)};

  if (int4_first_facing != st.int4_facing) {
    st.vec_path.push_back(cell_next);
    MovePartState part;
    part.kind = PartKind::kTurn;
    part.int4_desired_facing = int4_first_facing;
    st.part = std::move(part);
    return false;
  }

  set_location(st, st.cell_from, cell_next);
  const WPos pos_from{center_of_cell(st.cell_from)};
  const WPos pos_to{between_cells(st.cell_from, st.cell_to)};
  st.part = make_move_part(pos_from, pos_to, st.int4_facing, int4_first_facing,
                           st.int4_carryover, PartKind::kFirstHalf, false);
  st.int4_carryover = 0;
  return false;
}

}

void move_queue(MoveSimState& st, CPos cell_destination) {
  st.b_has_activity = true;
  st.b_first_run_done = false;
  st.b_path_preset = false;
  st.b_already_at_destination = false;
  st.b_had_no_path = false;
  st.vec_path.clear();
  st.cell_destination = cell_destination;
  st.b_has_destination = true;
  st.int4_carryover = 0;
  st.part = MovePartState{};
}

void move_sim_tick(MoveSimState& st, const MoveSimConfig& cfg, std::int32_t int4_world_tick) {
  if (!st.b_has_activity) return;

  if (st.part.kind != PartKind::kNone) {
    if (!move_part_chain(st, cfg, int4_world_tick)) return;
  }

  if (move_root_tick(st, cfg)) {
    st.b_has_activity = false;
    return;
  }

  if (st.part.kind == PartKind::kNone) return;

  move_part_chain(st, cfg, int4_world_tick);
}

WPos center_of_cell(CPos cell) {
  return WPos{1024 * cell.int4_x + 512, 1024 * cell.int4_y + 512, 0};
}

WPos between_cells(CPos cell_a, CPos cell_b) {
  return wpos_lerp(center_of_cell(cell_a), center_of_cell(cell_b), 1, 2);
}

std::int32_t facing_between(CPos cell, CPos cell_towards, std::int32_t int4_fallback) {
  const WVec vec_delta{wvec_sub(center_of_cell(cell_towards), center_of_cell(cell))};
  if (wvec_horizontal_length_squared(vec_delta) == 0) return int4_fallback;
  return wvec_yaw(vec_delta);
}

}
