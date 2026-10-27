#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct ScaredyCatInfoData {
  int int4_panic_chance = 40;
  int int4_attack_panic_chance = 20;
  std::string str_panic_sequence_prefix = "panic-";
  int int4_panic_speed_modifier = 150;
  std::vector<std::string> vec_avoid_terrain_types;

  static ScaredyCatInfoData Parse(const meta::RecordObject& rec_info);
};

class ScaredyCat final : public TraitBase, public sim::ISync {
 public:
  ScaredyCat(ScaredyCatInfoData&& info);

  ORA_TRAIT_INTERFACES(ScaredyCat, OpenRA_Mods_Common_Traits_ScaredyCat,
                       sim::ISync)

  int int4_panic_started_tick = 0;

  const ScaredyCatInfoData& Info() const { return info_; }

 private:
  ScaredyCatInfoData info_;
};

}
