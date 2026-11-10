#pragma once

import std;

#include "game/combat/weapon.hpp"
#include "game/pathfinding/wangle.hpp"

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
