#pragma once

import std;

#include "game/map_bin.hpp"
#include "game/pathfinding/move_sim.hpp"
#include "game/pathfinding/path_graph.hpp"
#include "game/tileset.hpp"
#include "game/world/trait.hpp"

namespace ora::game::world {
struct WorldModel;
struct FrameEndTask;
}  // namespace ora::game::world

namespace ora::game::economy {

constexpr std::uint32_t kNoActor{0xffffffffu};

struct ResourceTypeSpec {
  std::string str_name;
  std::uint8_t u1_index{};
  std::string str_terrain;
  std::vector<std::string> vec_allowed_terrain;
  std::int32_t int4_max_density{};
  std::int32_t int4_value{};
};

struct ResourceCellState {
  std::string str_type;
  std::uint8_t u1_density{};
};

struct ResourceLayerState {
  std::vector<std::string> vec_type_order;
  std::map<std::string, ResourceTypeSpec> map_types;
  bool b_recalculate_density{};
  std::int32_t int4_res_cells{};
  std::int32_t int4_width{};
  std::int32_t int4_height{};
  std::vector<ResourceCellState> vec_cells;
};

struct HarvCargoState {
  std::vector<std::pair<std::string, std::int32_t>> vec_contents;
  std::int32_t int4_capacity{20};
};

enum class ActKind : std::uint8_t {
  kFindAndDeliver = 0,
  kHarvestResource = 1,
  kMoveToDock = 2,
  kMoveOntoAndTurn = 3,
  kGenericDock = 4,
  kWait = 5,
  kMove = 6,
  kTurn = 7,
  // 上游 MoveOnto（MoveAdjacentTo 子类）的移动：FindPathToTargetCells 反向搜索，
  // 观测行仍显示为 Move。
  kMoveReverse = 8,
};

struct HarvActivity {
  ActKind kind{};
  bool b_first_run{};
  bool b_finishing{};
  bool b_dock_initiated{};
  pathfinding::CPos cell_target{};
  std::int32_t int4_wait_remaining{};
  std::int32_t int4_dock_state{};
  std::int32_t int4_dock_frames{};
  std::int32_t int4_child{-1};
  std::int32_t int4_next{-1};
};

enum class MoveOutcome : std::uint8_t {
  kInProgress = 0,
  kCompleteDestinationReached = 1,
  kCompleteDestinationBlocked = 2,
};

struct HarvState {
  std::uint32_t u4_actor{kNoActor};
  pathfinding::MoveSimState move;
  std::vector<HarvActivity> vec_activities;
  std::int32_t int4_root{-1};
  HarvCargoState cargo;
  std::optional<pathfinding::CPos> opt_last_harvested;
  bool b_last_search_failed{};
  bool b_has_waited{};
  bool b_has_delivered_load{};
  bool b_has_harvested_cell{};
  std::uint32_t u4_reserved_host{kNoActor};
  std::uint32_t u4_last_reserved_host{kNoActor};
  MoveOutcome move_outcome{MoveOutcome::kInProgress};
  std::int32_t int4_unload_cooldown{};
  std::set<std::pair<std::int32_t, std::int32_t>> set_blocked_move;
};

struct DockHostState {
  std::uint32_t u4_actor{kNoActor};
  std::vector<std::uint32_t> vec_reserved;
  std::uint32_t u4_docked_client{kNoActor};
};

struct EconomyConfig {
  std::int32_t int4_bale_load_delay{4};
  std::int32_t int4_bale_unload_delay{1};
  std::int32_t int4_bale_unload_amount{1};
  std::int32_t int4_search_from_proc_radius{15};
  std::int32_t int4_search_from_harvester_radius{8};
  std::int32_t int4_harvest_facings{8};
  std::int32_t int4_harv_speed{72};
  std::int32_t int4_harv_turn_speed{20};
  std::int32_t int4_fully_loaded_speed{85};
  std::int32_t int4_dock_wait{10};
  std::int32_t int4_dock_frames{8};
  std::int32_t int4_dock_angle{256};
  std::int32_t int4_refinery_penalty{200};
  std::int32_t int4_wait_duration{25};
  std::int32_t int4_search_for_dock_delay{125};
  std::int32_t int4_max_queue_length{3};
  std::int32_t int4_harv_capacity{20};
  std::int32_t int4_player_capacity{2000};
  std::string str_free_actor{"harv"};
  std::int32_t int4_free_offset_x{1};
  std::int32_t int4_free_offset_y{2};
  std::int32_t int4_free_facing{256};
  pathfinding::CPos cell_dock{};
  pathfinding::WPos pos_dock{};
  std::string str_host_type{"proc"};
  std::string str_harv_type{"harv"};
  bool b_use_storage{true};
  bool b_discard_excess{false};
  std::vector<pathfinding::LocomotorTerrainSpeed> vec_locomotor_speeds;
};

struct EconomyWorldState {
  ResourceLayerState layer;
  std::vector<HarvState> vec_harvs;
  std::vector<DockHostState> vec_hosts;
  pathfinding::LocomotorMap map_move;
  std::map<std::pair<std::int32_t, std::int32_t>, std::pair<std::int16_t, std::int32_t>> map_cost_base;
  EconomyConfig cfg;
  bool b_ready{};
};

[[nodiscard]] std::expected<void, std::string> economy_setup(
    world::WorldModel& w, const MapBin& bin_map, const Tileset& tileset,
    std::string_view str_rules_text, std::int32_t int4_left, std::int32_t int4_top,
    std::int32_t int4_right, std::int32_t int4_bottom);

void economy_on_tick(world::WorldModel& w);

void economy_on_actor_added(world::WorldModel& w, std::uint32_t u4_actor_id);

void economy_on_actor_removed(world::WorldModel& w, std::uint32_t u4_actor_id);

void economy_frame_end_task(world::WorldModel& w, const world::FrameEndTask& task);

[[nodiscard]] std::int32_t harv_cargo_sum(const HarvCargoState& cargo);

[[nodiscard]] bool harv_is_full(const EconomyWorldState& econ, const HarvState& harv);

[[nodiscard]] bool harv_is_empty(const HarvState& harv);

[[nodiscard]] std::int32_t harv_fullness(const HarvState& harv);

void cargo_add(HarvCargoState& cargo, std::string_view str_type, std::int32_t int4_amount);

std::int32_t refinery_accept(world::WorldModel& w, std::uint32_t u4_player,
                             std::string_view str_type, std::int32_t int4_count);

std::int32_t resource_remove(ResourceLayerState& layer, EconomyWorldState& econ,
                             std::string_view str_type, pathfinding::CPos cell);

[[nodiscard]] const ResourceCellState& resource_get(const ResourceLayerState& layer,
                                                    pathfinding::CPos cell);

[[nodiscard]] std::string economy_observation(const world::WorldModel& w,
                                              std::uint32_t u4_player);

[[nodiscard]] std::string economy_actor_line(const world::WorldModel& w);

[[nodiscard]] std::string harv_activity_chain(const HarvState& harv);

[[nodiscard]] const std::vector<world::TraitSpec>& economy_trait_specs();

}  // namespace ora::game::economy
