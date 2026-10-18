// UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/ParatroopersPower.cs
//          @e180e3225
#pragma once
import std;

#include "core/wdist.hpp"
#include "core/wvec.hpp"
#include "meta/generic_record.hpp"
#include "mods/airstrike_power.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

struct ParatroopersPowerInfoData : DirectionalSupportPowerInfoData {
  std::string str_unit_type{"badr"};
  int int4_squad_size = 1;
  WVec wvec_squad_offset{-1536, 1536, 0};
  std::string str_reinforcements_arrived_speech_notification;
  std::string str_reinforcements_arrived_text_notification;
  int int4_quantized_facings = 32;
  WDist dist_cordon{5120};
  std::vector<std::string> vec_drop_items;
  bool b_allow_impassable_cells = false;
  std::string str_camera_actor;
  int int4_camera_remove_delay = 85;
  WDist dist_beacon_distance_offset = WDist::FromCells(4);

  static ParatroopersPowerInfoData Parse(
      const meta::RecordObject& rec_info);
};

class ParatroopersPower final : public DirectionalSupportPower {
 public:
  ParatroopersPower(const ActorInitializer& init,
                    const ParatroopersPowerInfoData& info);

  void Activate(Actor& self, const net::Order& order,
                SupportPowerManager& manager) override;

  std::pair<std::vector<Actor*>, std::vector<Actor*>> SendParatroopers(
      Actor& self, WPos target, std::optional<WAngle> facing);

 private:
  void RemoveCamera(Actor* camera);
  void RemoveBeacon(class Beacon* beacon);

  ParatroopersPowerInfoData paratroopers_info_;
};

}  // namespace ora::mods
