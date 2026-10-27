#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

using TraitBase = sim::TraitBase;

class DebugPauseState final : public TraitBase, public ISync {
 public:
  explicit DebugPauseState(World& world);

  ORA_TRAIT_INTERFACES(DebugPauseState, OpenRA_Traits_DebugPauseState,
                       sim::ISync)

  bool IsWorldPaused() const;

 private:
  World& world_ref_;
};

}
