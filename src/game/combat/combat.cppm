export module ora.game.combat;

import std;
import ora.game.sound;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

export {
namespace ora::game::combat {

enum class WarheadKind : std::uint8_t {
  kSpreadDamage,
  kCreateEffect,
  kLeaveSmudge,
};

struct WarheadDef {
  WarheadKind kind{WarheadKind::kSpreadDamage};
  std::string str_kind_name;
  int int4_delay{};
  int int4_inaccuracy{};
  int int4_damage{};
  std::vector<std::pair<std::string, int>> vec_versus;
  std::vector<int> vec_falloff;
  int int4_spread{};
  std::vector<std::string> vec_valid_targets;
  std::vector<std::string> vec_invalid_targets;
  std::vector<std::string> vec_explosions;
  std::vector<std::string> vec_impact_sounds;
  int int4_impact_sound_chance{100};
};

enum class ProjectileKind : std::uint8_t {
  kNone,
  kInstantHit,
  kBullet,
};

struct ProjectileDef {
  ProjectileKind kind{ProjectileKind::kNone};
  std::string str_kind_name;
  int int4_speed{};
  std::vector<int> vec_launch_angle;
  int int4_inaccuracy{};
  bool b_blockable{};
  int int4_contrail_length{};
};

struct WeaponDef {
  std::string str_name_lower;
  int int4_range{};
  int int4_min_range{};
  int int4_reload_delay{1};
  int int4_burst{1};
  std::vector<int> vec_burst_delays{5};
  bool b_target_actor_center{};
  ProjectileDef projectile;
  std::vector<WarheadDef> vec_warheads;
  std::vector<std::string> vec_valid_targets{"Ground", "Water"};
  std::vector<std::string> vec_invalid_targets;
  std::vector<std::string> vec_report;
};

struct WeaponTable {
  std::vector<WeaponDef> vec_weapons;

  [[nodiscard]] const WeaponDef* find(std::string_view str_name_lower) const {
    for (const WeaponDef& def : vec_weapons)
      if (def.str_name_lower == str_name_lower) return &def;
    return nullptr;
  }
};

[[nodiscard]] std::expected<WeaponTable, std::string> weapon_table_load(
    const std::vector<std::string>& vec_file_texts,
    const std::vector<std::string>& vec_file_names);

[[nodiscard]] std::optional<int> wdist_parse(std::string_view str_value);

}

namespace ora::game::combat {

struct DamagePacket {
  std::int32_t int4_value{};
};

struct HealthState {
  std::int32_t int4_hp{};
  std::int32_t int4_max_hp{};
  bool b_dead{};
  bool b_died_this_call{};
};

[[nodiscard]] std::int32_t health_inflict(HealthState& hp, const DamagePacket& pkt_damage,
                                          std::span<const std::int32_t> vec_victim_modifiers,
                                          bool b_ignore_modifiers = false);

[[nodiscard]] std::int32_t health_kill(HealthState& hp);

}

namespace ora::game::combat {

[[nodiscard]] std::int32_t apply_percentage_modifiers(
    std::int32_t int4_number, std::initializer_list<std::int32_t> list_percentages);
[[nodiscard]] std::int32_t apply_percentage_modifiers(
    std::int32_t int4_number, const std::vector<std::int32_t>& vec_percentages);

[[nodiscard]] std::int32_t int2_lerp(std::int32_t int4_a, std::int32_t int4_b,
                                     std::int32_t int4_mul, std::int32_t int4_div);

[[nodiscard]] std::int32_t get_damage_falloff(const std::vector<std::int32_t>& vec_falloff,
                                              std::int32_t int4_spread,
                                              std::int32_t int4_distance);

[[nodiscard]] std::int32_t damage_versus(const WarheadDef& wh, std::string_view str_armor_type);

[[nodiscard]] std::int32_t warhead_inflict_damage(const WarheadDef& wh,
                                                  const std::vector<std::int32_t>& vec_modifiers,
                                                  std::string_view str_armor_type);

[[nodiscard]] std::int32_t circle_distance_from_edge(pathfinding::WPos pos_origin,
                                                     pathfinding::WPos pos_impact,
                                                     std::int32_t int4_radius);

}

namespace ora::game::combat {

struct CombatUnit;

struct ArmamentState {
  const WeaponDef* ptr_weapon{};
  std::vector<pathfinding::WVec> vec_barrel_offsets;
  int int4_recoil_amount{};
  int int4_recoil_recovery{9};
  int int4_fire_delay_info{};
  int int4_recoil{};
  int int4_ticks_since_last_shot{};
  int int4_current_barrel{};
  int int4_fire_delay{};
  int int4_burst{};
};

void armament_tick(ArmamentState& arm);

[[nodiscard]] bool armament_can_fire(const CombatUnit& unit, const ArmamentState& arm,
                                     pathfinding::WPos pos_target, bool b_target_valid,
                                     bool b_turret_desired_achieved);

[[nodiscard]] bool armament_check_fire(
    CombatUnit& unit, ArmamentState& arm, pathfinding::WPos pos_target, bool b_target_valid,
    bool b_turret_desired_achieved,
    const std::function<void(CombatUnit&, ArmamentState&, pathfinding::WPos pos_muzzle,
                             pathfinding::WPos pos_passive_target)>& fn_fire);

[[nodiscard]] pathfinding::WVec armament_muzzle_offset(const CombatUnit& unit,
                                                       const ArmamentState& arm,
                                                       pathfinding::WVec vec_barrel);

[[nodiscard]] pathfinding::WPos armament_passive_target(const CombatUnit& unit,
                                                        const WeaponDef& weapon,
                                                        pathfinding::WPos pos_target_center);

}

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
  ora::game::sfx_context* ptr_sfx{nullptr};
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

namespace ora::game::combat {

[[nodiscard]] pathfinding::WPos instant_hit_target_pos(const WeaponDef& weapon,
                                                       pathfinding::WPos pos_passive);

[[nodiscard]] Effect bullet_create(Arena& arena, const WeaponDef& weapon,
                                   pathfinding::WPos pos_source, pathfinding::WPos pos_passive,
                                   std::uint32_t u4_owner);

void weapon_impact(Arena& arena, const WeaponDef& weapon, pathfinding::WPos pos_impact,
                   std::uint32_t u4_owner);

void warhead_do_impact(Arena& arena, const WarheadDef& wh, pathfinding::WPos pos_impact,
                       std::uint32_t u4_owner);

void effects_tick(Arena& arena);

[[nodiscard]] pathfinding::WPos wpos_lerp_quadratic(pathfinding::WPos pos_a,
                                                    pathfinding::WPos pos_b,
                                                    std::int32_t int4_pitch,
                                                    std::int32_t int4_mul, std::int32_t int4_div);

[[nodiscard]] pathfinding::WVec wvec_from_pdf2(support::MersenneTwister& rng);

[[nodiscard]] std::int32_t wdist_from_pdf(support::MersenneTwister& rng, std::int32_t int4_samples);

}
}
