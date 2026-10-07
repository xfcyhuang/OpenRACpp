// UPSTREAM: OpenRA.Game/Traits/Target.cs @b6fc03f(实现部分:值语义核心)
//          The implementation half of Target.cs (the value-semantics core).
#include "sim/target.hpp"

#include "sim/actor.hpp"
#include "sim/player.hpp"

namespace ora::sim {

const std::vector<Target> Target::None = {};

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
  // L155-172(FrozenActor 中心位置 Phase 5 Shroud 链)
  switch (Type()) {
    case TargetType::Actor:
      return ActorPtr->CenterPosition();
    case TargetType::Terrain:
      return terrain_center_position;
    case TargetType::FrozenActor:
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
      // GetTargetablePositions(ITargetablePositions trait 面,Phase 5);
      // 本期回落中心位 —— COVERAGE 登记
      // GetTargetablePositions (the ITargetablePositions trait face,
      // Phase 5); falls back to the center meanwhile — in COVERAGE.
      // 不可变性:返回静态中心缓存会引入生命周期问题,此处按上游语义
      // 经 thread_local 单槽承载(调用方立即消费的约定不变)
      // thread_local single slot (callers consume immediately, matching
      // the upstream convention).
      // 注:中心位 = Actor.CenterPosition(IOccupySpace),Phase 3 直接计算
      {
        thread_local std::vector<WPos> actor_positions;
        actor_positions.clear();
        actor_positions.push_back(ActorPtr->CenterPosition());
        return actor_positions;
      }
    case TargetType::Terrain:
      return vec_terrain_positions;
    case TargetType::FrozenActor:
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
      // FrozenActor.Actor(Phase 5 Shroud 链);未接线时按上游 null 分支 → 0
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
      // FrozenActor 的 IsValid/Visible/Hidden 面随 Shroud 批(COVERAGE 登记;
      // 无实例 → 不可达分支)
      // The FrozenActor IsValid/Visible/Hidden faces ride the Shroud batch
      // (registered in COVERAGE; no instances → the unreachable branch).
      return false;
    case TargetType::Invalid:
      return false;
    case TargetType::Terrain:
    default:
      return true;
  }
}

Target Target::Recalculate(const Player* viewer,
                           bool& b_target_is_hidden_actor) const {
  // TargetExtensions.cs L31-81(Shroud/FrozenActorLayer 未移植的等价分支)
  // TargetExtensions.cs L31-81 (the equivalent branches under the
  // not-yet-ported Shroud/FrozenActorLayer).
  b_target_is_hidden_actor = false;

  // Check whether the target has transformed into something else
  // HACK: This relies on knowing the internal implementation details of Target
  if (Type() == TargetType::Invalid && ActorPtr != nullptr &&
      ActorPtr->ReplacedByActor() != nullptr)
    return FromActor(ActorPtr->ReplacedByActor());

  // Bot-controlled units aren't yet capable of understanding visibility changes
  if (viewer->IsBot()) {
    // Prevent that bot-controlled units endlessly fire at frozen actors.
    if (Type() == TargetType::FrozenActor) {
      // FrozenActor.Actor 面随 Shroud 批:上游 fa.Actor != null 时回退
      // FromActor,否则 Invalid —— 本批无实例,取 Invalid 分支(COVERAGE)
      // The FrozenActor.Actor face rides the Shroud batch: upstream falls
      // back to FromActor when fa.Actor != null, otherwise Invalid — no
      // instances this batch, so the Invalid branch (COVERAGE).
      return Invalid();
    }

    return *this;
  }

  if (Type() == TargetType::Actor) {
    // Actor has been hidden under the fog
    if (!ActorPtr->CanBeViewedByPlayer(const_cast<Player*>(viewer))) {
      // FrozenActorLayer.FromID 面随 Shroud 批:上游 frozen != null 时换
      // FromFrozenActor —— 本批无层,落 targetIsHiddenActor = true 分支
      // (COVERAGE 登记)
      // The FrozenActorLayer.FromID face rides the Shroud batch: upstream
      // swaps in FromFrozenActor when frozen != null — no layer this
      // batch, so the targetIsHiddenActor = true branch lands
      // (registered in COVERAGE).
      b_target_is_hidden_actor = true;
      return *this;
    }
  } else if (Type() == TargetType::FrozenActor) {
    // FrozenActor 可见性/换回 Actor 面随 Shroud 批;本批无实例,保持原样
    // (上游 Visible/IsValid 真分支)
    // The FrozenActor visibility / swap-back-to-Actor faces ride the Shroud
    // batch; no instances this batch, so the target passes through
    // (upstream's Visible/IsValid true branch).
    return *this;
  }

  return *this;
}

}  // namespace ora::sim
