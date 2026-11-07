#include "game/combat/projectile.hpp"

#include "game/combat/warhead_damage.hpp"
#include "game/pathfinding/wangle.hpp"
#include "game/support/mersenne_twister.hpp"

namespace ora::game::combat {

std::int32_t wdist_from_pdf(support::MersenneTwister& rng, std::int32_t int4_samples) {

  std::int32_t int4_sum{0};
  for (std::int32_t idx{0}; idx < int4_samples; ++idx)
    int4_sum += support::next(rng, -1024, 1024);
  return int4_sum / int4_samples;
}

pathfinding::WVec wvec_from_pdf2(support::MersenneTwister& rng) {

  return pathfinding::WVec{wdist_from_pdf(rng, 2), wdist_from_pdf(rng, 2), 0};
}

pathfinding::WPos instant_hit_target_pos(const WeaponDef& weapon,
                                         pathfinding::WPos pos_passive) {

  if (weapon.projectile.int4_inaccuracy > 0)
    return pos_passive;
  return pos_passive;
}

pathfinding::WPos wpos_lerp_quadratic(pathfinding::WPos pos_a, pathfinding::WPos pos_b,
                                      std::int32_t int4_pitch, std::int32_t int4_mul,
                                      std::int32_t int4_div) {

  const pathfinding::WPos pos_lerped{pathfinding::wpos_lerp(pos_a, pos_b, int4_mul, int4_div)};
  if (int4_pitch == 0) return pos_lerped;

  const pathfinding::WVec vec_ab{pathfinding::wvec_sub(pos_b, pos_a)};
  const std::int64_t int8_length{pathfinding::isqrt64(pathfinding::wvec_length_squared(vec_ab))};
  const std::int32_t int4_tan{pathfinding::wangle_tan(int4_pitch)};
  const std::int64_t int8_num{int8_length * int4_tan * int4_mul * (int4_div - int4_mul)};
  const std::int64_t int8_den{1024LL * int4_div * int4_div};
  const std::int64_t int8_offset{int8_num / int8_den};

  const std::int64_t int8_z{int8_offset + pos_lerped.int4_z};
  const std::int64_t int8_clamped{int8_z < INT32_MIN ? INT32_MIN
                                  : int8_z > INT32_MAX ? INT32_MAX
                                                       : int8_z};
  return pathfinding::WPos{pos_lerped.int4_x, pos_lerped.int4_y,
                           static_cast<std::int32_t>(int8_clamped)};
}

Effect bullet_create(Arena& arena, const WeaponDef& weapon, pathfinding::WPos pos_source,
                     pathfinding::WPos pos_passive, std::uint32_t u4_owner) {

  Effect eff;
  eff.kind = Effect::Kind::kBullet;
  eff.u4_serial = arena.u4_next_effect_serial++;
  eff.pos = pos_source;
  eff.source = pos_source;
  eff.u4_owner = u4_owner;
  eff.ptr_weapon = &weapon;
  eff.pos_passive = pos_passive;

  if (weapon.projectile.vec_launch_angle.size() > 1)
    eff.int4_angle = support::next(arena.rng_shared, weapon.projectile.vec_launch_angle[0],
                                   weapon.projectile.vec_launch_angle[1]);
  else if (!weapon.projectile.vec_launch_angle.empty())
    eff.int4_angle = weapon.projectile.vec_launch_angle[0];

  eff.int4_speed = weapon.projectile.int4_speed;

  eff.target = pos_passive;
  if (weapon.projectile.int4_inaccuracy > 0) {

    const pathfinding::WVec vec_flight{pathfinding::wvec_sub(pos_passive, pos_source)};
    const std::int32_t int4_dist{
        static_cast<std::int32_t>(pathfinding::isqrt64(pathfinding::wvec_length_squared(vec_flight)))};
    const std::int32_t int4_max_inacc{weapon.projectile.int4_inaccuracy * int4_dist /
                                      weapon.int4_range};
    const pathfinding::WVec vec_pdf{wvec_from_pdf2(arena.rng_shared)};
    eff.target = pathfinding::WPos{
        eff.target.int4_x + vec_pdf.int4_x * int4_max_inacc / 1024,
        eff.target.int4_y + vec_pdf.int4_y * int4_max_inacc / 1024,
        eff.target.int4_z + vec_pdf.int4_z * int4_max_inacc / 1024};
  }

  const pathfinding::WVec vec_to_target{pathfinding::wvec_sub(eff.target, eff.pos)};
  eff.int4_length = static_cast<std::int32_t>(
                        pathfinding::isqrt64(pathfinding::wvec_length_squared(vec_to_target))) /
                    eff.int4_speed;
  if (eff.int4_length < 1) eff.int4_length = 1;

  return eff;
}

void warhead_do_impact(Arena& arena, const WarheadDef& wh, pathfinding::WPos pos_impact,
                       std::uint32_t u4_owner) {
  if (wh.kind == WarheadKind::kLeaveSmudge) return;

  if (wh.kind == WarheadKind::kCreateEffect) {

    enum class ImpactActorType : std::uint8_t { kNone, kValid, kInvalid };
    ImpactActorType impact_actor{ImpactActorType::kNone};
    bool b_any_invalid{false};
    for (const CombatUnit& unit : arena.vec_units) {
      if (!unit.b_in_world) continue;
      const pathfinding::WVec vec_delta{pathfinding::wvec_sub(pos_impact, unit.pos_center)};
      if (pathfinding::wvec_horizontal_length_squared(vec_delta) > 0) continue;
      const std::int32_t int4_edge{
          circle_distance_from_edge(unit.pos_center, pos_impact, unit.int4_hit_radius)};
      if (int4_edge > 0) continue;

      bool b_valid{true};
      for (const std::string& str_invalid : wh.vec_invalid_targets)
        for (const std::string& str_type : unit.vec_target_types)
          if (str_type == str_invalid) b_valid = false;
      bool b_overlap{false};
      for (const std::string& str_valid : wh.vec_valid_targets)
        for (const std::string& str_type : unit.vec_target_types)
          if (str_type == str_valid) b_overlap = true;
      if (b_overlap && b_valid) {
        impact_actor = ImpactActorType::kValid;
        break;
      }
      b_any_invalid = true;
    }
    if (impact_actor == ImpactActorType::kNone && b_any_invalid)
      impact_actor = ImpactActorType::kInvalid;
    if (impact_actor == ImpactActorType::kInvalid) return;
    if (impact_actor == ImpactActorType::kNone) {

      bool b_terrain{false};
      for (const std::string& str_valid : wh.vec_valid_targets)
        if (str_valid == "Ground") b_terrain = true;
      if (!b_terrain) return;
    }

    if (wh.int4_inaccuracy > 0) (void)wvec_from_pdf2(arena.rng_shared);
    return;
  }

  const std::int32_t int4_eff_last{
      wh.vec_falloff.empty() ? 0
                             : static_cast<std::int32_t>(wh.vec_falloff.size() - 1) * wh.int4_spread};
  const std::int64_t int8_eff_last_sq{static_cast<std::int64_t>(int4_eff_last) * int4_eff_last};

  for (CombatUnit& unit : arena.vec_units) {
    if (!unit.b_in_world) continue;
    const pathfinding::WVec vec_delta{pathfinding::wvec_sub(pos_impact, unit.pos_center)};
    if (pathfinding::wvec_horizontal_length_squared(vec_delta) > int8_eff_last_sq) continue;

    if (unit.u4_id == u4_owner) continue;
    bool b_overlaps{false};
    bool b_invalid_hit{false};
    for (const std::string& str_valid : wh.vec_valid_targets)
      for (const std::string& str_type : unit.vec_target_types)
        if (str_type == str_valid) b_overlaps = true;
    for (const std::string& str_invalid : wh.vec_invalid_targets)
      for (const std::string& str_type : unit.vec_target_types)
        if (str_type == str_invalid) b_invalid_hit = true;
    if (!b_overlaps || b_invalid_hit) continue;

    const std::int32_t int4_falloff_distance{
        circle_distance_from_edge(unit.pos_center, pos_impact, unit.int4_hit_radius)};
    if (int4_falloff_distance > int4_eff_last) continue;

    const std::int32_t int4_falloff{get_damage_falloff(wh.vec_falloff, wh.int4_spread,
                                                        int4_falloff_distance)};

    const std::int32_t int4_damage{
        warhead_inflict_damage(wh, std::vector<std::int32_t>{int4_falloff},
                               unit.str_armor_type)};
    const std::int32_t int4_applied{health_inflict(unit.health, DamagePacket{int4_damage}, {})};
    (void)int4_applied;

    if (unit.health.b_died_this_call) {

      unit.health.b_died_this_call = false;
      arena.deque_frame_end.push_back(
          FrameEndTask{.kind = FrameEndTask::Kind::kRemoveActor, .u4_actor = unit.u4_id});
    }
  }
}

void weapon_impact(Arena& arena, const WeaponDef& weapon, pathfinding::WPos pos_impact,
                   std::uint32_t u4_owner) {

  for (const WarheadDef& wh : weapon.vec_warheads) {
    if (wh.int4_delay > 0) {
      Effect eff;
      eff.kind = Effect::Kind::kDelayedImpact;
      eff.u4_serial = arena.u4_next_effect_serial++;
      eff.int4_delay = wh.int4_delay;
      eff.ptr_warhead = &wh;
      eff.pos_impact = pos_impact;
      eff.u4_impact_owner = u4_owner;
      FrameEndTask task;
      task.kind = FrameEndTask::Kind::kAddEffect;
      task.effect = eff;
      arena.deque_frame_end.push_back(std::move(task));
    } else {
      warhead_do_impact(arena, wh, pos_impact, u4_owner);
    }
  }
}

void effects_tick(Arena& arena) {

  for (std::size_t idx{0}; idx < arena.vec_effects.size(); ++idx) {
    Effect& eff{arena.vec_effects[idx]};
    if (eff.kind == Effect::Kind::kInstantHit) {

      weapon_impact(arena, *eff.ptr_weapon, eff.pos_impact, eff.u4_owner);
      arena.deque_frame_end.push_back(
          FrameEndTask{.kind = FrameEndTask::Kind::kRemoveEffect,
                       .u4_effect_serial = eff.u4_serial});
    } else if (eff.kind == Effect::Kind::kBullet) {

      eff.last_pos = eff.pos;
      eff.pos = wpos_lerp_quadratic(eff.source, eff.target, eff.int4_angle, eff.int4_ticks,
                                    eff.int4_length);
      const bool b_flight_length_reached{eff.int4_ticks >= eff.int4_length};
      ++eff.int4_ticks;

      if (b_flight_length_reached) {

        if (eff.ptr_weapon != nullptr && eff.ptr_weapon->projectile.int4_contrail_length > 0) {
          Effect fader;
          fader.kind = Effect::Kind::kContrailFader;
          fader.u4_serial = arena.u4_next_effect_serial++;
          fader.int4_length = eff.ptr_weapon->projectile.int4_contrail_length;
          fader.pos = eff.pos;
          FrameEndTask task;
          task.kind = FrameEndTask::Kind::kAddEffect;
          task.effect = fader;
          arena.deque_frame_end.push_back(std::move(task));
        }

        arena.deque_frame_end.push_back(
            FrameEndTask{.kind = FrameEndTask::Kind::kRemoveEffect,
                         .u4_effect_serial = eff.u4_serial});
        weapon_impact(arena, *eff.ptr_weapon, eff.pos, eff.u4_owner);
      }
    } else if (eff.kind == Effect::Kind::kDelayedImpact) {

      if (--eff.int4_delay <= 0) {
        arena.deque_frame_end.push_back(
            FrameEndTask{.kind = FrameEndTask::Kind::kDelayedDoImpact,
                         .u4_effect_serial = eff.u4_serial});
      }
    } else if (eff.kind == Effect::Kind::kContrailFader) {

      if (eff.int4_ticks++ == eff.int4_length) {
        arena.deque_frame_end.push_back(
            FrameEndTask{.kind = FrameEndTask::Kind::kRemoveEffect,
                         .u4_effect_serial = eff.u4_serial});
      }
    }
  }
}

}
