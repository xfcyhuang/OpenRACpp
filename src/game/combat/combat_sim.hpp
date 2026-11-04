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

// 剧本世界（T12/issue #13）：双玩家敌对 + 最小合成规则集三单位型
// （rifleman/tank/arty，探针 t12-scratch 同配方）+ effects + 帧尾任务 +
// SharedRandom(MersenneTwister(0)) + 段序 tick（Activity → ITick → effects → 帧尾排干，
// 对齐上游 World.Tick）。dump 行/sync_hash 供逐 tick 对拍。

struct UnitTemplate {
  std::string str_type;
  std::string str_armor_type;
  std::vector<std::string> vec_target_types;
  int int4_hit_radius{};
  int int4_max_hp{};
  int int4_speed{};
  int int4_turn_speed{};
  int int4_quantized_facings{};
  int int4_facing_tolerance{};  // AttackFrontal 0 / AttackFollow 默认 512
  bool b_attack_follow{};       // AttackTurreted/AttackFollow 型（ITick 段驱动开火）
  bool b_has_turret{};
  int int4_turret_turn_speed{};
  std::vector<pathfinding::WVec> vec_barrel_offsets;
  int int4_recoil{};
  int int4_recoil_recovery{};
  std::string_view str_weapon;
};

struct CombatUnit {
  // 静态（spawn 时从模板拷贝）
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

  // 运行时
  std::uint32_t u4_id{};
  HealthState health;
  int int4_facing{};  // body facing（WAngle 0-1023）
  pathfinding::WPos pos_center{};
  pathfinding::CPos cell_from{}, cell_to{};
  bool b_in_world{true};
  ArmamentState armament;
  int int4_turret_yaw{};   // Turreted.LocalOrientation.Yaw（WorldOrientation 不量化面）
  bool b_turret_desired_achieved{};
  bool b_is_aiming{};      // AttackBase.IsAiming（ISync）

  // Attack 活动状态（Attack/AttackActivity）
  bool b_attack_active{};
  std::uint32_t u4_attack_target{};
  pathfinding::WPos pos_last_visible{};
  bool b_has_last_visible{};
  int int4_last_visible_max_range{};

  // MoveWithinRange 子活动（超程接近；duelD）
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
    kInstantHit,      // 当帧 effects 段结算（列表尾插可见）→ 帧尾移除
    kBullet,          // 弹道飞行（ISync）
    kDelayedImpact,   // 延迟 warhead（帧尾到期结算）
    kContrailFader,   // 弹道尾迹淡出（ContrailLength>0 爆炸帧尾加；纯生命周期，无 RNG/伤害）
  };
  Kind kind{Kind::kBullet};
  std::uint32_t u4_serial{};  // 帧尾任务寻址（稳定 id）

  // Bullet
  pathfinding::WPos pos{}, last_pos{}, target{}, source{};
  std::int32_t int4_ticks{}, int4_length{}, int4_angle{}, int4_speed{};
  std::uint32_t u4_owner{};
  const WeaponDef* ptr_weapon{};
  pathfinding::WPos pos_passive{};  // ProjectileArgs.PassiveTarget

  // DelayedImpact
  std::int32_t int4_delay{};
  const WarheadDef* ptr_warhead{};
  pathfinding::WPos pos_impact{};   // 结算位置（散布点/爆点）
  std::uint32_t u4_impact_owner{};  // WarheadArgs.SourceActor
};

struct FrameEndTask {
  enum class Kind : std::uint8_t {
    kRemoveEffect,     // InstantHit/Bullet 爆炸/DelayedImpact 到期/ContrailFader 淡出移除
    kAddEffect,        // 帧尾加 effect（Weapon.Impact 的 DelayedImpact / Bullet 爆炸的 ContrailFader）
    kDelayedDoImpact,  // DelayedImpact 到期：先 remove 再 DoImpact（同一任务）
    kRemoveActor,      // Actor.Dispose（HP==0 → RemoveOnDeath）
  };
  Kind kind{Kind::kRemoveEffect};
  std::uint32_t u4_effect_serial{};
  Effect effect{};
  std::uint32_t u4_actor{};
};

struct Arena {
  const WeaponTable* ptr_weapons{};
  const pathfinding::LocomotorMap* ptr_locomotor_foot{};  // duelD 接近寻路（foot）
  std::vector<CombatUnit> vec_units;                      // id = 下标 + 3（world0/p0/p1 占位）
  support::MersenneTwister rng_shared;                    // MersenneTwister(0)
  std::vector<Effect> vec_effects;
  std::deque<FrameEndTask> deque_frame_end;
  std::uint32_t u4_next_effect_serial{1};
  std::int32_t int4_world_tick{};
};

// 单位模板表（最小合成规则集三单位型；键 = 类型名小写）
[[nodiscard]] const std::map<std::string, UnitTemplate, std::less<>>& unit_templates();

// 探针剧本等价：spawn（FacingInit=0）+ Attack 订单（AttackTarget，queued=false allowMove=true）
[[nodiscard]] std::expected<std::uint32_t, std::string> arena_spawn(Arena& arena,
                                                                    std::string_view str_type,
                                                                    std::uint32_t u4_owner,
                                                                    pathfinding::CPos cell);
void arena_attack_order(Arena& arena, std::uint32_t u4_unit, std::uint32_t u4_target);

// 段序 tick：Activity（id 序）→ ITick（trait 注册序）→ effects → 帧尾排干（直到空）
void arena_tick(Arena& arena);

[[nodiscard]] CombatUnit* arena_unit(Arena& arena, std::uint32_t u4_id);
[[nodiscard]] const CombatUnit* arena_unit(const Arena& arena, std::uint32_t u4_id);

// World.SyncHash() 语义复刻（int32 wrap；actors 段 → ISync 段[Shroud×2 占位 →
// Health/BodyOrientation/Mobile[/Turreted]/Attack.IsAiming] → syncedEffects[Bullet] → rng.Last）
[[nodiscard]] std::int32_t arena_sync_hash(const Arena& arena);

// dump tick 行（探针格式逐字段一致：tick/a/b/eff/rng/sync）
[[nodiscard]] std::string arena_dump_row(const Arena& arena, std::uint32_t u4_a,
                                         std::uint32_t u4_b);

// effects 列表描述（"N[Type,Type,...]"——Bullet/DelayedImpact；SpriteEffect 不入面）
[[nodiscard]] std::string arena_effects_desc(const Arena& arena);

}  // namespace ora::game::combat
