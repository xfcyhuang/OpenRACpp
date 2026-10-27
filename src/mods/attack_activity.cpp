#include "mods/attack_activity.hpp"

#include "core/wdist.hpp"
#include "mods/util.hpp"
#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods::activities {

namespace {

constexpr Attack::AttackStatus operator|(Attack::AttackStatus a,
                                         Attack::AttackStatus b) {
  return static_cast<Attack::AttackStatus>(
      static_cast<std::int32_t>(a) | static_cast<std::int32_t>(b));
}

constexpr bool HasAttackStatus(Attack::AttackStatus a,
                               Attack::AttackStatus flag) {
  return (static_cast<std::int32_t>(a) & static_cast<std::int32_t>(flag)) !=
         0;
}

}

Attack::Attack(sim::Actor& self, const sim::Target& target,
               bool allow_movement, bool force_attack,
               std::optional<core::Color> target_line_color)
    : b_force_attack_{force_attack},
      opt_target_line_color_{target_line_color},
      move_cooldown_helper_{self.world(), nullptr},
      target_{target} {

  b_child_has_priority_ = false;

  for (AttackBaseFace* t : self.TraitsImplementing<AttackBaseFace>())
    if (!t->IsTraitDisabled())
      vec_attack_traits_.push_back(t);
  vec_reveals_shroud_ = self.TraitsImplementing<RevealsShroud>();
  facing_ = self.Trait<sim::IFacing>();
  positionable_ = self.Trait<sim::IPositionable>();

  sim::IMove* i_move = self.TraitOrDefault<sim::IMove>();
  mobile_ = dynamic_cast<Mobile*>(i_move);
  move_ = allow_movement ? i_move : nullptr;

  move_cooldown_helper_.SetMobile(mobile_);

  if ((target.Type() == sim::TargetType::Actor &&
       target.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target.Type() == sim::TargetType::FrozenActor ||
      target.Type() == sim::TargetType::Terrain) {
    last_visible_target_ = sim::Target::FromPos(target.CenterPosition());

    const sim::Target& range_target = target;
    WDist min_range = WDist::MaxValue();
    for (AttackBaseFace* attack : vec_attack_traits_) {
      const WDist range =
          attack->GetMaximumRangeVersusTarget(range_target);
      if (range < min_range)
        min_range = range;
    }
    dist_last_visible_maximum_range_ =
        min_range != WDist::MaxValue() ? min_range : WDist{0};

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

sim::Target Attack::RecalculateTarget(sim::Actor& self,
                                      bool& b_target_is_hidden_actor) {

  return target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
}

bool Attack::Tick(sim::Actor& self) {

  if (!IsCanceling() && !HasArmamentsFor(target_))
    Cancel(self, true);

  if (!TickChild(self))
    return false;

  if (IsCanceling())
    return true;

  bool b_target_is_hidden_actor = false;
  target_ = RecalculateTarget(self, b_target_is_hidden_actor);

  if (!b_target_is_hidden_actor &&
      target_.Type() == sim::TargetType::Actor) {
    last_visible_target_ = sim::Target::FromTargetPositions(target_);
    WDist min_range = WDist::MaxValue();
    for (AttackBaseFace* attack : vec_attack_traits_) {
      const WDist range = attack->GetMaximumRangeVersusTarget(target_);
      if (range < min_range)
        min_range = range;
    }
    dist_last_visible_maximum_range_ =
        min_range != WDist::MaxValue() ? min_range : WDist{0};

    p_last_visible_owner_ = target_.ActorPtr->Owner();
    bitset_last_visible_target_types_ =
        target_.ActorPtr->GetEnabledTargetTypes();
  }

  b_use_last_visible_target_ =
      b_target_is_hidden_actor || !target_.IsValidFor(&self);

  const std::optional<bool> result =
      move_cooldown_helper_.Tick(b_target_is_hidden_actor);
  if (result.has_value())
    return *result;

  if (b_use_last_visible_target_ &&
      !last_visible_target_.IsValidFor(&self))
    return true;

  const WPos pos = self.CenterPosition();
  const sim::Target& check_target =
      b_use_last_visible_target_ ? last_visible_target_ : target_;

  if (b_use_last_visible_target_) {

    if (check_target.IsInRange(pos, dist_last_visible_maximum_range_) ||
        move_ == nullptr || dist_last_visible_maximum_range_ == WDist{0})
      return true;

    move_cooldown_helper_.NotifyMoveQueued();
    QueueChild(move_->MoveWithinRange(
        target_, WDist{0}, dist_last_visible_maximum_range_,
        check_target.CenterPosition(), core::Color::FromArgb(255, 0, 0)));
    return false;
  }

  attack_status_ = AttackStatus::UnableToAttack;

  for (AttackBaseFace* attack : vec_attack_traits_) {
    const AttackStatus status = TickAttack(self, attack);
    attack->SetIsAiming(status == AttackStatus::Attacking ||
                       status == AttackStatus::NeedsToTurn);
  }

  if (static_cast<std::int32_t>(attack_status_) >=
      static_cast<std::int32_t>(AttackStatus::NeedsToTurn))
    return false;

  return true;
}

void Attack::OnLastRun(sim::Actor& ) {

  for (AttackBaseFace* attack : vec_attack_traits_)
    attack->SetIsAiming(false);
}

Attack::AttackStatus Attack::TickAttack(sim::Actor& self,
                                        AttackBaseFace* attack) {

  if (!target_.IsValidFor(&self))
    return AttackStatus::UnableToAttack;

  if (attack->AttackRequiresEnteringCell() &&
      !positionable_->CanEnterCell(
          target_.ActorPtr != nullptr ? target_.ActorPtr->Location()
                                      : CPos{},
          nullptr, sim::BlockedByActor::None))
    return AttackStatus::UnableToAttack;

  if (!attack->TargetFrozenActors() && !b_force_attack_ &&
      target_.Type() == sim::TargetType::FrozenActor) {

    if (move_ == nullptr)
      return AttackStatus::UnableToAttack;

    RevealsShroud* rs = nullptr;
    for (RevealsShroud* s : vec_reveals_shroud_)
      if (!s->IsTraitDisabled() && (rs == nullptr ||
                                    s->Range() > rs->Range()))
        rs = s;

    const WDist sight_range =
        rs != nullptr ? rs->Range() : WDist{2 * 1024};

    attack_status_ = attack_status_ | AttackStatus::NeedsToMove;
    move_cooldown_helper_.NotifyMoveQueued();
    QueueChild(move_->MoveWithinRange(
        target_, sight_range, target_.CenterPosition(),
        core::Color::FromArgb(255, 0, 0)));
    return AttackStatus::NeedsToMove;
  }

  const std::vector<Armament*> vec_armaments =
      attack->ChooseArmamentsForTarget(target_, b_force_attack_);
  if (vec_armaments.empty())
    return AttackStatus::UnableToAttack;

  bool b_has_active = false;
  WDist max_of_min_range = WDist{0};
  WDist min_of_max_range = WDist::MaxValue();
  for (Armament* a : vec_armaments)
    if (!a->IsTraitPaused()) {
      b_has_active = true;
      if (WDist{a->Weapon->int4_minRange} > max_of_min_range)
        max_of_min_range = WDist{a->Weapon->int4_minRange};
      if (a->MaxRange() < min_of_max_range)
        min_of_max_range = a->MaxRange();
    }
  if (b_has_active) {
    dist_min_range_ = max_of_min_range;
    dist_max_range_ = min_of_max_range;
  } else {
    dist_min_range_ = WDist{0};
    WDist max_paused = WDist{0};
    for (Armament* a : vec_armaments)
      if (a->MaxRange() > max_paused)
        max_paused = a->MaxRange();
    dist_max_range_ = max_paused;
  }

  const WPos pos = self.CenterPosition();
  if (!target_.IsInRange(pos, dist_max_range_) ||
      (dist_min_range_.Length != 0 &&
       target_.IsInRange(pos, dist_min_range_)) ||
      (mobile_ != nullptr && !mobile_->CanInteractWithGroundLayer(&self))) {

    if (move_ == nullptr)
      return AttackStatus::UnableToAttack;

    attack_status_ = attack_status_ | AttackStatus::NeedsToMove;
    move_cooldown_helper_.NotifyMoveQueued();
    const sim::Target& check_target =
        b_use_last_visible_target_ ? last_visible_target_ : target_;
    QueueChild(move_->MoveWithinRange(
        target_, dist_min_range_, dist_max_range_,
        check_target.CenterPosition(), core::Color::FromArgb(255, 0, 0)));
    return AttackStatus::NeedsToMove;
  }

  if (!attack->TargetInFiringArc(self, target_,
                                 attack->FacingTolerance())) {

    if (mobile_ == nullptr ||
        (!mobile_->IsTraitDisabled() && !mobile_->IsTraitPaused())) {

      facing_->SetFacing(TickFacing(
          facing_->Facing(),
          (attack->GetTargetPosition(pos, target_) - pos).Yaw(),
          facing_->TurnSpeed()));

      if (!attack->TargetInFiringArc(
              self, target_, attack->FacingTolerance())) {
        attack_status_ = attack_status_ | AttackStatus::NeedsToTurn;
        return AttackStatus::NeedsToTurn;
      }
    } else {
      attack_status_ = attack_status_ | AttackStatus::NeedsToTurn;
      return AttackStatus::NeedsToTurn;
    }
  }

  attack_status_ = attack_status_ | AttackStatus::Attacking;
  DoAttack(self, attack, vec_armaments);

  return AttackStatus::Attacking;
}

void Attack::DoAttack(sim::Actor& self, AttackBaseFace* attack,
                      const std::vector<Armament*>& vec_armaments) {

  if (!attack->IsTraitPaused())
    for (Armament* a : vec_armaments)
      a->CheckFire(&self, facing_, target_);
}

void Attack::StanceChanged(sim::Actor& self, AutoTarget* auto_target,
                           UnitStance old_stance, UnitStance new_stance) {

  if (new_stance > old_stance || b_force_attack_)
    return;

  if (!last_visible_target_.IsValidFor(&self) ||
      !auto_target->HasValidTargetPriority(
          self, p_last_visible_owner_, bitset_last_visible_target_types_))
    target_ = sim::Target::Invalid();
}

bool Attack::HasArmamentsFor(const sim::Target& target) {

  for (AttackBaseFace* attack : vec_attack_traits_)
    if (!attack->ChooseArmamentsForTarget(target, b_force_attack_)
             .empty())
      return true;
  return false;
}

}
