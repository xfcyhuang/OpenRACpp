#include "game/combat/combat_sim.hpp"

#include <algorithm>

#include "game/combat/armament.hpp"
#include "game/combat/projectile.hpp"
#include "game/combat/warhead_damage.hpp"
#include "game/pathfinding/astar.hpp"
#include "game/pathfinding/move_sim.hpp"
#include "game/pathfinding/wangle.hpp"

namespace ora::game::combat {

namespace {

std::int32_t wrap32(std::int64_t int8_value) {
  const std::int64_t int8_wrapped{((int8_value % 4294967296) + 4294967296) % 4294967296};
  return static_cast<std::int32_t>(int8_wrapped - (int8_wrapped >= 2147483648 ? 4294967296 : 0));
}

std::int32_t wpos_hash_code(pathfinding::WPos pos) {
  return pos.int4_x ^ pos.int4_y ^ pos.int4_z;
}

std::int32_t cpos_bits(pathfinding::CPos cell) {
  return ((cell.int4_x & 0xFFF) << 20) | ((cell.int4_y & 0xFFF) << 8);
}

// Util.FacingWithinTolerance（Util.cs:120-127）
bool facing_within_tolerance(std::int32_t int4_facing, std::int32_t int4_desired,
                             std::int32_t int4_tolerance) {
  if (int4_tolerance == 0 && int4_facing == int4_desired) return true;
  const std::int32_t int4_delta{pathfinding::wangle_wrap(int4_desired - int4_facing)};
  return int4_delta <= int4_tolerance || int4_delta >= 1024 - int4_tolerance;
}

// Target.IsInRange（Target.cs:196-203）：Positions 任一 2D 水平距离 <= range（含等号）。
// 最小面 Positions = [CenterPosition]
bool position_in_range(pathfinding::WPos pos_origin, pathfinding::WPos pos_target,
                       std::int32_t int4_range) {
  const pathfinding::WVec vec{pathfinding::wvec_sub(pos_target, pos_origin)};
  return pathfinding::wvec_horizontal_length_squared(vec) <=
         static_cast<std::int64_t>(int4_range) * int4_range;
}

// Weapon.IsValidAgainst（WeaponInfo.cs:209-221）：CanTargetSelf + TargetTypes Overlap
bool weapon_valid_against(const WeaponDef& weapon, const CombatUnit& victim,
                          std::uint32_t u4_firer) {
  bool b_overlaps{false};
  for (const std::string& str_valid : weapon.vec_valid_targets)
    for (const std::string& str_type : victim.vec_target_types)
      if (str_type == str_valid) b_overlaps = true;
  bool b_invalid_hit{false};
  for (const std::string& str_invalid : weapon.vec_invalid_targets)
    for (const std::string& str_type : victim.vec_target_types)
      if (str_type == str_invalid) b_invalid_hit = true;
  return b_overlaps && !b_invalid_hit && !(victim.u4_id == u4_firer);
}

// AttackBase.ChooseArmamentsForTarget 关系门（TargetRelationships=Enemy——双玩家敌对）
bool target_is_enemy(const Arena& arena, std::uint32_t u4_firer, std::uint32_t u4_victim) {
  (void)arena;
  return u4_firer != u4_victim;
}

// —— MoveWithinRange 子活动装配（MoveWithinRange.cs + PathFinder.FindPathToTargetCells）——
// maxCells = (maxRange+1023)/1024、minCells = minRange/1024（:34-35）；annulus 候选
// （TilesByDistance 序）CanStay/CanEnter/AtCorrectRange 过滤 → 多源反向 A*（swap：
// 候选=源、当前位置=目标、HeuristicWeight 125、laneBias 反向奇偶）→ reverse → Move。
void queue_move_within_range(Arena& arena, CombatUnit& unit, const CombatUnit& target) {
  auto& mwr{unit.move_within_range};
  const std::int32_t int4_max_range{unit.armament.ptr_weapon->int4_range};
  const std::int32_t int4_min_range{unit.armament.ptr_weapon->int4_min_range};
  mwr.int4_max_cells = (int4_max_range + 1023) / 1024;
  mwr.int4_min_cells = int4_min_range / 1024;

  // CalculatePathToTarget（:51-66）：候选格缓存按 tick（此处一次性——目标静止）
  const pathfinding::CPos cell_target_location{target.cell_to};
  std::vector<pathfinding::CPos> vec_candidates;
  for (const pathfinding::CPos cell :
       pathfinding::find_tiles_in_annulus(*arena.ptr_locomotor_foot, cell_target_location,
                                          mwr.int4_min_cells, mwr.int4_max_cells)) {
    if (pathfinding::movement_cost_for_cell(*arena.ptr_locomotor_foot, cell) ==
        pathfinding::kCostUnreachable)
      continue;  // CanStayInCell && CanEnterCell（BlockedByActor 面无阻挡）
    const pathfinding::WPos pos_cell{pathfinding::center_of_cell(cell)};
    if (!position_in_range(pos_cell, target.pos_center, int4_max_range)) continue;
    if (int4_min_range != 0 && position_in_range(pos_cell, target.pos_center, int4_min_range))
      continue;  // AtCorrectRange（格心 + FromSubCell=格心面）
    vec_candidates.push_back(cell);
  }

  if (vec_candidates.empty()) return;  // NoPath → Move 活动立即失败放弃

  // FindPathToTargetCells：swap 多源反向搜索（accessible 过滤同 CanStay 面）。
  // 返回序 = [A* target(攻击者当前格), ..., A* source(候选格)] —— swap 后恰好就是
  // 单位的行走序（起点→终点），无需再 reverse（back() = 移动终点候选格）。
  std::vector<pathfinding::CPos> vec_path{
      pathfinding::astar_to_target_cell_multi(*arena.ptr_locomotor_foot, vec_candidates,
                                              unit.cell_to, 125, true, true)};
  if (vec_path.empty()) return;

  if (vec_path.size() < 2) return;  // 已在候选格（ShouldStop 会截停）

  mwr.b_active = true;
  mwr.b_move_active = true;
  mwr.move = pathfinding::MoveSimState{};
  mwr.move.cell_from = unit.cell_from;
  mwr.move.cell_to = unit.cell_to;
  mwr.move.int4_facing = unit.int4_facing;
  mwr.move.pos_center = unit.pos_center;
  // Move 活动：外部预置路径（path[0] = 当前格，从 path[1] 起逐格走；
  // 内部形式 = 倒序 [终点, ..., 下一格]）
  mwr.move.vec_path.assign(vec_path.rbegin(), vec_path.rend() - 1);
  mwr.move.b_path_preset = true;
  mwr.move.b_has_activity = true;
  mwr.move.b_has_destination = true;
  mwr.move.cell_destination = vec_path.back();
}

// FireBarrel：创建 projectile（InstantHit 同帧尾插 / Bullet 含 RNG 消耗）
void fire_barrel(Arena& arena, CombatUnit& unit, ArmamentState& arm,
                 pathfinding::WPos pos_muzzle, pathfinding::WPos pos_passive) {
  const WeaponDef& weapon{*arm.ptr_weapon};

  if (weapon.projectile.kind == ProjectileKind::kInstantHit) {
    Effect eff;
    eff.kind = Effect::Kind::kInstantHit;
    eff.u4_serial = arena.u4_next_effect_serial++;
    eff.ptr_weapon = &weapon;
    eff.u4_owner = unit.u4_id;
    // InstantHit ctor：Inaccuracy>0 → PassiveTarget + FromPDF*max/1024；剧本面恒无散布
    eff.pos_impact = instant_hit_target_pos(weapon, pos_passive);
    eff.pos = pos_muzzle;
    eff.source = pos_muzzle;
    arena.vec_effects.push_back(std::move(eff));
  } else {
    // Recoil 在 projectile 创建回调内设置（Armament.FireBarrel 的 delayed lambda）
    arena.vec_effects.push_back(
        bullet_create(arena, weapon, pos_muzzle, pos_passive, unit.u4_id));
  }

  arm.int4_recoil = arm.int4_recoil_amount;
}

// —— Activity 段：Attack 活动（AttackFrontal 型，Activities/Attack.cs）——
void attack_activity_tick(Arena& arena, CombatUnit& unit) {
  if (!unit.b_attack_active) return;

  const CombatUnit* ptr_target{arena_unit(arena, unit.u4_attack_target)};

  // MoveWithinRange 子活动的 child Move 一步（TickChild；返回 true = 移动仍挂起）
  const auto tick_move_child{[&arena](CombatUnit& u) {
    pathfinding::MoveSimConfig cfg;
    cfg.ptr_map = arena.ptr_locomotor_foot;
    cfg.int4_speed = u.int4_speed;
    cfg.int4_turn_speed = u.int4_turn_speed;
    cfg.int4_heuristic_weight = 125;
    pathfinding::move_sim_tick(u.move_within_range.move, cfg, arena.int4_world_tick);
    u.cell_from = u.move_within_range.move.cell_from;
    u.cell_to = u.move_within_range.move.cell_to;
    u.pos_center = u.move_within_range.move.pos_center;
    u.int4_facing = u.move_within_range.move.int4_facing;
    if (!u.move_within_range.move.b_has_activity) {
      u.move_within_range.b_move_active = false;
      u.move_within_range.b_active = false;
      // Move 完成 → MWR 子活动链完成，同帧继续 TickAttack
      return false;
    }
    return true;
  }};

  // 1. TickChild：MoveWithinRange 子活动（duelD 超程接近）
  if (unit.move_within_range.b_active) {
    // MoveWithinRange/MoveAdjacentTo.Tick：ShouldStop → Cancel；否则 child Move tick
    const CombatUnit* ptr_move_target{ptr_target};
    const bool b_stop{
        ptr_move_target == nullptr
            ? false
            : position_in_range(unit.pos_center, ptr_move_target->pos_center,
                                unit.armament.ptr_weapon->int4_range) &&
                  !position_in_range(unit.pos_center, ptr_move_target->pos_center,
                                     unit.armament.ptr_weapon->int4_min_range)};
    if (b_stop || ptr_move_target == nullptr) {
      unit.move_within_range.b_active = false;
      unit.move_within_range.b_move_active = false;
    } else if (unit.move_within_range.b_move_active) {
      if (tick_move_child(unit)) return;  // 移动中，活动挂起（IsAiming 保持上次值）
    }
  }

  // 2. target 有效性（死/移除 → useLastVisibleTarget → FromPos 恒有效）
  const bool b_target_alive{ptr_target != nullptr && ptr_target->b_in_world};
  if (!b_target_alive) {
    if (unit.b_has_last_visible &&
        position_in_range(unit.pos_center, unit.pos_last_visible,
                          unit.int4_last_visible_max_range)) {
      // 活动完成（OnLastRun → IsAiming=false）
      unit.b_attack_active = false;
      unit.b_is_aiming = false;
    }
    return;
  }

  const CombatUnit& target{*ptr_target};
  const pathfinding::WPos pos_target_center{target.pos_center};

  // TickAttack（:164-252）
  const bool b_valid_armament{target_is_enemy(arena, unit.u4_id, target.u4_id) &&
                              weapon_valid_against(*unit.armament.ptr_weapon, target, unit.u4_id)};
  const std::int32_t int4_max_range{unit.armament.ptr_weapon->int4_range};
  const std::int32_t int4_min_range{unit.armament.ptr_weapon->int4_min_range};
  const bool b_in_range{position_in_range(unit.pos_center, pos_target_center, int4_max_range)};
  const bool b_too_close{int4_min_range != 0 &&
                         position_in_range(unit.pos_center, pos_target_center, int4_min_range)};

  if (!b_valid_armament || !b_in_range || b_too_close) {
    // NeedsToMove → QueueChild(MoveWithinRange)；IsAiming = false
    unit.b_is_aiming = false;
    if (!unit.move_within_range.b_active && arena.ptr_locomotor_foot != nullptr) {
      // 移动指令仅一次（Attack 活动 QueueChild 后 child 接管直到完成）
      queue_move_within_range(arena, unit, target);
      // Activity.TickOuter（Activity.cs:130-136）："Avoid a single tick delay if the
      // childactivity was just queued" —— 新 child 同帧立即 tick（MWR → Move 首
      // tick = Turn 一步，duelD GOLD t1 即 face 512）
      if (unit.move_within_range.b_move_active) (void)tick_move_child(unit);
    }
    return;
  }

  // 转身（FacingTolerance 0：精确朝向；内联 TurnFacing 不排子活动）
  const pathfinding::WVec vec_to_target{pathfinding::wvec_sub(pos_target_center, unit.pos_center)};
  const std::int32_t int4_desired_yaw{pathfinding::wvec_yaw(vec_to_target)};
  if (!facing_within_tolerance(unit.int4_facing, int4_desired_yaw, unit.int4_facing_tolerance)) {
    unit.int4_facing = pathfinding::tick_facing(unit.int4_facing, int4_desired_yaw,
                                                unit.int4_turn_speed);
    if (!facing_within_tolerance(unit.int4_facing, int4_desired_yaw,
                                 unit.int4_facing_tolerance)) {
      unit.b_is_aiming = true;  // NeedsToTurn
      return;
    }
  }

  // Attacking → DoAttack → CheckFire
  unit.b_is_aiming = true;
  armament_check_fire(unit, unit.armament, pos_target_center, b_valid_armament, true,
                      [&arena](CombatUnit& u, ArmamentState& a, pathfinding::WPos m,
                               pathfinding::WPos p) { fire_barrel(arena, u, a, m, p); });
}

// —— ITick 段：AttackFollow/AttackTurreted（tank）驱动 ——
void attack_follow_tick(Arena& arena, CombatUnit& unit) {
  if (!unit.b_attack_active) {
    unit.b_is_aiming = false;
    return;
  }
  const CombatUnit* ptr_target{arena_unit(arena, unit.u4_attack_target)};
  if (ptr_target == nullptr || !ptr_target->b_in_world) {
    unit.b_is_aiming = false;  // RequestedTarget invalid → else 分支无 Opportunity
    return;
  }
  const CombatUnit& target{*ptr_target};

  // CanAimAtTarget（AttackFollow.cs:86-103）：in range && min 外 && firing arc（512 容忍恒过）
  const bool b_valid_armament{target_is_enemy(arena, unit.u4_id, target.u4_id) &&
                              weapon_valid_against(*unit.armament.ptr_weapon, target, unit.u4_id)};
  const bool b_aim{b_valid_armament &&
                   position_in_range(unit.pos_center, target.pos_center,
                                     unit.armament.ptr_weapon->int4_range) &&
                   (unit.armament.ptr_weapon->int4_min_range == 0 ||
                    !position_in_range(unit.pos_center, target.pos_center,
                                       unit.armament.ptr_weapon->int4_min_range))};
  unit.b_is_aiming = b_aim;
  if (!b_aim) return;

  // DoAttack → AttackTurreted.CanAttack → Turreted.FaceTarget（转炮塔）
  const pathfinding::WVec vec_desired{pathfinding::wvec_sub(target.pos_center, unit.pos_center)};
  const std::int32_t int4_desired_local{
      vec_desired.int4_x == 0 && vec_desired.int4_y == 0 && vec_desired.int4_z == 0
          ? unit.int4_turret_yaw
          : pathfinding::wangle_wrap(pathfinding::wvec_yaw(vec_desired) - unit.int4_facing)};
  if (int4_desired_local != unit.int4_turret_yaw) {
    unit.int4_turret_yaw = pathfinding::tick_facing(unit.int4_turret_yaw, int4_desired_local,
                                                    unit.int4_turret_turn_speed);
  }
  unit.b_turret_desired_achieved = int4_desired_local == unit.int4_turret_yaw;

  if (unit.b_turret_desired_achieved) {
    armament_check_fire(unit, unit.armament, target.pos_center, b_valid_armament,
                        unit.b_turret_desired_achieved,
                        [&arena](CombatUnit& u, ArmamentState& a, pathfinding::WPos m,
                                 pathfinding::WPos p) { fire_barrel(arena, u, a, m, p); });
  }
}

// —— 帧尾任务执行 ——
void frame_end_run(Arena& arena, const FrameEndTask& task) {
  switch (task.kind) {
    case FrameEndTask::Kind::kRemoveEffect: {
      for (auto it{arena.vec_effects.begin()}; it != arena.vec_effects.end(); ++it)
        if (it->u4_serial == task.u4_effect_serial) {
          arena.vec_effects.erase(it);
          break;
        }
      break;
    }
    case FrameEndTask::Kind::kAddEffect:
      arena.vec_effects.push_back(task.effect);
      break;
    case FrameEndTask::Kind::kDelayedDoImpact: {
      for (auto it{arena.vec_effects.begin()}; it != arena.vec_effects.end(); ++it) {
        if (it->u4_serial == task.u4_effect_serial) {
          const Effect eff{*it};
          arena.vec_effects.erase(it);
          warhead_do_impact(arena, *eff.ptr_warhead, eff.pos_impact, eff.u4_impact_owner);
          break;
        }
      }
      break;
    }
    case FrameEndTask::Kind::kRemoveActor: {
      if (CombatUnit* ptr_unit{arena_unit(arena, task.u4_actor)}) ptr_unit->b_in_world = false;
      break;
    }
  }
}

}  // namespace

const std::map<std::string, UnitTemplate, std::less<>>& unit_templates() {
  static const std::map<std::string, UnitTemplate, std::less<>> map_templates{
      {"rifleman",
       UnitTemplate{.str_type = "rifleman",
                    .str_armor_type = "None",
                    .vec_target_types = {"GroundActor", "Infantry"},
                    .int4_hit_radius = 128,
                    .int4_max_hp = 5000,
                    .int4_speed = 54,
                    .int4_turn_speed = 512,
                    .int4_quantized_facings = 8,
                    .int4_facing_tolerance = 0,
                    .b_attack_follow = false,
                    .b_has_turret = false,
                    .int4_turret_turn_speed = 0,
                    .vec_barrel_offsets = {},
                    .int4_recoil = 0,
                    .int4_recoil_recovery = 9,
                    .str_weapon = "m1carbine"}},
      {"tank",
       UnitTemplate{.str_type = "tank",
                    .str_armor_type = "Heavy",
                    .vec_target_types = {"GroundActor", "Vehicle"},
                    .int4_hit_radius = 426,
                    .int4_max_hp = 23000,
                    .int4_speed = 113,
                    .int4_turn_speed = 512,
                    .int4_quantized_facings = 32,
                    .int4_facing_tolerance = 512,
                    .b_attack_follow = true,
                    .b_has_turret = true,
                    .int4_turret_turn_speed = 28,
                    .vec_barrel_offsets = {pathfinding::WVec{768, 0, 90}},
                    .int4_recoil = 85,
                    .int4_recoil_recovery = 25,
                    .str_weapon = "25mm"}},
      {"arty",
       UnitTemplate{.str_type = "arty",
                    .str_armor_type = "Light",
                    .vec_target_types = {"GroundActor", "Vehicle"},
                    .int4_hit_radius = 426,
                    .int4_max_hp = 10000,
                    .int4_speed = 72,
                    .int4_turn_speed = 8,
                    .int4_quantized_facings = 32,
                    .int4_facing_tolerance = 0,
                    .b_attack_follow = false,
                    .b_has_turret = false,
                    .int4_turret_turn_speed = 0,
                    .vec_barrel_offsets = {pathfinding::WVec{624, 0, 208}},
                    .int4_recoil = 0,
                    .int4_recoil_recovery = 9,
                    .str_weapon = "155mm"}},
  };
  return map_templates;
}

std::expected<std::uint32_t, std::string> arena_spawn(Arena& arena, std::string_view str_type,
                                                      std::uint32_t u4_owner,
                                                      pathfinding::CPos cell) {
  const auto it{unit_templates().find(str_type)};
  if (it == unit_templates().end()) return std::unexpected("unknown unit type " + std::string(str_type));
  const UnitTemplate& tpl{it->second};

  const WeaponDef* ptr_weapon{arena.ptr_weapons->find(tpl.str_weapon)};
  if (ptr_weapon == nullptr) return std::unexpected("unknown weapon " + std::string(tpl.str_weapon));

  CombatUnit unit;
  unit.str_type = tpl.str_type;
  unit.u4_owner = u4_owner;
  unit.str_armor_type = tpl.str_armor_type;
  unit.vec_target_types = tpl.vec_target_types;
  unit.int4_hit_radius = tpl.int4_hit_radius;
  unit.int4_speed = tpl.int4_speed;
  unit.int4_turn_speed = tpl.int4_turn_speed;
  unit.int4_quantized_facings = tpl.int4_quantized_facings;
  unit.int4_facing_tolerance = tpl.int4_facing_tolerance;
  unit.b_attack_follow = tpl.b_attack_follow;
  unit.b_has_turret = tpl.b_has_turret;
  unit.int4_turret_turn_speed = tpl.int4_turret_turn_speed;

  unit.u4_id = static_cast<std::uint32_t>(arena.vec_units.size() + 3);  // world0/p0/p1
  unit.health.int4_hp = tpl.int4_max_hp;
  unit.health.int4_max_hp = tpl.int4_max_hp;
  unit.cell_from = cell;
  unit.cell_to = cell;
  unit.pos_center = pathfinding::center_of_cell(cell);
  unit.armament.ptr_weapon = ptr_weapon;
  unit.armament.vec_barrel_offsets = tpl.vec_barrel_offsets;
  unit.armament.int4_recoil_amount = tpl.int4_recoil;
  unit.armament.int4_recoil_recovery = tpl.int4_recoil_recovery;
  unit.armament.int4_burst = ptr_weapon->int4_burst;

  arena.vec_units.push_back(std::move(unit));
  return arena.vec_units.back().u4_id;
}

void arena_attack_order(Arena& arena, std::uint32_t u4_unit, std::uint32_t u4_target) {
  CombatUnit* ptr_unit{arena_unit(arena, u4_unit)};
  const CombatUnit* ptr_target{arena_unit(arena, u4_target)};
  if (ptr_unit == nullptr || ptr_target == nullptr) return;

  ptr_unit->b_attack_active = true;
  ptr_unit->u4_attack_target = u4_target;
  // Attack ctor：lastVisibleTarget = FromTargetPositions；lastVisibleMaximumRange = 射程
  ptr_unit->pos_last_visible = ptr_target->pos_center;
  ptr_unit->b_has_last_visible = true;
  ptr_unit->int4_last_visible_max_range = ptr_unit->armament.ptr_weapon->int4_range;
}

CombatUnit* arena_unit(Arena& arena, std::uint32_t u4_id) {
  if (u4_id < 3 || u4_id - 3 >= arena.vec_units.size()) return nullptr;
  return &arena.vec_units[u4_id - 3];
}

const CombatUnit* arena_unit(const Arena& arena, std::uint32_t u4_id) {
  if (u4_id < 3 || u4_id - 3 >= arena.vec_units.size()) return nullptr;
  return &arena.vec_units[u4_id - 3];
}

void arena_tick(Arena& arena) {
  ++arena.int4_world_tick;

  // 1. Activity 段（actor id 序）：Attack 活动（frontal 型）；attack_follow 型活动
  //    仅射程等待（无移动面）——不产生观察差异
  for (CombatUnit& unit : arena.vec_units) {
    if (!unit.b_in_world) continue;
    if (!unit.b_attack_follow) attack_activity_tick(arena, unit);
  }

  // 2. ITick 段（trait 注册序：Turreted → AttackFollow → Armament；Turreted 在
  //    IsAiming 时 no-op —— AttackFollow 先行设置）
  for (CombatUnit& unit : arena.vec_units) {
    if (!unit.b_in_world) continue;
    if (unit.b_attack_follow) attack_follow_tick(arena, unit);
  }
  for (CombatUnit& unit : arena.vec_units) {
    if (!unit.b_in_world) continue;
    armament_tick(unit.armament);
  }

  // 3. effects 段（列表序）
  effects_tick(arena);

  // 4. 帧尾排干（直到空——含执行中新增）
  while (!arena.deque_frame_end.empty()) {
    const FrameEndTask task{arena.deque_frame_end.front()};
    arena.deque_frame_end.pop_front();
    frame_end_run(arena, task);
  }
}

std::int32_t arena_sync_hash(const Arena& arena) {
  // World.SyncHash（World.cs:482-510）+ Sync.EmitSyncOpcodes 聚合语义（int32 wrap）。
  // ISync 槽序（探针实证 t12-sync 诊断）：p0/p1 Shroud(0) → 每 unit
  // Health(HP) → BodyOrientation(qf) → Mobile(Facing ^ FromCell.Bits ^ ToCell.Bits ^
  // CenterPosition——Sync.cs 属性序，from==to 时 Bits 抵消) → [Turreted(0)] →
  // Attack(IsAiming)。effects：Bullet(pos^lastPos^target^source)。
  std::int32_t int4_n{0};
  std::int32_t int4_ret{0};

  // actors 段：world(0)/p0(1)/p1(2) + units in world（id 序）
  const auto actor_term{[&](std::uint32_t u4_id) {
    int4_ret = wrap32(static_cast<std::int64_t>(int4_ret) +
                      static_cast<std::int64_t>(int4_n) * (1 + static_cast<std::int32_t>(u4_id)) *
                          static_cast<std::int32_t>(u4_id << 16));
    ++int4_n;
  }};
  actor_term(0);
  actor_term(1);
  actor_term(2);
  for (const CombatUnit& unit : arena.vec_units)
    if (unit.b_in_world) actor_term(unit.u4_id);

  // ISync 段（player Shroud 占位 ×2）
  ++int4_n;
  ++int4_n;

  for (const CombatUnit& unit : arena.vec_units) {
    if (!unit.b_in_world) continue;
    const auto sync_term{[&](std::int32_t int4_hash) {
      int4_ret = wrap32(static_cast<std::int64_t>(int4_ret) +
                        static_cast<std::int64_t>(int4_n) * (1 + static_cast<std::int32_t>(unit.u4_id)) *
                            int4_hash);
      ++int4_n;
    }};
    sync_term(unit.health.int4_hp);
    sync_term(unit.int4_quantized_facings);
    // Mobile（Sync.cs GetProperties 声明序：Facing/FromCell/ToCell/CenterPosition）
    sync_term(unit.int4_facing ^ cpos_bits(unit.cell_from) ^ cpos_bits(unit.cell_to) ^
              wpos_hash_code(unit.pos_center));
    if (unit.b_has_turret) sync_term(0);  // Turreted.QuantizedFacings 恒 0（默认面）
    sync_term(unit.b_is_aiming ? 1 : 0);
  }

  // syncedEffects（Bullet）
  for (const Effect& eff : arena.vec_effects) {
    if (eff.kind != Effect::Kind::kBullet) continue;
    const std::int32_t int4_hash{wpos_hash_code(eff.pos) ^ wpos_hash_code(eff.last_pos) ^
                                 wpos_hash_code(eff.target) ^ wpos_hash_code(eff.source)};
    int4_ret = wrap32(static_cast<std::int64_t>(int4_ret) +
                      static_cast<std::int64_t>(int4_n) * int4_hash);
    ++int4_n;
  }

  int4_ret = wrap32(static_cast<std::int64_t>(int4_ret) + arena.rng_shared.int4_last);
  return int4_ret;
}

std::string arena_effects_desc(const Arena& arena) {
  std::string str_out{std::format("{}", arena.vec_effects.size())};
  str_out += "[";
  bool b_first{true};
  for (const Effect& eff : arena.vec_effects) {
    if (!b_first) str_out += ",";
    b_first = false;
    str_out += eff.kind == Effect::Kind::kBullet ? "Bullet"
               : eff.kind == Effect::Kind::kDelayedImpact ? "DelayedImpact"
               : eff.kind == Effect::Kind::kContrailFader ? "ContrailFader"
                                                          : "InstantHit";
  }
  str_out += "]";
  return str_out;
}

std::string arena_dump_row(const Arena& arena, std::uint32_t u4_a, std::uint32_t u4_b) {
  const auto describe{[&](std::uint32_t u4_id) {
    const CombatUnit* ptr{arena_unit(arena, u4_id)};
    if (ptr == nullptr) return std::string("MISSING");
    const char* sz_inworld{ptr->b_in_world ? "True" : "False"};
    return std::format("hp {} face {} cell {},{} pos {},{},{} inworld {}",
                       ptr->b_in_world ? ptr->health.int4_hp : 0, ptr->int4_facing,
                       ptr->cell_from.int4_x, ptr->cell_from.int4_y, ptr->pos_center.int4_x,
                       ptr->pos_center.int4_y, ptr->pos_center.int4_z, sz_inworld);
  }};
  return std::format("tick {} a {} | b {} | eff {} rng {} cnt {} sync {}", arena.int4_world_tick,
                     describe(u4_a), describe(u4_b), arena_effects_desc(arena),
                     arena.rng_shared.int4_last, arena.rng_shared.int4_total_count,
                     arena_sync_hash(arena));
}

}  // namespace ora::game::combat
