#pragma once

import std;

namespace ora::game::combat {

struct DamagePacket {
  std::int32_t int4_value{};
};

struct HealthState {
  std::int32_t int4_hp{};
  std::int32_t int4_max_hp{};
  bool b_dead{};
  bool b_died_this_call{};
};

[[nodiscard]] std::int32_t health_inflict(HealthState& hp, const DamagePacket& pkt_damage,
                                          std::span<const std::int32_t> vec_victim_modifiers,
                                          bool b_ignore_modifiers = false);

[[nodiscard]] std::int32_t health_kill(HealthState& hp);

}
