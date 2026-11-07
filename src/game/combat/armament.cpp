#include "game/combat/armament.hpp"

#include "game/combat/combat_sim.hpp"
#include "game/pathfinding/wangle.hpp"

namespace ora::game::combat {

namespace {

std::int32_t quantize_facing(std::int32_t int4_facing, std::int32_t int4_facings) {
  if (int4_facings == 0) return int4_facing;
  const std::int32_t int4_step{1024 / int4_facings};
  return ((int4_facing + int4_step / 2) & 1023) / int4_step * int4_step;
}

pathfinding::WVec local_to_world(pathfinding::WVec vec) {
  using pathfinding::wangle_sin;
  return pathfinding::WVec{vec.int4_y, -wangle_sin(113) * vec.int4_x / 1024, vec.int4_z};
}

}

void armament_tick(ArmamentState& arm) {

  if (arm.int4_ticks_since_last_shot < arm.ptr_weapon->int4_reload_delay)
    ++arm.int4_ticks_since_last_shot;
  if (arm.int4_fire_delay > 0) --arm.int4_fire_delay;
  arm.int4_recoil -= arm.int4_recoil_recovery;
  if (arm.int4_recoil < 0) arm.int4_recoil = 0;
}

bool armament_can_fire(const CombatUnit& unit, const ArmamentState& arm,
                       pathfinding::WPos pos_target, bool b_target_valid,
                       bool b_turret_desired_achieved) {
  if (arm.int4_fire_delay > 0) return false;
  if (unit.b_has_turret && !b_turret_desired_achieved) return false;

  const pathfinding::WVec vec_delta{pathfinding::wvec_sub(pos_target, unit.pos_center)};
  const std::int64_t int8_dist_sq{pathfinding::wvec_horizontal_length_squared(vec_delta)};

  const std::int64_t int8_range_sq{
      static_cast<std::int64_t>(arm.ptr_weapon->int4_range) * arm.ptr_weapon->int4_range};
  const std::int64_t int8_min_sq{
      static_cast<std::int64_t>(arm.ptr_weapon->int4_min_range) * arm.ptr_weapon->int4_min_range};
  if (int8_dist_sq > int8_range_sq) return false;
  if (arm.ptr_weapon->int4_min_range != 0 && int8_dist_sq <= int8_min_sq) return false;

  return b_target_valid;
}

bool armament_check_fire(
    CombatUnit& unit, ArmamentState& arm, pathfinding::WPos pos_target, bool b_target_valid,
    bool b_turret_desired_achieved,
    const std::function<void(CombatUnit&, ArmamentState&, pathfinding::WPos pos_muzzle,
                             pathfinding::WPos pos_passive_target)>& fn_fire) {
  if (!armament_can_fire(unit, arm, pos_target, b_target_valid, b_turret_desired_achieved))
    return false;

  if (arm.int4_ticks_since_last_shot >= arm.ptr_weapon->int4_reload_delay)
    arm.int4_burst = arm.ptr_weapon->int4_burst;

  arm.int4_ticks_since_last_shot = 0;
  do {

    const std::size_t sz_barrels{arm.vec_barrel_offsets.empty() ? 1 : arm.vec_barrel_offsets.size()};
    const std::size_t idx_barrel{arm.ptr_weapon->int4_burst == 1
                                     ? static_cast<std::size_t>(arm.int4_current_barrel) % sz_barrels
                                     : static_cast<std::size_t>(arm.int4_burst) % sz_barrels};
    ++arm.int4_current_barrel;

    const pathfinding::WVec vec_barrel{
        arm.vec_barrel_offsets.empty() ? pathfinding::WVec{} : arm.vec_barrel_offsets[idx_barrel]};
    const pathfinding::WVec vec_muzzle{armament_muzzle_offset(unit, arm, vec_barrel)};
    const pathfinding::WPos pos_muzzle_final{
        unit.pos_center.int4_x + vec_muzzle.int4_x, unit.pos_center.int4_y + vec_muzzle.int4_y,
        unit.pos_center.int4_z + vec_muzzle.int4_z};

    const pathfinding::WPos pos_passive{
        armament_passive_target(unit, *arm.ptr_weapon, pos_target)};
    fn_fire(unit, arm, pos_muzzle_final, pos_passive);

    if (--arm.int4_burst > 0) {
      arm.int4_fire_delay =
          arm.ptr_weapon->vec_burst_delays.size() == 1
              ? arm.ptr_weapon->vec_burst_delays[0]
              : arm.ptr_weapon->vec_burst_delays[static_cast<std::size_t>(
                    arm.ptr_weapon->int4_burst - (arm.int4_burst + 1))];
    } else {

      arm.int4_fire_delay = arm.ptr_weapon->int4_reload_delay;
      if (arm.int4_fire_delay <= 0) arm.int4_fire_delay = 1;
      arm.int4_burst = arm.ptr_weapon->int4_burst;
    }
  } while (arm.int4_fire_delay == 0 &&
           armament_can_fire(unit, arm, pos_target, b_target_valid, b_turret_desired_achieved));

  return true;
}

pathfinding::WVec armament_muzzle_offset(const CombatUnit& unit, const ArmamentState& arm,
                                         pathfinding::WVec vec_barrel) {

  pathfinding::WVec vec_local{vec_barrel.int4_x - arm.int4_recoil, vec_barrel.int4_y,
                              vec_barrel.int4_z};

  const std::int32_t int4_body_quantized{quantize_facing(unit.int4_facing, unit.int4_quantized_facings)};

  if (unit.b_has_turret) {

    const std::int32_t int4_turret_world{pathfinding::wangle_wrap(unit.int4_turret_yaw)};
    vec_local = pathfinding::wvec_rotate_yaw(vec_local, int4_turret_world);

  } else {
    vec_local = pathfinding::wvec_rotate_yaw(vec_local, int4_body_quantized);
  }

  return local_to_world(vec_local);
}

pathfinding::WPos armament_passive_target(const CombatUnit& unit, const WeaponDef& weapon,
                                          pathfinding::WPos pos_target_center) {

  (void)unit;
  (void)weapon;
  return pos_target_center;
}

}
