// UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackFollow.cs 实现部分
//          + AttackTurreted.cs | The implementation half.
#include "mods/attack_follow.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/affects_shroud.hpp"
#include "mods/armament.hpp"
#include "mods/move_activities.hpp"
#include "game/actor_info.hpp"
#include "mods/turreted.hpp"
#include "mods/util.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— AttackFollowInfoData/AttackTurretedInfoData(L24-38 + Turreted L18-26)————

AttackFollowInfoData AttackFollowInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AttackFollowInfoData data;
  static_cast<AttackBaseInfoData&>(data) =
      AttackBaseInfoData::Parse(rec_info);

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "OpportunityFire") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_opportunity_fire = *b;
      } else if (name == "PersistentTargeting") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_persistent_targeting = *b;
      } else if (name == "RangeMargin") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.dist_range_margin = WDist{static_cast<int>(*n)};
      } else if (name == "AbortOnResupply") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_abort_on_resupply = *b;
      }
    }
  }

  return data;
}

AttackTurretedInfoData AttackTurretedInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AttackTurretedInfoData data;
  static_cast<AttackFollowInfoData&>(data) =
      AttackFollowInfoData::Parse(rec_info);

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      if (fields[i]->str_name != "Turrets")
        continue;
      const meta::GenericValue& v = generated->Slot(i);
      if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
              &v.val)) {
        data.vec_turrets.clear();
        for (const auto& element : *list)
          if (auto* s = std::get_if<std::string>(&element.val))
            data.vec_turrets.push_back(*s);
      }
    }
  }

  return data;
}

// ———— AttackFollow(L40-465)————

AttackFollow::AttackFollow(ActorInitializer& init,
                           const AttackFollowInfoData& info)
    : AttackBaseCore<AttackFollow>(init.Self(), info),
      info_follow_{info} {}

void AttackFollow::Created(Actor& self) {
  // L79-84
  p_mobile_ = self.TraitOrDefault<Mobile>();
  p_auto_target_ = self.TraitOrDefault<AutoTarget>();
  CoreAttackBaseCreated(self);
  CoreCreated(self);
}

void AttackFollow::SetRequestedTarget(const sim::Target& target,
                                      bool is_force_attack,
                                      sim::Activity* requested_target_preset) {
  // L53-58
  target_requested_ = target;
  b_requested_force_attack_ = is_force_attack;
  p_requested_target_preset_for_activity_ = requested_target_preset;
}

void AttackFollow::ClearRequestedTarget() {
  // L60-71
  if (FollowInfo().b_persistent_targeting) {
    target_opportunity_ = target_requested_;
    b_opportunity_force_attack_ = b_requested_force_attack_;
    b_opportunity_target_is_persistent_target_ = true;
  }

  target_requested_ = sim::Target::Invalid();
  p_requested_target_preset_for_activity_ = nullptr;
}

bool AttackFollow::CanAimAtTarget(Actor& self, const sim::Target& target,
                                  bool force_attack) {
  // L86-103
  if (target.Type() == sim::TargetType::Actor &&
      !target.ActorPtr->CanBeViewedByPlayer(self.Owner()))
    return false;

  if (target.Type() == sim::TargetType::FrozenActor &&
      !target.FrozenActorPtr->IsValid())
    return false;

  const WPos pos = self.CenterPosition();
  const std::vector<Armament*> vec_armaments =
      ChooseArmamentsForTarget(target, force_attack);
  for (Armament* a : vec_armaments) {
    if (target.IsInRange(pos, a->MaxRange()) &&
        (WDist{a->Weapon->int4_minRange} == WDist{0} ||
         !target.IsInRange(pos, WDist{a->Weapon->int4_minRange})) &&
        TargetInFiringArc(self, target,
                          FollowInfo().angle_facing_tolerance))
      return true;
  }

  return false;
}

void AttackFollow::Tick(Actor& self) {
  // L105-160
  if (IsTraitDisabled()) {
    target_requested_ = target_opportunity_ = sim::Target::Invalid();
    b_opportunity_target_is_persistent_target_ = false;
  }

  if (p_requested_target_preset_for_activity_ != nullptr) {
    // RequestedTarget was set by OnQueueAttackActivity in preparation for a
    // queued activity(上游注释)
    if (self.CurrentActivity() != nullptr &&
        self.CurrentActivity()->NextActivity() ==
            p_requested_target_preset_for_activity_) {
      bool unused_hidden = false;
      target_requested_ =
          target_requested_.Recalculate(self.Owner(), unused_hidden);
    } else {
      // Requested activity has been canceled(上游注释)
      ClearRequestedTarget();
    }
  }

  // Can't fire on anything(上游注释)
  if (p_mobile_ != nullptr && !p_mobile_->CanInteractWithGroundLayer(&self))
    return;

  if (target_requested_.IsValidFor(&self)) {
    SetIsAiming(CanAimAtTarget(self, target_requested_,
                               b_requested_force_attack_));
    if (IsAiming())
      DoAttack(self, target_requested_);
  } else {
    SetIsAiming(false);

    if (target_opportunity_.IsValidFor(&self))
      SetIsAiming(CanAimAtTarget(self, target_opportunity_,
                                 b_opportunity_force_attack_));

    if (!IsAiming() && FollowInfo().b_opportunity_fire &&
        p_auto_target_ != nullptr &&
        !p_auto_target_->IsTraitDisabled() &&
        p_auto_target_->Stance() >= UnitStance::Defend) {
      target_opportunity_ =
          p_auto_target_->ScanForTarget(self, false, false);
      b_opportunity_force_attack_ = false;
      b_opportunity_target_is_persistent_target_ = false;

      if (target_opportunity_.IsValidFor(&self))
        SetIsAiming(CanAimAtTarget(self, target_opportunity_,
                                   b_opportunity_force_attack_));
    }

    if (IsAiming())
      DoAttack(self, target_opportunity_);
  }

  AttackBaseCore<AttackFollow>::Tick(self);
}

sim::Activity* AttackFollow::GetAttackActivity(
    Actor& self, AttackSource source, const sim::Target& new_target,
    bool allow_move, bool force_attack,
    std::optional<core::Color> target_line_color) {
  // L162-170
  // HACK: Manually set force attacking if we persisted an opportunity
  // target that required force attacking(上游注释)
  bool b_force_attack = force_attack;
  if (b_opportunity_target_is_persistent_target_ &&
      b_opportunity_force_attack_ && new_target == target_opportunity_)
    b_force_attack = true;

  return activities::NewActivity<activities::AttackFollowActivity>(
      self, source, new_target, allow_move, b_force_attack,
      target_line_color);
}

void AttackFollow::OnResolveAttackOrder(Actor& /*self*/,
                                        sim::Activity* activity,
                                        const sim::Target& target,
                                        bool queued, bool force_attack) {
  // L172-178
  // We can improve responsiveness for turreted actors by preempting the
  // last order (usually a move) and setting the target immediately
  // (上游注释)
  if (!queued)
    SetRequestedTarget(target, force_attack, activity);
}

void AttackFollow::OnStopOrder(Actor& self) {
  // L179-185
  target_requested_ = target_opportunity_ = sim::Target::Invalid();
  b_opportunity_target_is_persistent_target_ = false;
  AttackBaseCore<AttackFollow>::OnStopOrder(self);
}

void AttackFollow::OnOwnerChanged(Actor& /*self*/, Player& /*old_owner*/,
                                  Player& /*new_owner*/) {
  // L187-191
  target_requested_ = target_opportunity_ = sim::Target::Invalid();
  b_opportunity_target_is_persistent_target_ = false;
}

std::optional<sim::Target> AttackFollow::TryGetAutoTargetOverride(
    Actor& /*self*/) {
  // L193-209
  if (target_requested_.Type() != sim::TargetType::Invalid)
    return target_requested_;

  if (b_opportunity_target_is_persistent_target_ &&
      target_opportunity_.Type() != sim::TargetType::Invalid)
    return target_opportunity_;

  return std::nullopt;
}

void AttackFollow::StanceChanged(Actor& self, AutoTarget* auto_target,
                                 UnitStance old_stance,
                                 UnitStance new_stance) {
  // L211-229
  // Cancel opportunity targets when switching to a more restrictive stance
  // if they are no longer valid for auto-targeting(上游注释)
  if (new_stance > old_stance || b_opportunity_force_attack_)
    return;

  if (target_opportunity_.Type() == sim::TargetType::Actor) {
    Actor* a = const_cast<Actor*>(target_opportunity_.ActorPtr);
    if (!auto_target->HasValidTargetPriority(self, a->Owner(),
                                             a->GetEnabledTargetTypes()))
      target_opportunity_ = sim::Target::Invalid();
  } else if (target_opportunity_.Type() ==
             sim::TargetType::FrozenActor) {
    sim::FrozenActor* fa = const_cast<sim::FrozenActor*>(
        target_opportunity_.FrozenActorPtr);
    if (!auto_target->HasValidTargetPriority(self, fa->Owner(),
                                             fa->TargetTypes()))
      target_opportunity_ = sim::Target::Invalid();
  }
}

// ———— AttackFollowActivity(AttackFollow.cs L231-464)————

namespace activities {

AttackFollowActivity::AttackFollowActivity(
    sim::Actor& self, AttackSource source, const sim::Target& target,
    bool allow_move, bool force_attack,
    std::optional<core::Color> target_line_color)
    : b_force_attack_{force_attack},
      opt_target_line_color_{target_line_color},
      source_{source},
      move_cooldown_helper_{self.world(),
                            dynamic_cast<Mobile*>(
                                self.TraitOrDefault<sim::IMove>())},
      target_{target} {
  // L253-288
  p_attack_ = self.Trait<AttackFollow>();
  p_move_ = allow_move ? self.TraitOrDefault<sim::IMove>() : nullptr;
  vec_reveals_shroud_ = self.TraitsImplementing<RevealsShroud>();
  // Rearmable trait 未移植:rearmable == null 恒成立(补给分支不可达;
  // COVERAGE 登记)
  // The Rearmable trait is unported: rearmable == null holds constantly
  // (the resupply branch is unreachable; registered in COVERAGE).
  move_cooldown_helper_.SetRetryIfDestinationBlocked(true);

  b_child_has_priority_ = false;

  // The target may become hidden between the initial order request and the
  // first tick (e.g. if queued)(上游注释)
  if ((target.Type() == sim::TargetType::Actor &&
       target.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target.Type() == sim::TargetType::FrozenActor ||
      target.Type() == sim::TargetType::Terrain) {
    last_visible_target_ = sim::Target::FromPos(target.CenterPosition());
    dist_last_visible_maximum_range_ =
        p_attack_->GetMaximumRangeVersusTarget(target);
    dist_last_visible_minimum_range_ =
        p_attack_->GetMinimumRangeVersusTarget(target);

    if (target.Type() == sim::TargetType::Actor) {
      p_last_visible_owner_ = target.ActorPtr->Owner();
      bitset_last_visible_target_types_ =
          target.ActorPtr->GetEnabledTargetTypes();
    } else if (target.Type() == sim::TargetType::FrozenActor) {
      p_last_visible_owner_ = target.FrozenActorPtr->Owner();
      bitset_last_visible_target_types_ =
          target.FrozenActorPtr->TargetTypes();
    }
  }
}

bool AttackFollowActivity::Tick(sim::Actor& self) {
  // L290-429
  if (!IsCanceling() && !HasArmamentsFor(target_))
    Cancel(self, true);

  if (!TickChild(self))
    return false;

  b_return_to_base_ = false;

  if (IsCanceling())
    return true;

  // Check that AttackFollow hasn't cancelled the target by modifying
  // attack.Target(上游注释)
  if (b_has_ticked_ &&
      p_attack_->RequestedTarget().Type() == sim::TargetType::Invalid)
    return true;

  if (p_attack_->IsTraitPaused())
    return false;

  bool b_target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
  p_attack_->SetRequestedTarget(target_, b_force_attack_);
  b_has_ticked_ = true;

  if (!b_target_is_hidden_actor &&
      target_.Type() == sim::TargetType::Actor) {
    last_visible_target_ = sim::Target::FromTargetPositions(target_);
    dist_last_visible_maximum_range_ =
        p_attack_->GetMaximumRangeVersusTarget(target_);
    dist_last_visible_minimum_range_ = p_attack_->GetMinimumRange();
    p_last_visible_owner_ = target_.ActorPtr->Owner();
    bitset_last_visible_target_types_ =
        target_.ActorPtr->GetEnabledTargetTypes();

    const int leeway = p_attack_->FollowInfo().dist_range_margin.Length;
    if (leeway != 0 && p_move_ != nullptr &&
        target_.ActorPtr->Info()->HasTraitInfoOfInterface(
            "OpenRA.Traits.IMoveInfo")) {
      const int prefer_min_range =
          std::min(dist_last_visible_minimum_range_.Length + leeway,
                   dist_last_visible_maximum_range_.Length);
      const int prefer_max_range =
          std::max(dist_last_visible_maximum_range_.Length - leeway,
                   dist_last_visible_minimum_range_.Length);
      dist_last_visible_maximum_range_ =
          WDist{std::clamp(dist_last_visible_maximum_range_.Length - leeway,
                           prefer_min_range, prefer_max_range)};
    }
  }
  // The target may become hidden in the same tick the AttackActivity
  // constructor is called, causing lastVisible* to remain uninitialized.
  // Fix the fallback values based on the frozen actor properties(上游注释)
  else if (target_.Type() == sim::TargetType::FrozenActor &&
           !last_visible_target_.IsValidFor(&self)) {
    last_visible_target_ = sim::Target::FromTargetPositions(target_);
    dist_last_visible_maximum_range_ =
        p_attack_->GetMaximumRangeVersusTarget(target_);
    p_last_visible_owner_ = target_.FrozenActorPtr->Owner();
    bitset_last_visible_target_types_ =
        target_.FrozenActorPtr->TargetTypes();
  }

  WDist dist_max_range = dist_last_visible_maximum_range_;
  const WDist dist_min_range = dist_last_visible_minimum_range_;
  b_use_last_visible_target_ =
      b_target_is_hidden_actor || !target_.IsValidFor(&self);

  // Most actors want to be able to see their target before shooting
  // (上游注释)
  if (target_.Type() == sim::TargetType::FrozenActor &&
      !p_attack_->TargetFrozenActors() && !b_force_attack_) {
    // revealsShroud.Where(!disabled).MaxByOrDefault(Range)
    RevealsShroud* rs = nullptr;
    for (RevealsShroud* s : vec_reveals_shroud_)
      if (!s->IsTraitDisabled() &&
          (rs == nullptr || s->Range() > rs->Range()))
        rs = s;

    // Default to 2 cells if there are no active traits(上游注释)
    const WDist sight_range =
        rs != nullptr ? rs->Range() : WDist{2 * 1024};
    if (sight_range < dist_max_range)
      dist_max_range = sight_range;
  }

  const std::optional<bool> result =
      move_cooldown_helper_.Tick(b_target_is_hidden_actor);
  if (result.has_value())
    return *result;

  // Target is hidden or dead, and we don't have a fallback position to
  // move towards(上游注释)
  if (b_use_last_visible_target_ &&
      !last_visible_target_.IsValidFor(&self))
    return true;

  // If all valid weapons have depleted their ammo and Rearmable trait
  // exists, return to RearmActor to reload(上游注释;Rearmable 未移植 →
  // 分支不可达)
  // (upstream's comment; Rearmable is unported → the branch is
  // unreachable.)

  const WPos pos = self.CenterPosition();
  const sim::Target& check_target =
      b_use_last_visible_target_ ? last_visible_target_ : target_;

  // We've reached the required range - if the target is visible and valid
  // then we wait, otherwise if it is hidden or dead we give up(上游注释)
  if (check_target.IsInRange(pos, dist_max_range) &&
      !check_target.IsInRange(pos, dist_min_range)) {
    if (b_use_last_visible_target_)
      return true;

    return false;
  }

  // We can't move into range, so give up(上游注释)
  if (p_move_ == nullptr || dist_max_range == WDist{0} ||
      dist_max_range < dist_min_range)
    return true;

  move_cooldown_helper_.NotifyMoveQueued();
  QueueChild(p_move_->MoveWithinRange(
      target_, dist_min_range, dist_max_range,
      check_target.CenterPosition(), std::optional<core::Color>{}));
  return false;
}

void AttackFollowActivity::OnLastRun(sim::Actor& /*self*/) {
  // L431-435
  // Cancel the requested target, but keep firing on it while in range
  // (上游注释)
  p_attack_->ClearRequestedTarget();
}

void AttackFollowActivity::StanceChanged(sim::Actor& self,
                                         AutoTarget* auto_target,
                                         UnitStance old_stance,
                                         UnitStance new_stance) {
  // L437-446
  // Cancel non-forced targets when switching to a more restrictive stance
  // if they are no longer valid for auto-targeting(上游注释)
  if (new_stance > old_stance || b_force_attack_)
    return;

  // If lastVisibleTarget is invalid we could never view the target in the
  // first place, so we just drop it here too(上游注释)
  if (!last_visible_target_.IsValidFor(&self) ||
      !auto_target->HasValidTargetPriority(
          self, p_last_visible_owner_, bitset_last_visible_target_types_))
    p_attack_->ClearRequestedTarget();
}

bool AttackFollowActivity::HasArmamentsFor(const sim::Target& target) {
  // L460-463
  return !p_attack_->IsTraitDisabled() &&
         !p_attack_->ChooseArmamentsForTarget(target, b_force_attack_)
              .empty();
}

}  // namespace activities

// ———— AttackTurreted(AttackTurreted.cs L27-50)————

AttackTurreted::AttackTurreted(ActorInitializer& init,
                               const AttackTurretedInfoData& info)
    : AttackFollow(init, info), info_turrets_{info} {
  // L31-35:ctor 的 Turreted 解析(构造序同上游 —— Created 之前;
  // info 副本保 vec_turrets_ —— 基类持 AttackFollowInfoData 切片)
  // L31-35: the ctor's Turreted resolution (upstream's construct order —
  // before Created; the info copy keeps vec_turrets_ — the base holds the
  // AttackFollowInfoData slice).
  for (Turreted* t : init.Self().TraitsImplementing<Turreted>())
    if (std::find(info_turrets_.vec_turrets.begin(),
                  info_turrets_.vec_turrets.end(),
                  t->Name()) != info_turrets_.vec_turrets.end())
      vec_turrets_.push_back(t);
}

bool AttackTurreted::CanAttack(Actor& self, const sim::Target& target) {
  // L37-49
  if (target.Type() == sim::TargetType::Invalid)
    return false;

  // Don't break early from this loop - we want to bring all turrets to
  // bear!(上游注释)
  bool b_turret_ready = false;
  for (Turreted* t : vec_turrets_)
    if (t->FaceTarget(self, target))
      b_turret_ready = true;

  return b_turret_ready && AttackFollow::CanAttack(self, target);
}


// ———— [VerifySync] 哈希注册(AttackFollow/AttackTurreted 各 {IsAiming})————
// ———— The [VerifySync] hash registrations (each of AttackFollow/
//      AttackTurreted carries {IsAiming}) ————

namespace {

int AttackFollowSyncHash(const sim::ISync* s) {
  const auto* follow = static_cast<const AttackFollow*>(s);
  return sim::sync::CombineSyncHash(
      0, sim::sync::HashBool(follow->IsAiming()));
}

int AttackTurretedSyncHash(const sim::ISync* s) {
  const auto* turreted = static_cast<const AttackTurreted*>(s);
  return sim::sync::CombineSyncHash(
      0, sim::sync::HashBool(turreted->IsAiming()));
}

const bool b_attack_follow_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.AttackFollow",
                                &AttackFollowSyncHash);
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.AttackTurreted",
                                &AttackTurretedSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_attack_follow_sync_registered_anchor =
    &b_attack_follow_sync_registered;

}  // namespace

}  // namespace ora::mods
