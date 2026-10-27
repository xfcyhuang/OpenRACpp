#pragma once
import std;

#include "core/percent_modifiers.hpp"
#include "mods/player_resources.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::Player;
using sim::TraitBase;

struct RefineryInfoData {
  bool b_use_storage = true;
  bool b_discard_excess_resources = false;
  bool b_show_ticks = true;
  int int4_tick_rate = 10;

  static RefineryInfoData Parse(const meta::RecordObject& rec_info);
};

class Refinery final : public TraitBase,
                       public sim::IAcceptResources,
                       public sim::INotifyCreated,
                       public sim::ITick,
                       public sim::INotifyOwnerChanged {
 public:
  Refinery(ActorInitializer& init, const RefineryInfoData& info);

  ORA_TRAIT_INTERFACES(Refinery, OpenRA_Mods_Common_Traits_Refinery,
                       sim::IAcceptResources, sim::INotifyCreated,
                       sim::ITick, sim::INotifyOwnerChanged)

  int AcceptResources(Actor& self, const std::string& resource_type,
                      int count = 1) override;

  void Created(Actor& self) override;
  void Tick(Actor& self) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

 private:
  RefineryInfoData info_;
  PlayerResources* p_player_resources_;
  std::vector<int> vec_resource_value_modifiers_;

  int int4_current_display_tick_ = 0;
  int int4_current_display_value_ = 0;
};

}
