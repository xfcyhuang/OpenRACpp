// UPSTREAM: OpenRA.Mods.Common/Traits/Render/RenderUtils.cs @b6fc03f
//          L14-21 全文(ZOffsetFromCenter 单函数)
//          The whole of RenderUtils.cs L14-21 (the single ZOffsetFromCenter).
#pragma once

#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods::render {

/// ZOffsetFromCenter(L16-20):渲染 z 序的地物深度参考
/// ZOffsetFromCenter (L16-20): the terrain-depth reference for the render
/// z-order.
inline int ZOffsetFromCenter(sim::Actor& self, WPos pos, int offset) {
  const WVec delta = self.CenterPosition() - pos;
  return delta.Y + delta.Z + offset;
}

/// Actor.GetDamageState(Actor.cs L489-494 的渲染族消费点;无 IHealth =
/// Undamaged)
/// Actor.GetDamageState (Actor.cs L489-494's render-family consumption; no
/// IHealth = Undamaged).
inline sim::DamageState GetDamageState(sim::Actor& self) {
  auto* health = self.TraitOrDefault<sim::IHealth>();
  return health != nullptr ? health->DamageState()
                           : sim::DamageState::Undamaged;
}

}  // namespace ora::mods::render
