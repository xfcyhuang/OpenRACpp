#pragma once

import std;

#include "game/combat/combat_sim.hpp"
#include "game/combat/weapon.hpp"
#include "game/pathfinding/wangle.hpp"
#include "game/support/mersenne_twister.hpp"

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
