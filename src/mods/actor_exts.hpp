#pragma once
import std;

#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods {

inline bool IsAtGroundLevel(sim::Actor& self) {
  if (self.OccupiesSpace() == nullptr)
    return false;

  if (!self.IsInWorld())
    return false;

  auto& map = self.world().Map();

  if (!map.Contains(self.Location()))
    return false;

  return map.DistanceAboveTerrain(self.CenterPosition()).Length == 0;
}

inline bool AppearsFriendlyTo(sim::Actor& self, sim::Actor& to_actor) {
  const sim::PlayerRelationship stance =
      to_actor.Owner()->RelationshipWith(self.Owner());
  return stance == sim::PlayerRelationship::Ally;
}

inline bool AppearsHostileTo(sim::Actor& self, sim::Actor& to_actor) {
  const sim::PlayerRelationship stance =
      to_actor.Owner()->RelationshipWith(self.Owner());
  if (stance == sim::PlayerRelationship::Ally)
    return false;

  if (self.EffectiveOwner() != nullptr && self.EffectiveOwner()->Disguised())
    return to_actor.Owner()->RelationshipWith(
               self.EffectiveOwner()->Owner()) ==
           sim::PlayerRelationship::Enemy;

  return stance == sim::PlayerRelationship::Enemy;
}

inline void NotifyBlocker(sim::Actor& self, CPos position) {
  for (sim::Actor* blocker :
       self.world().ActorMapFace()->GetActorsAt(position))
    for (auto* move_blocked :
         blocker->TraitsImplementing<sim::INotifyBlockingMove>())
      move_blocked->OnNotifyBlockingMove(*blocker, self);
}

inline void NotifyBlocker(sim::Actor& self,
                          std::span<const CPos> vec_positions) {
  for (const CPos position : vec_positions)
    for (sim::Actor* blocker :
         self.world().ActorMapFace()->GetActorsAt(position))
      for (auto* move_blocked :
           blocker->TraitsImplementing<sim::INotifyBlockingMove>())
        move_blocked->OnNotifyBlockingMove(*blocker, self);
}

inline WPos ClosestToIgnoringPath(const std::vector<WPos>& vec_positions,
                                  const WPos& position) {
  const WPos* best = nullptr;
  std::int64_t best_length = 0;
  for (const auto& p : vec_positions) {
    const auto dx = static_cast<std::int64_t>(p.X - position.X);
    const auto dy = static_cast<std::int64_t>(p.Y - position.Y);
    const std::int64_t length = dx * dx + dy * dy;
    if (best == nullptr || length < best_length) {
      best = &p;
      best_length = length;
    }
  }

  return *best;
}

inline CPos ClosestCell(sim::Actor& self, std::span<const CPos> vec_cells) {
  const CPos* best = nullptr;
  std::int64_t best_length = 0;
  for (const CPos& c : vec_cells) {
    const auto dx = static_cast<std::int64_t>(self.Location().X() - c.X());
    const auto dy = static_cast<std::int64_t>(self.Location().Y() - c.Y());
    const std::int64_t length = dx * dx + dy * dy;
    if (best == nullptr || length < best_length) {
      best = &c;
      best_length = length;
    }
  }
  return best != nullptr ? *best : CPos{};
}

inline core::Color OwnerColor(sim::Actor& self) {
  if (self.EffectiveOwner() != nullptr && self.EffectiveOwner()->Disguised() &&
      self.world().RenderPlayer() != nullptr)
    return self.EffectiveOwner()->Owner()->Color();
  return self.Owner()->Color();
}

}
