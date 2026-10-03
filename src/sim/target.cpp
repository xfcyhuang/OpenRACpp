// UPSTREAM: OpenRA.Game/Traits/Target.cs @7d57605(实现部分:值语义核心)
//          The implementation half of Target.cs (the value-semantics core).
#include "sim/target.hpp"

#include "sim/actor.hpp"

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

}  // namespace ora::sim
