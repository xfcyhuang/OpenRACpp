#pragma once

import std;

#include <initializer_list>

#include "game/combat/health.hpp"
#include "game/combat/weapon.hpp"
#include "game/pathfinding/wangle.hpp"

namespace ora::game::combat {

// 三条独立 decimal 取整链（Util.ApplyPercentageModifiers / DamageWarhead /
// SpreadDamageWarhead / Health 最小面，T12/issue #13）。链内 __int128 精确乘积、
// 链末一次向零截断——与 C# decimal（96 位尾数）在 int 输入域内逐值等价。
// 三条链绝不合并：链间是截断后的 int 传递（23000×70%×60% 型值合并计算会差 1）。

// Util.ApplyPercentageModifiers（Util.cs:203-211）：number × ∏(p/100)，链末 (int) 截断
[[nodiscard]] std::int32_t apply_percentage_modifiers(
    std::int32_t int4_number, std::initializer_list<std::int32_t> list_percentages);
[[nodiscard]] std::int32_t apply_percentage_modifiers(
    std::int32_t int4_number, const std::vector<std::int32_t>& vec_percentages);

// int2.Lerp（Primitives/int2.cs:77-80）：a + (b-a)*mul/div（int 乘除向零截断）
[[nodiscard]] std::int32_t int2_lerp(std::int32_t int4_a, std::int32_t int4_b,
                                     std::int32_t int4_mul, std::int32_t int4_div);

// SpreadDamageWarhead.GetDamageFalloff（:128-141）：首个 outer > distance 的区间
// Lerp(Falloff[i-1], Falloff[i], distance-inner, outer-inner)；超界 0。
// vec_falloff/spread 语义与上游一致（Range 空表 → effectiveRange[i] = i*Spread）。
[[nodiscard]] std::int32_t get_damage_falloff(const std::vector<std::int32_t>& vec_falloff,
                                              std::int32_t int4_spread,
                                              std::int32_t int4_distance);

// DamageWarhead.DamageVersus（:72-84）：Versus 空 → 100；否则 victim 装甲键命中序列
// 的 decimal 链（链 1）
[[nodiscard]] std::int32_t damage_versus(const WarheadDef& wh, std::string_view str_armor_type);

// DamageWarhead.InflictDamage（:86-90）：Damage × DamageModifiers(火力/falloff) ×
// versus（链 2）→ Health.InflictDamage（链 3 在 health 模块）
[[nodiscard]] std::int32_t warhead_inflict_damage(const WarheadDef& wh,
                                                  const std::vector<std::int32_t>& vec_modifiers,
                                                  std::string_view str_armor_type);

// Circle HitShape.DistanceFromEdge（HitShapes/Circle.cs）：max(0, |pos-origin| - radius)
[[nodiscard]] std::int32_t circle_distance_from_edge(pathfinding::WPos pos_origin,
                                                     pathfinding::WPos pos_impact,
                                                     std::int32_t int4_radius);

}  // namespace ora::game::combat
