#include "game/combat/warhead_damage.hpp"

#include "game/pathfinding/wangle.hpp"

namespace ora::game::combat {

std::int32_t apply_percentage_modifiers(std::int32_t int4_number,
                                        const std::vector<std::int32_t>& vec_percentages) {
  // decimal 链：a = number; foreach p: a *= p/100m; return (int)a
  // （p/100m 是精确两位小数 → 全链等价于 number × ∏p / 100^n 的精确商，
  //   链末一次向零截断 —— 链内绝不能逐步 /100 截断）
  __int128 int8_value{int4_number};
  for (const std::int32_t int4_p : vec_percentages) int8_value *= int4_p;
  for (std::size_t idx{0}; idx < vec_percentages.size(); ++idx) int8_value /= 100;
  return static_cast<std::int32_t>(int8_value);
}

std::int32_t apply_percentage_modifiers(std::int32_t int4_number,
                                        std::initializer_list<std::int32_t> list_percentages) {
  return apply_percentage_modifiers(int4_number, std::vector<std::int32_t>{list_percentages});
}

std::int32_t int2_lerp(std::int32_t int4_a, std::int32_t int4_b, std::int32_t int4_mul,
                       std::int32_t int4_div) {
  return int4_a + (int4_b - int4_a) * int4_mul / int4_div;
}

std::int32_t get_damage_falloff(const std::vector<std::int32_t>& vec_falloff,
                                std::int32_t int4_spread, std::int32_t int4_distance) {
  // effectiveRange[i] = i * Spread（Range 空表派生，SpreadDamageWarhead.RulesetLoaded）
  std::int32_t int4_inner{0};
  for (std::size_t idx{1}; idx < vec_falloff.size(); ++idx) {
    const std::int32_t int4_outer{static_cast<std::int32_t>(idx) * int4_spread};
    if (int4_outer > int4_distance)
      return int2_lerp(vec_falloff[idx - 1], vec_falloff[idx], int4_distance - int4_inner,
                       int4_outer - int4_inner);
    int4_inner = int4_outer;
  }
  return 0;
}

std::int32_t damage_versus(const WarheadDef& wh, std::string_view str_armor_type) {
  if (wh.vec_versus.empty()) return 100;

  // victim 的全部启用 Armor trait 中 Versus 含其 Type 键的值序列（最小面：单 Armor）
  std::vector<std::int32_t> vec_values;
  for (const auto& [str_key, int4_value] : wh.vec_versus)
    if (str_key == str_armor_type) vec_values.push_back(int4_value);

  // 链 1：100 基数 decimal 链（DamageWarhead.DamageVersus）
  return apply_percentage_modifiers(100, vec_values);
}

std::int32_t warhead_inflict_damage(const WarheadDef& wh,
                                    const std::vector<std::int32_t>& vec_modifiers,
                                    std::string_view str_armor_type) {
  // 链 2：Damage × DamageModifiers（火力/falloff） × versus —— DamageVersus 结果
  // 是链 1 截断后的 int，再进本链（DamageWarhead.InflictDamage :86-90）
  std::vector<std::int32_t> vec_chain{vec_modifiers};
  vec_chain.push_back(damage_versus(wh, str_armor_type));
  return apply_percentage_modifiers(wh.int4_damage, vec_chain);
}

std::int32_t circle_distance_from_edge(pathfinding::WPos pos_origin, pathfinding::WPos pos_impact,
                                       std::int32_t int4_radius) {
  // CircleShape.DistanceFromEdge（无 VerticalTop/Bottom 偏移面）：Z 越界平移后同式；
  // 最小规则集 top/bottom 均 0 → 直接 2D
  const pathfinding::WVec vec_delta{pathfinding::wvec_sub(pos_impact, pos_origin)};
  const std::int64_t int8_length{pathfinding::isqrt64(pathfinding::wvec_length_squared(vec_delta))};
  const std::int64_t int8_dist{int8_length - int4_radius};
  return int8_dist > 0 ? static_cast<std::int32_t>(int8_dist) : 0;
}

}  // namespace ora::game::combat
