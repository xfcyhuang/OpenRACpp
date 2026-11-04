#pragma once

import std;

namespace ora::game::combat {

// Health trait 最小面（Traits/Health.cs，T12/issue #13）。
// InflictDamage 第三条 decimal 链（victim 的 IDamageModifier 序列）+ clamp +
// overkill 保护 + 死亡标志（HP==0 → RemoveOnDeath → Dispose → 帧尾移除由世界层消费）。

struct DamagePacket {
  std::int32_t int4_value{};
};

struct HealthState {
  std::int32_t int4_hp{};
  std::int32_t int4_max_hp{};
  bool b_dead{};           // IsDead（HP <= 0）
  bool b_died_this_call{}; // 本次调用触发 HP==0（INotifyKilled + Dispose 时机）
};

// Health.InflictDamage（:158-226）：
//   IsDead → return 0（overkill 保护，不重复触发击杀）
//   damage>0 且非 ignoreModifiers：victim 修饰 decimal 链（链 3）末一次截断
//   HP = clamp(HP - damage, 0, MaxHP)；HP==0 → 死亡标志
// 返回实际扣血量（= min(链 3 后伤害, HP)）。
[[nodiscard]] std::int32_t health_inflict(HealthState& hp, const DamagePacket& pkt_damage,
                                          std::span<const std::int32_t> vec_victim_modifiers,
                                          bool b_ignore_modifiers = false);

// Health.Kill（:228-231）：InflictDamage(MaxHP, ignoreModifiers: true)
[[nodiscard]] std::int32_t health_kill(HealthState& hp);

}  // namespace ora::game::combat
