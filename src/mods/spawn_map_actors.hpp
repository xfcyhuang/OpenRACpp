#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class SpawnMapActors final : public sim::TraitBase,
                             public sim::IWorldLoaded {
 public:
  ORA_TRAIT_INTERFACES(SpawnMapActors,
                       OpenRA_Mods_Common_Traits_SpawnMapActors,
                       sim::IWorldLoaded)

  void WorldLoaded(sim::World& world,
                   gfx::WorldRenderer* wr) override;

  const std::map<std::string, sim::Actor*>& Actors() const {
    return map_actors_;
  }
  std::uint32_t LastMapActorID() const { return uint4_last_map_actor_id_; }

 private:
  std::map<std::string, sim::Actor*> map_actors_;
  std::uint32_t uint4_last_map_actor_id_ = 0;
};

void RegisterCommonActorInits();

}
