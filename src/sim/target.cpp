// UPSTREAM: OpenRA.Game/Traits/Target.cs @b6fc03f(实现部分:值语义核心)
//          The implementation half of Target.cs (the value-semantics core).
#include "sim/target.hpp"

#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::sim {

const std::vector<Target> Target::None = {};

Target Target::FromCell(const World& w, CPos c, SubCell sub_cell) {
  // L87-88 / L46-57
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
  // L88
  if (a == nullptr)
    return Invalid();
  Target t;
  t.type = TargetType::Actor;
  t.ActorPtr = a;
  t.generation = a->Generation();
  return t;
}

Target Target::FromSerializedActor(Actor* a, int actor_generation) {
  // L290
  if (a == nullptr)
    return Invalid();
  Target t;
  t.type = TargetType::Actor;
  t.ActorPtr = a;
  t.generation = actor_generation;
  return t;
}

TargetType Target::Type() const {
  // L91-107
  if (type == TargetType::Actor) {
    // Actor is no longer in the world
    if (!ActorPtr->IsInWorld() || ActorPtr->IsDead())
      return TargetType::Invalid;

    // Actor generation has changed (teleported or captured)
    if (ActorPtr->Generation() != generation)
      return TargetType::Invalid;
  }

  return type;
}

WPos Target::CenterPosition() const {
  // L155-172
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
  // L175-194
  static const std::vector<WPos> no_positions;
  switch (Type()) {
    case TargetType::Actor:
      // GetTargetablePositions(ITargetablePositions trait 面;HitShape 批
      // 接线)。空集(无 HitShape)回落中心位 —— 上游 [CenterPosition] 同
      // 值;thread_local 单槽承载(调用方立即消费的约定不变)
      // GetTargetablePositions (the ITargetablePositions trait face; wired
      // with the HitShape batch). The empty set (no HitShape) falls back to
      // the center — upstream's [CenterPosition] value; carried by a
      // thread_local single slot (callers consume immediately, the
      // unchanged convention).
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
      // TargetablePositions may be null if it is Invalid(上游注释)
      if (FrozenActorPtr->IsValid())
        return FrozenActorPtr->TargetablePositions();
      return no_positions;
    case TargetType::Invalid:
    default:
      return no_positions;
  }
}

bool Target::IsInRange(const WPos& origin, const WDist& range) const {
  // L196-203:射程判定(2D 距离;HorizontalLengthSquared)
  if (Type() == TargetType::Invalid)
    return false;

  // Target ranges are calculated in 2D, so ignore height differences
  for (const auto& t : Positions()) {
    const auto dx = static_cast<std::int64_t>(t.X - origin.X);
    const auto dy = static_cast<std::int64_t>(t.Y - origin.Y);
    if (dx * dx + dy * dy <= range.LengthSquared())
      return true;
  }
  return false;
}

int HashTarget(const Target& t) {
  // Sync.cs L138-159
  switch (t.type) {
    case TargetType::Actor:
      return static_cast<int>(t.ActorPtr->ActorID() << 16) * 0x567;

    case TargetType::FrozenActor:
      // FrozenActor.Actor != null 时回退 FromActor,否则 Invalid(Sync.cs
      // L138-159 的 fa.Actor 分支)
      // Fall back to FromActor when FrozenActor.Actor != null, otherwise
      // Invalid (Sync.cs L138-159's fa.Actor branch).
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
  // L110-125
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
  // L131-152:全有或全无
  // L131-152: all or nothing.
  if (ActorPtr == nullptr)
    return false;

  // PERF: Avoid LINQ.(上游注释)
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
  // TargetExtensions.cs L31-81(全分支;FrozenActorLayer 已就位)
  // TargetExtensions.cs L31-81 (every branch; the FrozenActorLayer is in
  // place).
  b_target_is_hidden_actor = false;

  // Check whether the target has transformed into something else
  // HACK: This relies on knowing the internal implementation details of
  // Target(上游注释)
  if (Type() == TargetType::Invalid && ActorPtr != nullptr &&
      ActorPtr->ReplacedByActor() != nullptr)
    return FromActor(ActorPtr->ReplacedByActor());

  // Bot-controlled units aren't yet capable of understanding visibility
  // changes(上游注释)
  if (viewer->IsBot()) {
    // Prevent that bot-controlled units endlessly fire at frozen actors.
    // (上游注释)
    if (Type() == TargetType::FrozenActor) {
      if (FrozenActorPtr->ActorPtr() != nullptr)
        return FromActor(FrozenActorPtr->ActorPtr());

      // Original actor was killed
      return Invalid();
    }

    return *this;
  }

  if (Type() == TargetType::Actor) {
    // Actor has been hidden under the fog
    if (!ActorPtr->CanBeViewedByPlayer(const_cast<Player*>(viewer))) {
      // Replace with FrozenActor if applicable, otherwise return target
      // unmodified(上游注释)
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
    // Frozen actor has been revealed
    if (!FrozenActorPtr->Visible() || !FrozenActorPtr->IsValid()) {
      // Original actor is still alive
      if (FrozenActorPtr->ActorPtr() != nullptr &&
          FrozenActorPtr->ActorPtr()->CanBeViewedByPlayer(
              const_cast<Player*>(viewer)))
        return FromActor(FrozenActorPtr->ActorPtr());

      // Original actor was killed while hidden
      return Invalid();
    }
  }

  return *this;
}

}  // namespace ora::sim
