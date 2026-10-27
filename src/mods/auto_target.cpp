#include "mods/auto_target.hpp"

#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "mods/attack_activity.hpp"
#include "meta/field_loader.hpp"
#include "net/order.hpp"
#include "mods/actor_exts.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

AutoTargetInfoData AutoTargetInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AutoTargetInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "AllowMovement"))
    data.b_allow_movement = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "AllowTurning"))
    data.b_allow_turning = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "ScanOnIdle"))
    data.b_scan_on_idle = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "ScanRadius"))
    data.int4_scan_radius = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "InitialStanceAI"))
    data.initial_stance_ai = static_cast<UnitStance>(
        static_cast<std::int32_t>(*v));
  if (const auto v = sim::RecordFieldInt(rec_info, "InitialStance"))
    data.initial_stance = static_cast<UnitStance>(
        static_cast<std::int32_t>(*v));
  if (const auto v = sim::RecordFieldString(rec_info, "HoldFireCondition"))
    data.str_hold_fire_condition = std::string{*v};
  if (const auto v =
          sim::RecordFieldString(rec_info, "ReturnFireCondition"))
    data.str_return_fire_condition = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "DefendCondition"))
    data.str_defend_condition = std::string{*v};
  if (const auto v =
          sim::RecordFieldString(rec_info, "AttackAnythingCondition"))
    data.str_attack_anything_condition = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "EnableStances"))
    data.b_enable_stances = *v != 0;
  if (const auto v =
          sim::RecordFieldInt(rec_info, "MinimumScanTimeInterval"))
    data.int4_minimum_scan_time_interval = static_cast<int>(*v);
  if (const auto v =
          sim::RecordFieldInt(rec_info, "MaximumScanTimeInterval"))
    data.int4_maximum_scan_time_interval = static_cast<int>(*v);

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);

  if (!data.str_hold_fire_condition.empty())
    data.map_condition_by_stance[UnitStance::HoldFire] =
        data.str_hold_fire_condition;
  if (!data.str_return_fire_condition.empty())
    data.map_condition_by_stance[UnitStance::ReturnFire] =
        data.str_return_fire_condition;
  if (!data.str_defend_condition.empty())
    data.map_condition_by_stance[UnitStance::Defend] =
        data.str_defend_condition;
  if (!data.str_attack_anything_condition.empty())
    data.map_condition_by_stance[UnitStance::AttackAnything] =
        data.str_attack_anything_condition;

  return data;
}

AutoTargetPriorityInfoData AutoTargetPriorityInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AutoTargetPriorityInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "ValidTargets") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.bitset_valid_targets =
              core::BitSet<sim::TargetableType>::FromRawBits(
                  static_cast<std::uint64_t>(*n));
      } else if (name == "InvalidTargets") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.bitset_invalid_targets =
              core::BitSet<sim::TargetableType>::FromRawBits(
                  static_cast<std::uint64_t>(*n));
      } else if (name == "ValidRelationships") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.valid_relationships =
              static_cast<sim::PlayerRelationship>(
                  static_cast<std::int32_t>(*n));
      }
    }
  }
  if (const auto v = sim::RecordFieldInt(rec_info, "Priority"))
    data.int4_priority = static_cast<int>(*v);

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

AutoTargetPriority::AutoTargetPriority(AutoTargetPriorityInfoData&& info)
    : sim::ConditionalTraitCore<AutoTargetPriority>(info.conditional),
      info_{std::move(info)} {}

AutoTarget::AutoTarget(ActorInitializer& init, const AutoTargetInfoData& info)
    : sim::ConditionalTraitCore<AutoTarget>(info.conditional), info_{info} {

  Actor& self = init.Self();
  for (AttackBaseFace* t : self.TraitsImplementing<AttackBaseFace>())
    if (!t->IsTraitDisabled())
      vec_active_attack_bases_.push_back(t);

  stance_ = init.GetValue<StanceInit, UnitStance>(
      self.Owner()->IsBot() || !self.Owner()->Playable()
          ? info.initial_stance_ai
          : info.initial_stance);

  predicted_stance_ = stance_;

  b_allow_movement_ =
      info.b_allow_movement && self.TraitOrDefault<sim::IMove>() != nullptr;
}

void AutoTarget::SetStance(Actor& self, UnitStance value) {

  if (stance_ == value)
    return;

  const UnitStance old_stance = stance_;
  stance_ = predicted_stance_ = value;
  ApplyStanceCondition(self);

  for (INotifyStanceChanged* nsc : vec_notify_stance_changed_)
    nsc->StanceChanged(self, this, old_stance, stance_);

  if (self.CurrentActivity() != nullptr)
    for (auto* a :
         self.CurrentActivity()
             ->template ActivitiesImplementing<
                 IActivityNotifyStanceChanged>())
      a->StanceChanged(self, this, old_stance, stance_);
}

void AutoTarget::ApplyStanceCondition(Actor& self) {

  if (int4_condition_token_ != Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);

  const auto it = info_.map_condition_by_stance.find(stance_);
  if (it != info_.map_condition_by_stance.end())
    int4_condition_token_ = self.GrantCondition(it->second);
}

void AutoTarget::Created(Actor& self) {

  std::vector<AutoTargetPriority*> vec_priorities =
      self.TraitsImplementing<AutoTargetPriority>();

  std::stable_sort(vec_priorities.begin(), vec_priorities.end(),
                   [](const AutoTargetPriority* a,
                      const AutoTargetPriority* b) {
                     return a->Info().int4_priority >
                            b->Info().int4_priority;
                   });
  for (AutoTargetPriority* ati : vec_priorities)
    vec_priorities_ordered_.push_back(ati);

  vec_override_auto_target_ =
      self.TraitsImplementing<sim::IOverrideAutoTarget>();
  vec_notify_stance_changed_ =
      self.TraitsImplementing<INotifyStanceChanged>();
  ApplyStanceCondition(self);

  CoreCreated(self);
}

std::vector<const AutoTargetPriorityInfoData*>
AutoTarget::ActiveTargetPriorities() const {

  std::vector<const AutoTargetPriorityInfoData*> vec_out;
  for (const AutoTargetPriority* ati : vec_priorities_ordered_)
    if (!ati->IsTraitDisabled())
      vec_out.push_back(&ati->Info());
  return vec_out;
}

void AutoTarget::OnOwnerChanged(Actor& self, Player& ,
                                Player& ) {

  SetStance(self, self.Owner()->IsBot() || !self.Owner()->Playable()
                      ? info_.initial_stance_ai
                      : info_.initial_stance);
}

void AutoTarget::ResolveOrder(Actor& self, const net::Order& order) {

  if (order.str_order_string == "SetUnitStance" &&
      info_.b_enable_stances)
    SetStance(self,
              static_cast<UnitStance>(order.uint4_extra_data));
}

void AutoTarget::Damaged(Actor& self, const sim::AttackInfo& e) {

  if (IsTraitDisabled() || !self.IsIdle() ||
      stance_ < UnitStance::ReturnFire)
    return;

  if (e.Damage->Value < 0)
    return;

  Actor* attacker = const_cast<Actor*>(e.Attacker);
  if (attacker->Disposed())
    return;

  for (sim::IOverrideAutoTarget* oat : vec_override_auto_target_)
    if (oat->TryGetAutoTargetOverride(self))
      return;

  if (!attacker->IsInWorld()) {

  }

  if (!AllowMove() && !attacker->CanBeViewedByPlayer(self.Owner()))
    return;

  const sim::Target attacker_as_target = sim::Target::FromActor(attacker);
  bool b_any_valid = false;
  for (AttackBaseFace* a : vec_active_attack_bases_)
    if (a->HasAnyValidWeapons(attacker_as_target)) {
      b_any_valid = true;
      break;
    }
  if (!b_any_valid)
    return;

  if (AppearsFriendlyTo(*attacker, self))
    return;

  if (stance_ > UnitStance::ReturnFire) {
    const sim::Target auto_target_scan =
        ScanForTarget(self, AllowMove(), true);

    if (auto_target_scan.Type() != sim::TargetType::Invalid)
      attacker = const_cast<Actor*>(auto_target_scan.ActorPtr);
  }

  Aggressor = attacker;

  Attack(sim::Target::FromActor(Aggressor), AllowMove());
}

void AutoTarget::TickIdle(Actor& self) {

  if (IsTraitDisabled() || !info_.b_scan_on_idle ||
      stance_ < UnitStance::Defend)
    return;

  const bool b_allow_turn =
      info_.b_allow_turning && stance_ > UnitStance::HoldFire;
  ScanAndAttack(self, AllowMove(), b_allow_turn);
}

void AutoTarget::Tick(Actor& ) {

  if (IsTraitDisabled())
    return;

  if (next_scan_time > 0)
    --next_scan_time;
}

sim::Target AutoTarget::ScanForTarget(Actor& self, bool allow_move,
                                      bool allow_turn,
                                      bool ignore_scan_interval) {

  const bool b_any_attack_base = !vec_active_attack_bases_.empty();
  if ((ignore_scan_interval || next_scan_time <= 0) && b_any_attack_base) {
    for (sim::IOverrideAutoTarget* oat : vec_override_auto_target_)
      if (std::optional<sim::Target> existing_target =
              oat->TryGetAutoTargetOverride(self))
        return *existing_target;

    if (!ignore_scan_interval)
      next_scan_time = self.world().SharedRandom().Next(
          info_.int4_minimum_scan_time_interval,
          info_.int4_maximum_scan_time_interval);

    for (AttackBaseFace* ab : vec_active_attack_bases_) {

      const sim::PlayerRelationship attack_stances =
          ab->UnforcedAttackTargetStances();
      if (attack_stances != sim::PlayerRelationship::None) {
        const WDist range =
            info_.int4_scan_radius > 0
                ? WDist{info_.int4_scan_radius * 1024}
                : ab->GetMaximumRange();
        const sim::Target target = ChooseTarget(
            self, ab, attack_stances, range, allow_move, allow_turn);
        if (target.Type() != sim::TargetType::Invalid)
          return target;
      }
    }
  }

  return sim::Target::Invalid();
}

void AutoTarget::ScanAndAttack(Actor& self, bool allow_move,
                               bool allow_turn) {

  const sim::Target target = ScanForTarget(self, allow_move, allow_turn);
  if (target.Type() != sim::TargetType::Invalid)
    Attack(target, allow_move);
}

void AutoTarget::Attack(const sim::Target& target, bool allow_move) {

  for (AttackBaseFace* ab : vec_active_attack_bases_)
    ab->AttackTarget(target, AttackSource::AutoTarget, false, allow_move);
}

bool AutoTarget::HasValidTargetPriority(
    Actor& self, Player* owner,
    const core::BitSet<sim::TargetableType>& target_types) {

  if (owner == nullptr || stance_ <= UnitStance::ReturnFire)
    return false;

  for (const AutoTargetPriorityInfoData* ati :
       ActiveTargetPriorities()) {

    if (!sim::HasRelationship(
            ati->valid_relationships,
            self.Owner()->RelationshipWith(owner)))
      continue;

    if (!ati->bitset_valid_targets.Overlaps(target_types) ||
        ati->bitset_invalid_targets.Overlaps(target_types))
      continue;

    return true;
  }
  return false;
}

sim::Target AutoTarget::ChooseTarget(
    Actor& self, AttackBaseFace* ab, sim::PlayerRelationship attack_stances,
    WDist scan_range, bool allow_move, bool allow_turn) {

  sim::Target chosen_target = sim::Target::Invalid();
  int chosen_target_priority = std::numeric_limits<int>::min();
  int chosen_target_range = 0;

  const std::vector<const AutoTargetPriorityInfoData*> vec_active_priorities =
      ActiveTargetPriorities();
  if (vec_active_priorities.empty())
    return chosen_target;

  std::vector<sim::Target> vec_targets_in_range;
  for (Actor* a :
       self.world().FindActorsInCircle(self.CenterPosition(), scan_range))
    vec_targets_in_range.push_back(sim::Target::FromActor(a));

  sim::FrozenActorLayer* frozen_layer =
      self.Owner()->GetFrozenActorLayer();
  if ((allow_move || ab->TargetFrozenActors()) &&
      frozen_layer != nullptr)
    for (sim::FrozenActor* fa : frozen_layer->FrozenActorsInCircle(
             self.world(), self.CenterPosition(), scan_range))
      vec_targets_in_range.push_back(
          sim::Target::FromFrozenActor(fa));

  for (const sim::Target& target : vec_targets_in_range) {
    core::BitSet<sim::TargetableType> target_types;
    Player* owner = nullptr;
    if (target.Type() == sim::TargetType::Actor) {

      if (attack_stances == sim::PlayerRelationship::Enemy &&
          !AppearsHostileTo(*const_cast<Actor*>(target.ActorPtr), self))
        continue;

      target_types = target.ActorPtr->GetEnabledTargetTypes();

      if (PreventsAutoTarget(self,
                             *const_cast<Actor*>(target.ActorPtr)) ||
          !target.ActorPtr->CanBeViewedByPlayer(self.Owner()))
        continue;

      owner = target.ActorPtr->Owner();
    } else if (target.Type() == sim::TargetType::FrozenActor) {
      if (attack_stances == sim::PlayerRelationship::Enemy &&
          self.Owner()->RelationshipWith(target.FrozenActorPtr->Owner()) ==
              sim::PlayerRelationship::Ally)
        continue;

      if (self.Owner()->IsBot() &&
          target.FrozenActorPtr->ActorPtr() == nullptr)
        continue;

      target_types = target.FrozenActorPtr->TargetTypes();
      owner = target.FrozenActorPtr->Owner();
    } else {
      continue;
    }

    vec_valid_priorities_buffer_.clear();
    for (const AutoTargetPriorityInfoData* ati : vec_active_priorities) {

      if (ati->int4_priority < chosen_target_priority)
        continue;

      if (!sim::HasRelationship(
              ati->valid_relationships,
              self.Owner()->RelationshipWith(owner)))
        continue;

      if (!ati->bitset_valid_targets.Overlaps(target_types) ||
          ati->bitset_invalid_targets.Overlaps(target_types))
        continue;

      vec_valid_priorities_buffer_.push_back(ati);
    }

    if (vec_valid_priorities_buffer_.empty())
      continue;

    std::vector<Armament*> vec_armaments =
        ab->ChooseArmamentsForTarget(target, false);
    if (!allow_move) {

      std::vector<Armament*> vec_in_range;
      for (Armament* arm : vec_armaments)
        if (target.IsInRange(self.CenterPosition(), arm->MaxRange()) &&
            !target.IsInRange(
                self.CenterPosition(),
                WDist{arm->Weapon->int4_minRange}))
          vec_in_range.push_back(arm);
      vec_armaments = std::move(vec_in_range);
    }

    if (vec_armaments.empty())
      continue;

    if (!allow_turn &&
        !ab->TargetInFiringArc(self, target,
                               ab->FacingTolerance()))
      continue;

    const int target_range =
        (target.CenterPosition() - self.CenterPosition()).Length();
    for (const AutoTargetPriorityInfoData* ati :
         vec_valid_priorities_buffer_) {
      if (chosen_target.Type() == sim::TargetType::Invalid ||
          chosen_target_priority < ati->int4_priority ||
          (chosen_target_priority == ati->int4_priority &&
           target_range < chosen_target_range)) {
        chosen_target = target;
        chosen_target_priority = ati->int4_priority;
        chosen_target_range = target_range;
      }
    }
  }

  return chosen_target;
}

bool AutoTarget::PreventsAutoTarget(Actor& attacker, Actor& target) {

  for (sim::IDisableEnemyAutoTarget* deat :
       target.TraitsImplementing<sim::IDisableEnemyAutoTarget>())
    if (deat->DisableEnemyAutoTarget(target, attacker))
      return true;

  return false;
}

int AutoTargetSyncHash(const sim::ISync* s) {
  const auto* auto_target = static_cast<const AutoTarget*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(0, auto_target->next_scan_time),
      sim::sync::HashActor(auto_target->Aggressor));
}

const bool b_auto_target_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.AutoTarget",
                                &AutoTargetSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_auto_target_sync_registered_anchor =
    &b_auto_target_sync_registered;

}
