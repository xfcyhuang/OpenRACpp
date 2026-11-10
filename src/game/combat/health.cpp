module ora.game.combat;

import std;

namespace ora::game::combat {

std::int32_t health_inflict(HealthState& hp, const DamagePacket& pkt_damage,
                            std::span<const std::int32_t> vec_victim_modifiers,
                            bool b_ignore_modifiers) {

  if (hp.b_dead) {
    hp.b_died_this_call = false;
    return 0;
  }

  std::int32_t int4_damage{pkt_damage.int4_value};
  if (!b_ignore_modifiers && int4_damage > 0) {

    __int128 int8_applied{int4_damage};
    for (const std::int32_t int4_modifier : vec_victim_modifiers) int8_applied *= int4_modifier;
    for (std::size_t idx{0}; idx < vec_victim_modifiers.size(); ++idx) int8_applied /= 100;
    int4_damage = static_cast<std::int32_t>(int8_applied);
  }

  const std::int32_t int4_old_hp{hp.int4_hp};
  hp.int4_hp = int4_old_hp - int4_damage;
  if (hp.int4_hp < 0) hp.int4_hp = 0;
  if (hp.int4_hp > hp.int4_max_hp) hp.int4_hp = hp.int4_max_hp;

  hp.b_dead = hp.int4_hp <= 0;
  hp.b_died_this_call = hp.int4_hp == 0;

  return int4_old_hp - hp.int4_hp;
}

std::int32_t health_kill(HealthState& hp) {
  return health_inflict(hp, DamagePacket{hp.int4_max_hp}, {}, true);
}

}
