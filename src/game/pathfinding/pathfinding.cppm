export module ora.game.pathfinding;

import std;
import ora.game;
import ora.game.miniyaml;

export {
namespace ora::game::pathfinding {

struct CPos {
  std::int32_t int4_x{};
  std::int32_t int4_y{};

  friend bool operator==(const CPos& a, const CPos& b) {
    return a.int4_x == b.int4_x && a.int4_y == b.int4_y;
  }
};

struct GraphConnection {
  CPos cell;
  std::int32_t int4_cost{};
};

struct LevelOrderQueue {
  std::vector<GraphConnection> vec_items;
  std::int32_t int4_level{0};
  std::int32_t int4_index{0};
};

void queue_add(LevelOrderQueue& q, const GraphConnection& item);
bool queue_empty(const LevelOrderQueue& q);
GraphConnection queue_pop(LevelOrderQueue& q);

}

namespace ora::game::pathfinding {

struct WPos {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_z{};
};

struct WVec {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_z{};
};

struct WRot {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_z{};
  std::int32_t int4_w{};
};

struct RotMatrix {
  std::int32_t vec4_m[16]{};
};

constexpr std::int32_t wangle_wrap(std::int32_t int4_angle) { return int4_angle & 1023; }

[[nodiscard]] std::int32_t wangle_lerp(std::int32_t int4_a, std::int32_t int4_b,
                                       std::int32_t int4_mul, std::int32_t int4_div);
[[nodiscard]] std::int32_t wangle_cos(std::int32_t int4_angle);
[[nodiscard]] std::int32_t wangle_sin(std::int32_t int4_angle);
[[nodiscard]] std::int32_t wangle_tan(std::int32_t int4_angle);
[[nodiscard]] std::int32_t wangle_arctan(std::int32_t int4_y, std::int32_t int4_x);
[[nodiscard]] std::int32_t tick_facing(std::int32_t int4_facing, std::int32_t int4_desired,
                                       std::int32_t int4_step);
[[nodiscard]] std::int64_t isqrt64(std::int64_t int8_number);

[[nodiscard]] WVec wvec_sub(WPos pos_a, WPos pos_b);
[[nodiscard]] std::int64_t wvec_length_squared(WVec vec);
[[nodiscard]] std::int64_t wvec_horizontal_length_squared(WVec vec);
[[nodiscard]] std::int32_t wvec_horizontal_length(WVec vec);
[[nodiscard]] std::int32_t wvec_yaw(WVec vec);
[[nodiscard]] WPos wpos_lerp(WPos pos_a, WPos pos_b, std::int32_t int4_mul, std::int32_t int4_div);
[[nodiscard]] std::int32_t int2_lerp(std::int32_t int4_a, std::int32_t int4_b, std::int32_t int4_mul,
                                     std::int32_t int4_div);
[[nodiscard]] WRot wrot_from_yaw(std::int32_t int4_yaw);
[[nodiscard]] RotMatrix wrot_as_matrix(WRot rot);
[[nodiscard]] WVec wvec_rotate_matrix(WVec vec, const RotMatrix& mtx);
[[nodiscard]] WVec wvec_rotate_yaw(WVec vec, std::int32_t int4_yaw);

}

namespace ora::game::pathfinding {

constexpr std::int16_t kCostUnreachable{32767};

struct LocomotorTerrainSpeed {
  std::string str_type;
  std::int32_t int4_speed{};
  std::int32_t int4_cost{};
};

struct LocomotorDef {
  std::string str_name;
  std::vector<LocomotorTerrainSpeed> vec_speeds;
};

struct LocomotorMap {
  std::int32_t int4_width{};
  std::int32_t int4_height{};
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};
  std::int32_t int4_min_cost{};
  std::vector<std::int16_t> vec2_cost;
  std::vector<std::int32_t> vec4_speed;
};

[[nodiscard]] std::expected<LocomotorDef, std::string> locomotor_def_from_node(
    const MiniYamlNode& node_locomotor);

[[nodiscard]] LocomotorMap locomotor_map_build(const MapBin& bin_map, const Tileset& tileset,
                                               const LocomotorDef& def_locomotor, std::int32_t int4_left,
                                               std::int32_t int4_top, std::int32_t int4_right,
                                               std::int32_t int4_bottom);

[[nodiscard]] bool map_contains(const LocomotorMap& map, CPos cell);

[[nodiscard]] std::int16_t movement_cost_for_cell(const LocomotorMap& map, CPos cell);

[[nodiscard]] std::int32_t terrain_speed_for_cell(const LocomotorMap& map, CPos cell);

}

namespace ora::game::pathfinding {

struct CellInfoNode {
  std::uint8_t u1_status{};
  std::int32_t int4_cost_so_far{};
  std::int32_t int4_est_total{};
  CPos cell_prev;
};

using CellBlockSet = std::set<std::pair<std::int32_t, std::int32_t>>;

[[nodiscard]] std::vector<CPos> astar_to_target_cell(const LocomotorMap& map, CPos cell_from,
                                                     CPos cell_target, std::int32_t int4_weight,
                                                     bool b_lane_bias,
                                                     const CellBlockSet* ptr_set_blocked = nullptr,
                                                     bool b_reverse = false);

[[nodiscard]] std::vector<CPos> astar_to_target_cell_multi(
    const LocomotorMap& map, const std::vector<CPos>& vec_cells_from, CPos cell_target,
    std::int32_t int4_weight, bool b_lane_bias, bool b_lane_bias_reverse);

[[nodiscard]] std::vector<CPos> find_tiles_in_annulus(const LocomotorMap& map, CPos cell_center,
                                                      std::int32_t int4_min_range,
                                                      std::int32_t int4_max_range);

}

namespace ora::game::pathfinding {

struct HpfGridInfo {
  bool b_has_single{};
  CPos cell_single{};
  std::map<std::pair<std::int32_t, std::int32_t>, CPos> map_local_to_abstract;
};

struct HpfGraph {
  const LocomotorMap* ptr_map{};
  std::int32_t int4_grid_xs{};
  std::int32_t int4_grid_ys{};
  std::int32_t int4_min_cost{};
  std::int32_t int4_diag_cost{};
  std::vector<HpfGridInfo> vec_grids;
  std::map<std::pair<std::int32_t, std::int32_t>, std::vector<GraphConnection>> map_abstract_edges;
  std::map<std::pair<std::int32_t, std::int32_t>, std::uint32_t> map_domains;
};

[[nodiscard]] HpfGraph hpf_build(const LocomotorMap& map);

[[nodiscard]] std::vector<CPos> hpf_find_path_to_target_cell(const HpfGraph& graph, CPos cell_from,
                                                             CPos cell_target,
                                                             std::int32_t int4_weight);

}

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
  bool b_path_preset{};
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

  std::int32_t int4_speed_den{100};
  std::int32_t int4_turn_speed{512};
  std::int32_t int4_heuristic_weight{125};
  const CellBlockSet* ptr_set_blocked{};

  bool b_reverse_path{};
};

void move_queue(MoveSimState& st, CPos cell_destination);
void move_sim_tick(MoveSimState& st, const MoveSimConfig& cfg, std::int32_t int4_world_tick);

[[nodiscard]] WPos center_of_cell(CPos cell);
[[nodiscard]] WPos between_cells(CPos cell_a, CPos cell_b);
[[nodiscard]] std::int32_t facing_between(CPos cell, CPos cell_towards,
                                          std::int32_t int4_fallback);

}
}
