#pragma once

import std;

#include <initializer_list>

#include "game/combat/health.hpp"
#include "game/combat/weapon.hpp"
#include "game/pathfinding/wangle.hpp"

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
