// UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackBase.cs 实现部分
//          + AttackFrontal.cs | The implementation half.
#include "mods/attack_base.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/attack_activity.hpp"
#include "mods/mobile.hpp"
#include "net/order.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "game/actor_info.hpp"
#include "sim/trait_registry.hpp"
#include "sim/weapons.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

// ———— AttackBaseInfoData::Parse(L25-71)————

AttackBaseInfoData AttackBaseInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AttackBaseInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "Armaments") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          data.vec_armaments.clear();
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              data.vec_armaments.push_back(*s);
        }
      } else if (name == "Cursor") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_cursor = *s;
      } else if (name == "OutsideRangeCursor") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_outside_range_cursor = *s;
      } else if (name == "TargetLineColor") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.target_line_color = core::Color::FromArgbRaw(
              static_cast<std::uint32_t>(*n));
      } else if (name == "AttackRequiresEnteringCell") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_attack_requires_entering_cell = *b;
      } else if (name == "TargetFrozenActors") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_target_frozen_actors = *b;
      } else if (name == "ForceFireIgnoresActors") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_force_fire_ignores_actors = *b;
      } else if (name == "OutsideRangeRequiresForceFire") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_outside_range_requires_force_fire = *b;
      } else if (name == "Voice") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_voice = *s;
      } else if (name == "FacingTolerance") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.angle_facing_tolerance =
              WAngle{static_cast<std::int32_t>(*n)};
      } else if (name == "TargetTerrainWithoutForceFire") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_target_terrain_without_force_fire = *b;
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);

  // RulesetLoaded(L62-68)的 FacingTolerance 校验 —— 工厂时点
  // RulesetLoaded's (L62-68) FacingTolerance validation — at factory time.
  if (data.angle_facing_tolerance.Angle > 512)
    throw yaml::YamlException(
        "Facing tolerance must be in range of [0, 512], 512 covers 360 "
        "degrees.");

  return data;
}

// ———— AttackBaseCore(L73-525)————

template <class Derived>
AttackBaseCore<Derived>::AttackBaseCore(Actor& self,
                                        const AttackBaseInfoData& info)
    : sim::ConditionalTraitCore<Derived>(info.conditional),
      info_{info},
      p_self_{&self} {}

template <class Derived>
void AttackBaseCore<Derived>::CoreAttackBaseCreated(Actor& self) {
  // L98-107
  facing_ = self.TraitOrDefault<sim::IFacing>();
  positionable_ =
      dynamic_cast<sim::IPositionable*>(self.OccupiesSpace());
  vec_notify_aiming_ = self.TraitsImplementing<sim::INotifyAiming>();

  // L126-132:InitializeGetArmaments(过滤物化 —— 上游闭包的 ToArray 同值)
  // L126-132: InitializeGetArmaments (the filtered materialization — the
  // same value as upstream's closure's ToArray).
  vec_armaments_.clear();
  for (Armament* a : self.TraitsImplementing<Armament>())
    if (std::find(info_.vec_armaments.begin(), info_.vec_armaments.end(),
                  a->InfoData().str_name) != info_.vec_armaments.end())
      vec_armaments_.push_back(a);
}

template <class Derived>
void AttackBaseCore<Derived>::Tick(Actor& self) {
  // L109-124(protected Tick 的 ITick 面)
  if (!b_was_aiming_ && b_is_aiming)
    for (auto* n : vec_notify_aiming_)
      n->StartedAiming(self, static_cast<TraitBase*>(static_cast<Derived*>(
                                 this)));
  else if (b_was_aiming_ && !b_is_aiming)
    for (auto* n : vec_notify_aiming_)
      n->StoppedAiming(self, static_cast<TraitBase*>(
                                 static_cast<Derived*>(this)));

  b_was_aiming_ = b_is_aiming;
}

template <class Derived>
bool AttackBaseCore<Derived>::TargetInFiringArc(Actor& self,
                                                const sim::Target& target,
                                                WAngle facing_tolerance) {
  // L134-147
  if (facing_ == nullptr)
    return true;

  const WPos pos = self.CenterPosition();
  const WPos targeted_position = GetTargetPosition(pos, target);
  const WVec delta = targeted_position - pos;

  if (delta.HorizontalLengthSquared() == 0)
    return true;

  return FacingWithinTolerance(facing_->Facing(), delta.Yaw(),
                               facing_tolerance);
}

template <class Derived>
bool AttackBaseCore<Derived>::CanAttack(Actor& self,
                                        const sim::Target& target) {
  // L149-165
  if (!self.IsInWorld() ||
      sim::ConditionalTraitCore<Derived>::IsTraitDisabled() ||
      sim::ConditionalTraitCore<Derived>::IsTraitPaused())
    return false;

  if (!target.IsValidFor(&self))
    return false;

  if (!HasAnyValidWeapons(target, false, true))
    return false;

  // PERF: Mobile implements IPositionable, so we can use 'as' to save a
  // trait look-up here.(上游注释)
  if (auto* mobile = dynamic_cast<Mobile*>(positionable_);
      mobile != nullptr && !mobile->CanInteractWithGroundLayer(&self))
    return false;

  return true;
}

template <class Derived>
void AttackBaseCore<Derived>::DoAttack(Actor& self,
                                       const sim::Target& target) {
  // L167-174
  if (!CanAttack(self, target))
    return;

  for (Armament* a : vec_armaments_)
    a->CheckFire(&self, facing_, target);
}

template <class Derived>
std::vector<sim::IOrderTargeter*> AttackBaseCore<Derived>::Orders() {
  // L176-188
  std::vector<sim::IOrderTargeter*> vec_out;
  if (sim::ConditionalTraitCore<Derived>::IsTraitDisabled())
    return vec_out;

  if (vec_armaments_.empty())
    return vec_out;

  // 上游每次枚举 new AttackOrderTargeter(this, 6)(OrderID 的 ForceAttack
  // 改名不跨枚举泄漏)—— 每调用重建实例
  // Upstream constructs a new AttackOrderTargeter(this, 6) per enumeration
  // (the ForceAttack OrderID rename never leaks across enumerations) — a
  // fresh instance per call.
  vec_order_targeters_.clear();
  vec_order_targeters_.push_back(
      std::make_unique<AttackOrderTargeter>(*this, 6));
  vec_out.push_back(vec_order_targeters_[0].get());
  return vec_out;
}

template <class Derived>
net::Order* AttackBaseCore<Derived>::IssueOrder(
    Actor& self, sim::IOrderTargeter* order, const sim::Target& target,
    bool queued) {
  // L190-196
  if (dynamic_cast<AttackOrderTargeter*>(order) != nullptr)
    return new net::Order(order->OrderID(), &self, target, queued);

  return nullptr;
}

template <class Derived>
void AttackBaseCore<Derived>::ResolveOrder(Actor& self,
                                           const net::Order& order) {
  // L198-211
  const bool b_force_attack =
      order.str_order_string == "ForceAttack";
  if (b_force_attack || order.str_order_string == "Attack") {
    if (!order.target.IsValidFor(&self))
      return;

    AttackTarget(order.target, AttackSource::Default, order.b_queued, true,
                 b_force_attack, info_.target_line_color);
    self.ShowTargetLines();
  } else if (order.str_order_string == "Stop") {
    OnStopOrder(self);
  }
}

template <class Derived>
void AttackBaseCore<Derived>::OnStopOrder(Actor& self) {
  // L214-223(Resupply/ReturnToBase 活动未移植 → is 判恒 false —— 空集
  // 等价面,COVERAGE 登记)
  // L214-223 (Resupply/ReturnToBase unported → the `is` checks stay false
  // — the empty-set equivalent face, registered in COVERAGE).
  if (self.CurrentActivity() == nullptr)
    return;

  self.CancelActivity();
}

template <class Derived>
std::string AttackBaseCore<Derived>::VoicePhraseForOrder(
    Actor& /*self*/, const net::Order& order) {
  // L225-228
  return order.str_order_string == "Attack" ||
                 order.str_order_string == "ForceAttack"
             ? info_.str_voice
             : std::string{};
}

template <class Derived>
bool AttackBaseCore<Derived>::HasAnyValidWeapons(
    const sim::Target& t, bool check_for_center_targeting_weapons,
    bool reloading_is_invalid) {
  // L233-251
  if (sim::ConditionalTraitCore<Derived>::IsTraitDisabled())
    return false;

  if (info_.b_attack_requires_entering_cell &&
      (positionable_ == nullptr ||
       !positionable_->CanEnterCell(
           t.ActorPtr != nullptr ? t.ActorPtr->Location() : CPos{},
           nullptr, sim::BlockedByActor::None)))
    return false;

  // PERF: Avoid LINQ.(上游注释)
  for (Armament* armament : vec_armaments_) {
    const bool b_check_is_valid =
        check_for_center_targeting_weapons
            ? armament->Weapon->b_targetActorCenter
            : !armament->IsTraitPaused();
    const bool b_reloading_state_is_valid =
        !reloading_is_invalid || !armament->IsReloading();
    if (b_check_is_valid && b_reloading_state_is_valid &&
        !armament->IsTraitDisabled() &&
        sim::WeaponIsValidAgainst(*armament->Weapon, t, p_self_->world(),
                                  p_self_))
      return true;
  }

  return false;
}

template <class Derived>
WPos AttackBaseCore<Derived>::GetTargetPosition(
    const WPos& pos, const sim::Target& target) {
  // L253-256
  if (HasAnyValidWeapons(target, true))
    return target.CenterPosition();

  // Positions.ClosestToIgnoringPath(pos) = MinBy 距离(WorldUtils L64-67)
  // Positions.ClosestToIgnoringPath(pos) = the MinBy distance
  // (WorldUtils L64-67).
  const std::vector<WPos>& positions = target.Positions();
  const WPos* closest = nullptr;
  std::int64_t best = 0;
  for (const WPos& p : positions) {
    const std::int64_t d = (p - pos).LengthSquared();
    if (closest == nullptr || d < best) {
      closest = &p;
      best = d;
    }
  }
  return closest != nullptr ? *closest : target.CenterPosition();
}

template <class Derived>
WDist AttackBaseCore<Derived>::GetMinimumRange() {
  // L258-279
  if (sim::ConditionalTraitCore<Derived>::IsTraitDisabled())
    return WDist{0};

  // PERF: Avoid LINQ.(上游注释)
  auto min = WDist::MaxValue();
  for (Armament* armament : vec_armaments_) {
    if (armament->IsTraitDisabled())
      continue;

    if (armament->IsTraitPaused())
      continue;

    const WDist range = WDist{armament->Weapon->int4_minRange};
    if (min > range)
      min = range;
  }

  return min != WDist::MaxValue() ? min : WDist{0};
}

template <class Derived>
WDist AttackBaseCore<Derived>::GetMaximumRange() {
  // L281-302
  if (sim::ConditionalTraitCore<Derived>::IsTraitDisabled())
    return WDist{0};

  // PERF: Avoid LINQ.(上游注释)
  auto max = WDist{0};
  for (Armament* armament : vec_armaments_) {
    if (armament->IsTraitDisabled())
      continue;

    if (armament->IsTraitPaused())
      continue;

    const WDist range = armament->MaxRange();
    if (max < range)
      max = range;
  }

  return max;
}

template <class Derived>
WDist AttackBaseCore<Derived>::GetMinimumRangeVersusTarget(
    const sim::Target& target) {
  // L304-328
  if (sim::ConditionalTraitCore<Derived>::IsTraitDisabled())
    return WDist{0};

  // PERF: Avoid LINQ.(上游注释)
  auto min = WDist::MaxValue();
  for (Armament* armament : vec_armaments_) {
    if (armament->IsTraitDisabled())
      continue;

    if (armament->IsTraitPaused())
      continue;

    if (!sim::WeaponIsValidAgainst(*armament->Weapon, target,
                                   p_self_->world(), p_self_))
      continue;

    const WDist range = WDist{armament->Weapon->int4_minRange};
    if (min > range)
      min = range;
  }

  return min != WDist::MaxValue() ? min : WDist{0};
}

template <class Derived>
WDist AttackBaseCore<Derived>::GetMaximumRangeVersusTarget(
    const sim::Target& target) {
  // L330-362
  if (sim::ConditionalTraitCore<Derived>::IsTraitDisabled())
    return WDist{0};

  auto max = WDist{0};

  // We want actors to use only weapons with ammo for this, except when ALL
  // weapons are out of ammo, then we use the paused, valid weapon with
  // highest range.(上游注释)
  auto max_fallback = WDist{0};

  // PERF: Avoid LINQ.(上游注释)
  for (Armament* armament : vec_armaments_) {
    if (armament->IsTraitDisabled())
      continue;

    if (!sim::WeaponIsValidAgainst(*armament->Weapon, target,
                                   p_self_->world(), p_self_))
      continue;

    const WDist range = armament->MaxRange();
    if (max_fallback < range)
      max_fallback = range;

    if (armament->IsTraitPaused())
      continue;

    if (max < range)
      max = range;
  }

  return max != WDist{0} ? max : max_fallback;
}

template <class Derived>
std::vector<Armament*> AttackBaseCore<Derived>::ChooseArmamentsForTarget(
    const sim::Target& t, bool force_attack) {
  // L365-385
  // If force-fire is not used, and the target requires force-firing or the
  // target is terrain or invalid, no armaments can be used(上游注释)
  if (!force_attack &&
      ((t.Type() == sim::TargetType::Terrain &&
        !info_.b_target_terrain_without_force_fire) ||
       t.Type() == sim::TargetType::Invalid || t.RequiresForceFire()))
    return {};

  // Get target's owner; in case of terrain or invalid target there will be
  // no problems with owner == null since forceFire will have to be true in
  // this part of the method(上游注释)
  sim::Player* owner = nullptr;
  if (t.Type() == sim::TargetType::FrozenActor)
    owner = t.FrozenActorPtr->Owner();
  else if (t.Type() == sim::TargetType::Actor)
    owner = t.ActorPtr->Owner();

  std::vector<Armament*> vec_out;
  for (Armament* a : vec_armaments_) {
    const sim::PlayerRelationship relationships =
        force_attack
            ? a->InfoData().force_target_relationships
            : a->InfoData().target_relationships;
    if (!a->IsTraitDisabled() &&
        (owner == nullptr ||
         sim::HasRelationship(relationships,
                              p_self_->Owner()->RelationshipWith(owner))) &&
        sim::WeaponIsValidAgainst(*a->Weapon, t, p_self_->world(), p_self_))
      vec_out.push_back(a);
  }
  return vec_out;
}

template <class Derived>
void AttackBaseCore<Derived>::AttackTarget(
    const sim::Target& target, AttackSource source, bool queued,
    bool allow_move, bool force_attack,
    std::optional<core::Color> target_line_color) {
  // L387-398
  if (sim::ConditionalTraitCore<Derived>::IsTraitDisabled())
    return;

  if (!target.IsValidFor(p_self_))
    return;

  sim::Activity* activity = static_cast<Derived*>(this)->GetAttackActivity(
      *p_self_, source, target, allow_move, force_attack,
      target_line_color);
  p_self_->QueueActivity(queued, activity);
  static_cast<Derived*>(this)->OnResolveAttackOrder(*p_self_, activity,
                                                    target, queued,
                                                    force_attack);
}

template <class Derived>
bool AttackBaseCore<Derived>::IsReachableTarget(const sim::Target& target,
                                                bool allow_move) {
  // L402-406
  return HasAnyValidWeapons(target) &&
         (target.IsInRange(p_self_->CenterPosition(),
                           GetMaximumRangeVersusTarget(target)) ||
          (allow_move &&
           p_self_->Info()->HasTraitInfoOfInterface(
               "OpenRA.Traits.IMoveInfo")));
}

template <class Derived>
sim::PlayerRelationship AttackBaseCore<Derived>::UnforcedAttackTargetStances() {
  // L408-417
  // PERF: Avoid LINQ.(上游注释)
  auto stances = sim::PlayerRelationship::None;
  for (Armament* armament : vec_armaments_)
    if (!armament->IsTraitDisabled())
      stances = static_cast<sim::PlayerRelationship>(
          static_cast<std::int32_t>(stances) |
          static_cast<std::int32_t>(
              armament->InfoData().target_relationships));

  return stances;
}

// ———— AttackOrderTargeter(L419-524)————

template <class Derived>
bool AttackBaseCore<Derived>::AttackOrderTargeter::CanTarget(
    Actor& self, const sim::Target& target, sim::TargetModifiers& modifiers,
    std::string& cursor) {
  // L509-521
  switch (target.Type()) {
    case sim::TargetType::Actor:
    case sim::TargetType::FrozenActor:
      return CanTargetActor(self, target, modifiers, cursor);
    case sim::TargetType::Terrain:
      return CanTargetLocation(
          self, self.world().Map().CellContaining(target.CenterPosition()),
          modifiers, cursor);
    default:
      return false;
  }
}

template <class Derived>
bool AttackBaseCore<Derived>::AttackOrderTargeter::CanTargetActor(
    Actor& self, const sim::Target& target, sim::TargetModifiers& modifiers,
    std::string& cursor) {
  // L434-476
  b_is_queued_ =
      sim::HasModifier(modifiers, sim::TargetModifiers::ForceQueue);

  if (sim::HasModifier(modifiers, sim::TargetModifiers::ForceMove))
    return false;

  if (ab_.info_.b_force_fire_ignores_actors &&
      sim::HasModifier(modifiers, sim::TargetModifiers::ForceAttack))
    return false;

  // Disguised actors are revealed by the attack cursor(上游 HACK 注释)
  if (target.Type() == sim::TargetType::Actor &&
      target.ActorPtr->EffectiveOwner() != nullptr &&
      target.ActorPtr->EffectiveOwner()->Disguised() &&
      self.Owner()->RelationshipWith(target.ActorPtr->Owner()) ==
          sim::PlayerRelationship::Enemy)
    modifiers = modifiers | sim::TargetModifiers::ForceAttack;

  const bool b_force_attack =
      sim::HasModifier(modifiers, sim::TargetModifiers::ForceAttack);

  // Use valid armament with highest range out of those that have ammo
  // If all are out of ammo, just use valid armament with highest range
  // (上游注释;OrderBy(IsTraitPaused).ThenByDescending(MaxRange) 稳定序 →
  // 单遍择优,首见平键胜)
  // (the upstream comments; OrderBy(IsTraitPaused).
  // ThenByDescending(MaxRange)'s stable order → the single-pass pick with
  // the first-seen winning ties).
  Armament* a = nullptr;
  bool a_paused = false;
  WDist a_range{0};
  for (Armament* candidate : ab_.ChooseArmamentsForTarget(target,
                                                          b_force_attack)) {
    const bool candidate_paused = candidate->IsTraitPaused();
    const WDist candidate_range = candidate->MaxRange();
    if (a == nullptr || (candidate_paused && !a_paused) ||
        (!candidate_paused == !a_paused && candidate_range > a_range)) {
      a = candidate;
      a_paused = candidate_paused;
      a_range = candidate_range;
    }
  }
  if (a == nullptr)
    return false;

  const bool b_out_of_range =
      !target.IsInRange(self.CenterPosition(), a->MaxRange()) ||
      (!b_force_attack &&
       target.Type() == sim::TargetType::FrozenActor &&
       !ab_.info_.b_target_frozen_actors);

  if (b_out_of_range && ab_.info_.b_outside_range_requires_force_fire &&
      !sim::HasModifier(modifiers, sim::TargetModifiers::ForceAttack))
    return false;

  cursor = b_out_of_range
               ? (!ab_.info_.str_outside_range_cursor.empty()
                      ? ab_.info_.str_outside_range_cursor
                      : a->InfoData().str_outside_range_cursor)
               : (!ab_.info_.str_cursor.empty()
                      ? ab_.info_.str_cursor
                      : a->InfoData().str_cursor);

  if (!b_force_attack)
    return true;

  str_order_id_ = "ForceAttack";
  return true;
}

template <class Derived>
bool AttackBaseCore<Derived>::AttackOrderTargeter::CanTargetLocation(
    Actor& self, CPos location, sim::TargetModifiers modifiers,
    std::string& cursor) {
  // L478-507
  if (!self.world().Map().Contains(location))
    return false;

  b_is_queued_ =
      sim::HasModifier(modifiers, sim::TargetModifiers::ForceQueue);

  // Targeting the terrain is only possible with force-attack modifier or
  // when TargetTerrainWithoutForceFire is set(上游注释)
  if (sim::HasModifier(modifiers, sim::TargetModifiers::ForceMove) ||
      !(ab_.info_.b_target_terrain_without_force_fire ||
        sim::HasModifier(modifiers, sim::TargetModifiers::ForceAttack)))
    return false;

  const sim::Target target = sim::Target::FromCell(self.world(), location);

  // 同 CanTargetActor 的单遍择优
  // The same single-pass pick as CanTargetActor.
  Armament* a = nullptr;
  bool a_paused = false;
  WDist a_range{0};
  for (Armament* candidate : ab_.ChooseArmamentsForTarget(target, true)) {
    const bool candidate_paused = candidate->IsTraitPaused();
    const WDist candidate_range = candidate->MaxRange();
    if (a == nullptr || (candidate_paused && !a_paused) ||
        (!candidate_paused == !a_paused && candidate_range > a_range)) {
      a = candidate;
      a_paused = candidate_paused;
      a_range = candidate_range;
    }
  }
  if (a == nullptr)
    return false;

  cursor = !target.IsInRange(self.CenterPosition(), a->MaxRange())
               ? (!ab_.info_.str_outside_range_cursor.empty()
                      ? ab_.info_.str_outside_range_cursor
                      : a->InfoData().str_outside_range_cursor)
               : (!ab_.info_.str_cursor.empty()
                      ? ab_.info_.str_cursor
                      : a->InfoData().str_cursor);

  str_order_id_ = "ForceAttack";
  return true;
}

// ———— AttackFrontal(L18-48)————

AttackFrontal::AttackFrontal(ActorInitializer& init,
                             AttackBaseInfoData&& info)
    : AttackBaseCore<AttackFrontal>(init.Self(), info) {}

void AttackFrontal::Created(Actor& self) {
  CoreAttackBaseCreated(self);
  CoreCreated(self);
}

bool AttackFrontal::CanAttack(Actor& self, const sim::Target& target) {
  // L34-40
  if (!AttackBaseCore<AttackFrontal>::CanAttack(self, target))
    return false;

  return TargetInFiringArc(self, target, Info().angle_facing_tolerance);
}

sim::Activity* AttackFrontal::GetAttackActivity(
    Actor& self, AttackSource /*source*/, const sim::Target& new_target,
    bool allow_move, bool force_attack,
    std::optional<core::Color> target_line_color) {
  // L42-46
  return activities::NewActivity<activities::Attack>(
      self, new_target, allow_move, force_attack, target_line_color);
}

// 模板显式实例化(AttackTurreted 经 AttackFollow 继承 —— 实例化点在
// 本 TU:模板体定义于此)
// The explicit template instantiations (AttackTurreted inherits through
// AttackFollow — the instantiation point sits in this TU: the template
// bodies are defined here).
template class AttackBaseCore<AttackFrontal>;

// ———— [VerifySync] 哈希注册(gen/sync_gen.cpp:AttackFrontal {IsAiming})————
// ———— The [VerifySync] hash registration (gen/sync_gen.cpp:
//      AttackFrontal {IsAiming}) ————
int AttackFrontalSyncHash(const sim::ISync* s) {
  const auto* frontal = static_cast<const AttackFrontal*>(s);
  return sim::sync::CombineSyncHash(0,
                                    sim::sync::HashBool(frontal->b_is_aiming));
}

const bool b_attack_frontal_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.AttackFrontal",
                                &AttackFrontalSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_attack_frontal_sync_registered_anchor =
    &b_attack_frontal_sync_registered;

}  // namespace ora::mods

// AttackFollow 的显式实例化(attack_follow.hpp 引入完整类;实例化点在
// 本 TU —— 模板体定义于此)
// AttackFollow's explicit instantiation (attack_follow.hpp pulls in the
// complete class; the instantiation point sits in this TU — the template
// bodies are defined here).
#include "mods/attack_follow.hpp"
template class ora::mods::AttackBaseCore<ora::mods::AttackFollow>;
