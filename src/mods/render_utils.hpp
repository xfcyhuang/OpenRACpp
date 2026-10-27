#pragma once

#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods::render {

inline int ZOffsetFromCenter(sim::Actor& self, WPos pos, int offset) {
  const WVec delta = self.CenterPosition() - pos;
  return delta.Y + delta.Z + offset;
}

inline sim::DamageState GetDamageState(sim::Actor& self) {
  auto* health = self.TraitOrDefault<sim::IHealth>();
  return health != nullptr ? health->DamageState()
                           : sim::DamageState::Undamaged;
}

}
