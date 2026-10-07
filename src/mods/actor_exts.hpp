// UPSTREAM: OpenRA.Mods.Common/ActorExts.cs @b6fc03f L21-83(逐语义重写;
//          AppearsHostileTo 已随第四批落地;ClosestCell 随消费批)+ WorldUtils.cs L26-45
//          的 ClosestToIgnoringPath 位置面
//          Verbatim-semantics rewrite (AppearsHostileTo/ClosestCell land
//          with their consumers) + the ClosestToIgnoringPath position face
//          of WorldUtils.cs L26-45.
#pragma once
import std;

#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods {

/// ActorExts.cs L21-33:IsAtGroundLevel
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

/// ActorExts.cs L35-44:AppearsFriendlyTo(EffectiveOwner 伪装面:上游
/// Disguised 分支 —— 伪装 trait 未移植,IEffectiveOwner 缓存恒空,落
/// stance==Ally 真值;COVERAGE 登记)
/// ActorExts.cs L35-44: AppearsFriendlyTo (the EffectiveOwner disguise
/// face: upstream's Disguised branch — the disguise traits are unported,
/// the IEffectiveOwner cache stays empty, so the stance==Ally truth lands;
/// registered in COVERAGE).
inline bool AppearsFriendlyTo(sim::Actor& self, sim::Actor& to_actor) {
  const sim::PlayerRelationship stance =
      to_actor.Owner()->RelationshipWith(self.Owner());
  return stance == sim::PlayerRelationship::Ally;
}

/// ActorExts.cs L46-55:AppearsHostileTo(EffectiveOwner 伪装面同
/// AppearsFriendlyTo 的空集等价;IgnoresDisguise trait 未移植 ——
/// COVERAGE 登记)
/// ActorExts.cs L46-55: AppearsHostileTo (the EffectiveOwner disguise
/// face keeps the same empty-set equivalence as AppearsFriendlyTo; the
/// IgnoresDisguise trait is unported — registered in COVERAGE).
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

/// ActorExts.cs L61-75:NotifyBlocker(position)—— GetActorsAt 全员
/// INotifyBlockingMove 回调
/// ActorExts.cs L61-75: NotifyBlocker(position) — the INotifyBlockingMove
/// callbacks over everyone GetActorsAt returns.
inline void NotifyBlocker(sim::Actor& self, CPos position) {
  for (sim::Actor* blocker :
       self.world().ActorMapFace()->GetActorsAt(position))
    for (auto* move_blocked :
         blocker->TraitsImplementing<sim::INotifyBlockingMove>())
      move_blocked->OnNotifyBlockingMove(*blocker, self);
}

/// WorldUtils.cs L35-45:ClosestToIgnoringPath(IEnumerable<WPos>, WPos)
/// (MinBy 的水平距离平方;平局取首)
/// WorldUtils.cs L35-45: ClosestToIgnoringPath(IEnumerable<WPos>, WPos)
/// (MinBy on horizontal distance squared; first wins on ties).
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

  // 上游空集抛 MinBy 的 InvalidOperationException("Sequence contains no
  // elements")—— 消费面(Target.Positions 恒非空)不可达;空集即未定义
  // Upstream's empty set throws MinBy's InvalidOperationException
  // ("Sequence contains no elements") — unreachable at the consumption
  // face (Target.Positions is never empty); an empty set is undefined.
  return *best;
}

}  // namespace ora::mods
