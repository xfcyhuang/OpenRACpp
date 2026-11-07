#pragma once

import std;

#include <functional>

#include "game/combat/armament.hpp"
#include "game/combat/health.hpp"
#include "game/combat/weapon.hpp"
#include "game/pathfinding/move_sim.hpp"
#include "game/pathfinding/path_graph.hpp"
#include "game/pathfinding/wangle.hpp"
#include "game/support/mersenne_twister.hpp"

namespace ora::game::combat {

struct UnitTemplate {
  std::string str_type;
  std::string str_armor_type;
  std::vector<std::string> vec_target_types;
  int int4_hit_radius{};
  int int4_max_hp{};
  int int4_speed{};
  int int4_turn_speed{};
  int int4_quantized_facings{};
  int int4_facing_tolerance{};
  bool b_attack_follow{};
  bool b_has_turret{};
  int int4_turret_turn_speed{};
  std::vector<pathfinding::WVec> vec_barrel_offsets;
  int int4_recoil{};
  int int4_recoil_recovery{};
  std::string_view str_weapon;
};

struct CombatUnit {

  std::string str_type;
  std::uint32_t u4_owner{};
  std::string str_armor_type;
  std::vector<std::string> vec_target_types;
  int int4_hit_radius{};
  int int4_speed{};
  int int4_turn_speed{};
  int int4_quantized_facings{};
  int int4_facing_tolerance{};
  bool b_attack_follow{};
  bool b_has_turret{};
  int int4_turret_turn_speed{};

  std::uint32_t u4_id{};
  HealthState health;
  int int4_facing{};
  pathfinding::WPos pos_center{};
  pathfinding::CPos cell_from{}, cell_to{};
  bool b_in_world{true};
  ArmamentState armament;
  int int4_turret_yaw{};
  bool b_turret_desired_achieved{};
  bool b_is_aiming{};

  bool b_attack_active{};
  std::uint32_t u4_attack_target{};
  pathfinding::WPos pos_last_visible{};
  bool b_has_last_visible{};
  int int4_last_visible_max_range{};

  struct MoveWithinRangeState {
    bool b_active{};
    std::int32_t int4_min_cells{}, int4_max_cells{};
    std::int32_t int4_search_cells_tick{-1};
    std::vector<pathfinding::CPos> vec_search_cells;
    bool b_move_active{};
    pathfinding::MoveSimState move;
  } move_within_range;
};

struct Effect {
  enum class Kind : std::uint8_t {
    kInstantHit,
    kBullet,
    kDelayedImpact,
    kContrailFader,
  };
  Kind kind{Kind::kBullet};
  std::uint32_t u4_serial{};

  pathfinding::WPos pos{}, last_pos{}, target{}, source{};
  std::int32_t int4_ticks{}, int4_length{}, int4_angle{}, int4_speed{};
  std::uint32_t u4_owner{};
  const WeaponDef* ptr_weapon{};
  pathfinding::WPos pos_passive{};

  std::int32_t int4_delay{};
  const WarheadDef* ptr_warhead{};
  pathfinding::WPos pos_impact{};
  std::uint32_t u4_impact_owner{};
};

struct FrameEndTask {
  enum class Kind : std::uint8_t {
    kRemoveEffect,
    kAddEffect,
    kDelayedDoImpact,
    kRemoveActor,
  };
  Kind kind{Kind::kRemoveEffect};
  std::uint32_t u4_effect_serial{};
  Effect effect{};
  std::uint32_t u4_actor{};
};

struct Arena {
  const WeaponTable* ptr_weapons{};
  const pathfinding::LocomotorMap* ptr_locomotor_foot{};
  std::vector<CombatUnit> vec_units;
  support::MersenneTwister rng_shared;
  std::vector<Effect> vec_effects;
  std::deque<FrameEndTask> deque_frame_end;
  std::uint32_t u4_next_effect_serial{1};
  std::int32_t int4_world_tick{};
};

[[nodiscard]] const std::map<std::string, UnitTemplate, std::less<>>& unit_templates();

[[nodiscard]] std::expected<std::uint32_t, std::string> arena_spawn(Arena& arena,
                                                                    std::string_view str_type,
                                                                    std::uint32_t u4_owner,
                                                                    pathfinding::CPos cell);
void arena_attack_order(Arena& arena, std::uint32_t u4_unit, std::uint32_t u4_target);

void arena_tick(Arena& arena);

[[nodiscard]] CombatUnit* arena_unit(Arena& arena, std::uint32_t u4_id);
[[nodiscard]] const CombatUnit* arena_unit(const Arena& arena, std::uint32_t u4_id);

[[nodiscard]] std::int32_t arena_sync_hash(const Arena& arena);

[[nodiscard]] std::string arena_dump_row(const Arena& arena, std::uint32_t u4_a,
                                         std::uint32_t u4_b);

[[nodiscard]] std::string arena_effects_desc(const Arena& arena);

}
