// UPSTREAM: OpenRA.Mods.Common/Activities/Attack.cs 实现部分
//          The implementation half.
#include "mods/attack_activity.hpp"

#include "core/wdist.hpp"
#include "mods/util.hpp"
#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods::activities {

namespace {

/// AttackStatus 的 [Flags] 组合/测试(上游 |= 与 >= 比较)
/// AttackStatus's [Flags] composition/test (upstream's |= and >=).
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

}  // namespace

Attack::Attack(sim::Actor& self, const sim::Target& target,
               bool allow_movement, bool force_attack,
               std::optional<core::Color> target_line_color)
    : b_force_attack_{force_attack},
      opt_target_line_color_{target_line_color},
      move_cooldown_helper_{self.world(), nullptr},
      target_{target} {
  // L49-89
  b_child_has_priority_ = false;

  for (AttackFrontal* t : self.TraitsImplementing<AttackFrontal>())
    if (!t->IsTraitDisabled())
      vec_attack_traits_.push_back(t);
  vec_reveals_shroud_ = self.TraitsImplementing<RevealsShroud>();
  facing_ = self.Trait<sim::IFacing>();
  positionable_ = self.Trait<sim::IPositionable>();

  sim::IMove* i_move = self.TraitOrDefault<sim::IMove>();
  mobile_ = dynamic_cast<Mobile*>(i_move);
  move_ = allow_movement ? i_move : nullptr;

  move_cooldown_helper_.SetMobile(mobile_);

  // The target may become hidden between the initial order request and the
  // first tick (e.g. if queued)(上游注释)
  if ((target.Type() == sim::TargetType::Actor &&
       target.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target.Type() == sim::TargetType::FrozenActor ||
      target.Type() == sim::TargetType::Terrain) {
    last_visible_target_ = sim::Target::FromPos(target.CenterPosition());

    // Lambdas can't use 'in' variables, so capture a copy for later
    // (上游注释)
    const sim::Target& range_target = target;
    WDist min_range = WDist::MaxValue();
    for (AttackFrontal* attack : vec_attack_traits_) {
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
  // L91-94
  return target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
}

bool Attack::Tick(sim::Actor& self) {
  // L96-156
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
    for (AttackFrontal* attack : vec_attack_traits_) {
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

  // Target is hidden or dead, and we don't have a fallback position to
  // move towards(上游注释)
  if (b_use_last_visible_target_ &&
      !last_visible_target_.IsValidFor(&self))
    return true;

  const WPos pos = self.CenterPosition();
  const sim::Target& check_target =
      b_use_last_visible_target_ ? last_visible_target_ : target_;

  // We don't know where the target actually is, so move to where we last
  // saw it(上游注释)
  if (b_use_last_visible_target_) {
    // We've reached the assumed position but it is not there or we can't
    // move any further - give up(上游注释)
    if (check_target.IsInRange(pos, dist_last_visible_maximum_range_) ||
        move_ == nullptr || dist_last_visible_maximum_range_ == WDist{0})
      return true;

    // Move towards the last known position
    move_cooldown_helper_.NotifyMoveQueued();
    QueueChild(move_->MoveWithinRange(
        target_, WDist{0}, dist_last_visible_maximum_range_,
        check_target.CenterPosition(), core::Color::FromArgb(255, 0, 0)));
    return false;
  }

  attack_status_ = AttackStatus::UnableToAttack;

  for (AttackFrontal* attack : vec_attack_traits_) {
    const AttackStatus status = TickAttack(self, attack);
    attack->b_is_aiming = status == AttackStatus::Attacking ||
                          status == AttackStatus::NeedsToTurn;
  }

  if (static_cast<std::int32_t>(attack_status_) >=
      static_cast<std::int32_t>(AttackStatus::NeedsToTurn))
    return false;

  return true;
}

void Attack::OnLastRun(sim::Actor& /*self*/) {
  // L158-162
  for (AttackFrontal* attack : vec_attack_traits_)
    attack->b_is_aiming = false;
}

Attack::AttackStatus Attack::TickAttack(sim::Actor& self,
                                        AttackFrontal* attack) {
  // L164-252
  if (!target_.IsValidFor(&self))
    return AttackStatus::UnableToAttack;

  if (attack->Info().b_attack_requires_entering_cell &&
      !positionable_->CanEnterCell(
          target_.ActorPtr != nullptr ? target_.ActorPtr->Location()
                                      : CPos{},
          nullptr, sim::BlockedByActor::None))
    return AttackStatus::UnableToAttack;

  if (!attack->Info().b_target_frozen_actors && !b_force_attack_ &&
      target_.Type() == sim::TargetType::FrozenActor) {
    // Try to move within range, drop the target otherwise(上游注释)
    if (move_ == nullptr)
      return AttackStatus::UnableToAttack;

    // revealsShroud.Where(!disabled).MaxByOrDefault(Range)
    RevealsShroud* rs = nullptr;
    for (RevealsShroud* s : vec_reveals_shroud_)
      if (!s->IsTraitDisabled() && (rs == nullptr ||
                                    s->Range() > rs->Range()))
        rs = s;

    // Default to 2 cells if there are no active traits(上游注释)
    const WDist sight_range =
        rs != nullptr ? rs->Range() : WDist{2 * 1024};

    attack_status_ = attack_status_ | AttackStatus::NeedsToMove;
    move_cooldown_helper_.NotifyMoveQueued();
    QueueChild(move_->MoveWithinRange(
        target_, sight_range, target_.CenterPosition(),
        core::Color::FromArgb(255, 0, 0)));
    return AttackStatus::NeedsToMove;
  }

  // Drop the target once none of the weapons are effective against it
  // (上游注释)
  const std::vector<Armament*> vec_armaments =
      attack->ChooseArmamentsForTarget(target_, b_force_attack_);
  if (vec_armaments.empty())
    return AttackStatus::UnableToAttack;

  // Update ranges. Exclude paused armaments except when ALL weapons are
  // paused (e.g. out of ammo), in which case use the paused, valid weapon
  // with highest range.(上游注释)
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
    // Try to move within range, drop the target otherwise(上游注释)
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
                                 attack->Info().angle_facing_tolerance)) {
    // Mirror Turn activity checks.(上游注释)
    if (mobile_ == nullptr ||
        (!mobile_->IsTraitDisabled() && !mobile_->IsTraitPaused())) {
      // Don't queue a Turn activity: Executing a child takes an additional
      // tick during which the target may have moved again.(上游注释)
      facing_->SetFacing(TickFacing(
          facing_->Facing(),
          (attack->GetTargetPosition(pos, target_) - pos).Yaw(),
          facing_->TurnSpeed()));

      // Check again if we turned enough and directly continue attacking if
      // we did.(上游注释)
      if (!attack->TargetInFiringArc(
              self, target_, attack->Info().angle_facing_tolerance)) {
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

void Attack::DoAttack(sim::Actor& self, AttackFrontal* attack,
                      const std::vector<Armament*>& vec_armaments) {
  // L254-259
  if (!attack->IsTraitPaused())
    for (Armament* a : vec_armaments)
      a->CheckFire(&self, facing_, target_);
}

void Attack::StanceChanged(sim::Actor& self, AutoTarget* auto_target,
                           UnitStance old_stance, UnitStance new_stance) {
  // L261-270
  // Cancel non-forced targets when switching to a more restrictive stance
  // if they are no longer valid for auto-targeting(上游注释)
  if (new_stance > old_stance || b_force_attack_)
    return;

  // If lastVisibleTarget is invalid we could never view the target in the
  // first place, so we just drop it here too(上游注释)
  if (!last_visible_target_.IsValidFor(&self) ||
      !auto_target->HasValidTargetPriority(
          self, p_last_visible_owner_, bitset_last_visible_target_types_))
    target_ = sim::Target::Invalid();
}

bool Attack::HasArmamentsFor(const sim::Target& target) {
  // L278-281
  for (AttackFrontal* attack : vec_attack_traits_)
    if (!attack->ChooseArmamentsForTarget(target, b_force_attack_)
             .empty())
      return true;
  return false;
}

}  // namespace ora::mods::activities
