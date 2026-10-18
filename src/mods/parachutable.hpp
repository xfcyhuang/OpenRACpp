// UPSTREAM: OpenRA.Mods.Common/Traits/Parachutable.cs @e180e3225
#pragma once
import std;

#include "core/bitset.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::TraitBase;

struct ParachutableInfoData {
  bool b_killed_on_impassable_terrain = true;
  core::BitSet<sim::DamageType> bitset_damage_types;
  std::string str_image{"explosion"};
  std::string str_ground_corpse_sequence;
  std::string str_ground_corpse_palette{"effect"};
  std::string str_ground_impact_sound;
  std::string str_water_corpse_sequence;
  std::string str_water_corpse_palette{"effect"};
  std::vector<std::string> vec_water_terrain_types{"Water"};
  std::string str_water_impact_sound;
  int int4_fall_rate = 13;
  std::string str_parachuting_condition;

  static ParachutableInfoData Parse(const meta::RecordObject& rec_info);
};

class Parachutable final : public TraitBase,
                           public sim::INotifyParachute {
 public:
  Parachutable(sim::Actor& self, const ParachutableInfoData& info);

  ORA_TRAIT_INTERFACES(Parachutable,
                       OpenRA_Mods_Common_Traits_Parachutable,
                       sim::INotifyParachute)

  const ParachutableInfoData& Info() const { return info_; }

  Actor* IgnoreActor = nullptr;

  bool IsInAir() const { return b_is_in_air_; }

  void OnParachute(sim::Actor& self) override;
  void OnLanded(sim::Actor& self) override;

 private:
  ParachutableInfoData info_;
  sim::IPositionable* ptr_positionable_ = nullptr;
  sim::Actor& self_;
  bool b_is_in_air_ = false;
  int int4_parachuting_token = sim::Actor::InvalidConditionToken;
};

}  // namespace ora::mods
