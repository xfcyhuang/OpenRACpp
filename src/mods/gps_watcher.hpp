#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

class GpsWatcher final : public TraitBase, public sim::ISync {
 public:
  GpsWatcher();

  ORA_TRAIT_INTERFACES(GpsWatcher, OpenRA_Mods_Cnc_Traits_GpsWatcher,
                       sim::ISync)

  bool Launched = false;
  bool GrantedAllies = false;
  bool Granted = false;
  bool Explored = false;
};

}
