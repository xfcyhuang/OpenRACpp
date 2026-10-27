#include "sim/target.hpp"

#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::sim {

const std::vector<Target> Target::None = {};

Target Target::FromCell(const World& w, CPos c, SubCell sub_cell) {

  Target t;
  t.type = TargetType::Terrain;
  t.terrain_center_position =
      const_cast<World&>(w).Map().CenterOfSubCell(c, sub_cell);
  t.vec_terrain_positions = std::vector<WPos>{t.terrain_center_position};
  t.b_has_cell = true;
  t.cell = c;
  t.b_has_sub_cell = true;
  t.sub_cell = sub_cell;
  return t;
}

Target Target::FromActor(const Actor* a) {

  if (a == nullptr)
    return Invalid();
  Target t;
  t.type = TargetType::Actor;
  t.ActorPtr = a;
  t.generation = a->Generation();
  return t;
}

Target Target::FromSerializedActor(Actor* a, int actor_generation) {

  if (a == nullptr)
    return Invalid();
  Target t;
  t.type = TargetType::Actor;
  t.ActorPtr = a;
  t.generation = actor_generation;
  return t;
}

TargetType Target::Type() const {

  if (type == TargetType::Actor) {

    if (!ActorPtr->IsInWorld() || ActorPtr->IsDead())
      return TargetType::Invalid;

    if (ActorPtr->Generation() != generation)
      return TargetType::Invalid;
  }

  return type;
}

WPos Target::CenterPosition() const {

  switch (Type()) {
    case TargetType::Actor:
      return ActorPtr->CenterPosition();
    case TargetType::Terrain:
      return terrain_center_position;
    case TargetType::FrozenActor:
      return FrozenActorPtr->CenterPosition;
    case TargetType::Invalid:
    default:
      throw std::runtime_error(
          "Attempting to query the position of an invalid Target");
  }
}

const std::vector<WPos>& Target::Positions() const {

  static const std::vector<WPos> no_positions;
  switch (Type()) {
    case TargetType::Actor:

      if (!ActorPtr->EnabledTargetablePositions().empty())
        return ActorPtr->EnabledTargetableWorldPositions();

      {
        thread_local std::vector<WPos> actor_positions;
        actor_positions.clear();
        actor_positions.push_back(ActorPtr->CenterPosition());
        return actor_positions;
      }
    case TargetType::Terrain:
      return vec_terrain_positions;
    case TargetType::FrozenActor:

      if (FrozenActorPtr->IsValid())
        return FrozenActorPtr->TargetablePositions();
      return no_positions;
    case TargetType::Invalid:
    default:
      return no_positions;
  }
}

bool Target::IsInRange(const WPos& origin, const WDist& range) const {

  if (Type() == TargetType::Invalid)
    return false;

  for (const auto& t : Positions()) {
    const auto dx = static_cast<std::int64_t>(t.X - origin.X);
    const auto dy = static_cast<std::int64_t>(t.Y - origin.Y);
    if (dx * dx + dy * dy <= range.LengthSquared())
      return true;
  }
  return false;
}

int HashTarget(const Target& t) {

  switch (t.type) {
    case TargetType::Actor:
      return static_cast<int>(t.ActorPtr->ActorID() << 16) * 0x567;

    case TargetType::FrozenActor:

      if (t.FrozenActorPtr->ActorPtr() != nullptr)
        return static_cast<int>(
                   t.FrozenActorPtr->ActorPtr()->ActorID() << 16) * 0x567;
      return 0;

    case TargetType::Terrain:
      return sync::HashWPos(t.terrain_center_position);

    case TargetType::Invalid:
    default:
      return 0;
  }
}

bool Target::IsValidFor(const Actor* targeter) const {

  if (targeter == nullptr)
    return false;

  switch (Type()) {
    case TargetType::Actor:
      return ActorPtr->IsTargetableBy(const_cast<Actor&>(*targeter));
    case TargetType::FrozenActor:
      return FrozenActorPtr->IsValid() && FrozenActorPtr->Visible() &&
             !FrozenActorPtr->Hidden();
    case TargetType::Invalid:
      return false;
    case TargetType::Terrain:
    default:
      return true;
  }
}

bool Target::RequiresForceFire() const {

  if (ActorPtr == nullptr)
    return false;

  bool b_is_targetable = false;
  for (ITargetable* targetable : ActorPtr->Targetables()) {
    auto* targetable_base = dynamic_cast<TraitBase*>(targetable);
    if (targetable_base != nullptr && !targetable_base->IsTraitEnabled())
      continue;

    b_is_targetable = true;
    if (!targetable->RequiresForceFire())
      return false;
  }

  return b_is_targetable;
}

Target Target::Recalculate(const Player* viewer,
                           bool& b_target_is_hidden_actor) const {

  b_target_is_hidden_actor = false;

  if (Type() == TargetType::Invalid && ActorPtr != nullptr &&
      ActorPtr->ReplacedByActor() != nullptr)
    return FromActor(ActorPtr->ReplacedByActor());

  if (viewer->IsBot()) {

    if (Type() == TargetType::FrozenActor) {
      if (FrozenActorPtr->ActorPtr() != nullptr)
        return FromActor(FrozenActorPtr->ActorPtr());

      return Invalid();
    }

    return *this;
  }

  if (Type() == TargetType::Actor) {

    if (!ActorPtr->CanBeViewedByPlayer(const_cast<Player*>(viewer))) {

      FrozenActorLayer* layer = viewer->GetFrozenActorLayer();
      if (layer != nullptr) {
        FrozenActor* frozen = layer->FromID(ActorPtr->ActorID());
        if (frozen != nullptr)
          return FromFrozenActor(frozen);
      }

      b_target_is_hidden_actor = true;
      return *this;
    }
  } else if (Type() == TargetType::FrozenActor) {

    if (!FrozenActorPtr->Visible() || !FrozenActorPtr->IsValid()) {

      if (FrozenActorPtr->ActorPtr() != nullptr &&
          FrozenActorPtr->ActorPtr()->CanBeViewedByPlayer(
              const_cast<Player*>(viewer)))
        return FromActor(FrozenActorPtr->ActorPtr());

      return Invalid();
    }
  }

  return *this;
}

}
