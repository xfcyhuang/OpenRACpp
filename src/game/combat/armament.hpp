#pragma once

import std;

#include "game/combat/weapon.hpp"
#include "game/pathfinding/wangle.hpp"

namespace ora::game::combat {

struct CombatUnit;

// Armament trait 状态机最小面（OpenRA.Mods.Common/Traits/Armament.cs，T12/issue #13）。
// 运行时字段：FireDelay/Burst/ticksSinceLastShot/Recoil/currentBarrel。
struct ArmamentState {
  const WeaponDef* ptr_weapon{};
  std::vector<pathfinding::WVec> vec_barrel_offsets;  // Barrel.LocalOffset（弹匣轮换序）
  int int4_recoil_amount{};      // Info.Recoil.Length
  int int4_recoil_recovery{9};   // Info.RecoilRecovery.Length
  int int4_fire_delay_info{};    // Info.FireDelay（ScheduleDelayedAction 延迟；剧本面恒 0）
  int int4_recoil{};             // 运行时（WDist.Length）
  int int4_ticks_since_last_shot{};
  int int4_current_barrel{};
  int int4_fire_delay{};  // 运行时（IsReloading = FireDelay > 0）
  int int4_burst{};       // 运行时
};

// Armament.Tick（:211-236）：计数/FireDelay 递减/后坐恢复（ITick 段，开火驱动之后）
void armament_tick(ArmamentState& arm);

// Armament.CanFire（:252-268）——turret_desired_achieved 为炮塔门闸（AttackTurreted 单位）；
// b_target_valid = Weapon.IsValidAgainst（TargetTypes Overlap + CanTargetSelf，最小面）
[[nodiscard]] bool armament_can_fire(const CombatUnit& unit, const ArmamentState& arm,
                                     pathfinding::WPos pos_target, bool b_target_valid,
                                     bool b_turret_desired_achieved);

// Armament.CheckFire（:272-294）：burst do-while 连发（BurstDelays=0 同帧连发）。
// fn_fire = FireBarrel 回调（创建 projectile——由世界层注入，避免库内依赖 effects 容器）
[[nodiscard]] bool armament_check_fire(
    CombatUnit& unit, ArmamentState& arm, pathfinding::WPos pos_target, bool b_target_valid,
    bool b_turret_desired_achieved,
    const std::function<void(CombatUnit&, ArmamentState&, pathfinding::WPos pos_muzzle,
                             pathfinding::WPos pos_passive_target)>& fn_fire);

// Armament.CalculateMuzzleOffset（:401-418）：(offset - recoil) → 炮塔朝向旋转（含
// turret.Offset 的 body 量化旋转）→ body 量化朝向旋转 → LocalToWorld（ClassicPerspectiveFudge，
// CameraPitch 40°）。unit 无炮塔时仅 body 链。
[[nodiscard]] pathfinding::WVec armament_muzzle_offset(const CombatUnit& unit,
                                                       const ArmamentState& arm,
                                                       pathfinding::WVec vec_barrel);

// Armament.FireBarrel 的瞄准点（:296-363 最小面）：
//   TargetActorCenter ? target.CenterPosition : Positions.ClosestToIgnoringPath(muzzle)
// （HitShape 无 TargetableOffsets → Positions = [CenterPosition] → 恒 center）
[[nodiscard]] pathfinding::WPos armament_passive_target(const CombatUnit& unit,
                                                        const WeaponDef& weapon,
                                                        pathfinding::WPos pos_target_center);

}  // namespace ora::game::combat
