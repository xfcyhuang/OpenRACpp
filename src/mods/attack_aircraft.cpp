// UPSTREAM: OpenRA.Mods.Common/Traits/Air/AttackAircraft.cs +
//          AttackBomber.cs @1f594e320 + Activities/Air/FlyAttack.cs 的实现面
#include "mods/attack_aircraft.hpp"

#include "map/map.hpp"
#include "mods/air_activities.hpp"
#include "mods/air_support.hpp"
#include "mods/aircraft.hpp"
#include "mods/auto_target.hpp"
#include "mods/util.hpp"
#include "sim/actor_map.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

AttackAircraftInfoData AttackAircraftInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AttackAircraftInfoData data;
  static_cast<AttackFollowInfoData&>(data) =
      AttackFollowInfoData::Parse(rec_info);
  if (const auto v = sim::RecordFieldInt(rec_info, "AttackType"))
    data.kind_attack_type = static_cast<AirAttackType>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "StrafeRunLength"))
    data.dist_strafe_run_length = WDist{static_cast<std::int32_t>(*v)};
  return data;
}

AttackAircraft::AttackAircraft(ActorInitializer& init,
                               const AttackAircraftInfoData& info)
    : AttackFollow(init, info),
      attack_aircraft_info_{info} {
  ptr_aircraft_rec_ = sim::FindTraitInfoOfInterface(
      *init.Self().Info(), "OpenRA.Mods.Common.Traits.AircraftInfo");
}

sim::Activity* AttackAircraft::GetAttackActivity(
    Actor& self, AttackSource source, const sim::Target& new_target,
    bool allow_move, bool force_attack,
    std::optional<core::Color> target_line_color) {
  return self.world().Arena().Create<activities::FlyAttack>(
      self, source, new_target, force_attack, target_line_color);
}

bool AttackAircraft::CanAttack(Actor& self, const sim::Target& target) {
  const auto min_airborne_altitude =
      sim::RecordFieldInt(*ptr_aircraft_rec_, "MinAirborneAltitude")
          .value_or(1);
  if (self.world().Map().DistanceAboveTerrain(self.CenterPosition())
              .Length < min_airborne_altitude ||
      !self.world().Map().Contains(self.Location()))
    return false;

  if (!AttackFollow::CanAttack(self, target))
    return false;

  return TargetInFiringArc(self, target,
                           attack_aircraft_info_.angle_facing_tolerance);
}

AttackBomberInfoData AttackBomberInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AttackBomberInfoData data;
  static_cast<AttackBaseInfoData&>(data) =
      AttackBaseInfoData::Parse(rec_info);
  return data;
}

AttackBomber::AttackBomber(ActorInitializer& init,
                           const AttackBomberInfoData& info)
    : AttackBaseCore<AttackBomber>(init.Self(), info),
      info_{info} {}

void AttackBomber::AddOnRemovedFromWorld(OnRemovedFromWorldHandler handler) {
  vec_on_removed_from_world_.push_back(std::move(handler));
}

void AttackBomber::AddOnEnteredAttackRange(
    OnEnteredAttackRangeHandler handler) {
  vec_on_entered_attack_range_.push_back(std::move(handler));
}

void AttackBomber::AddOnExitedAttackRange(OnExitedAttackRangeHandler handler) {
  vec_on_exited_attack_range_.push_back(std::move(handler));
}

void AttackBomber::Tick(Actor& self) {
  const auto was_in_attack_range = b_in_attack_range_;
  b_in_attack_range_ = false;

  if (self.IsInWorld()) {
    const auto dat =
        self.world().Map().DistanceAboveTerrain(target_.CenterPosition());
    target_ = sim::Target::FromPos(
        target_.CenterPosition() - WVec{WDist{0}, WDist{0}, dat});

    const auto was_facing_target = b_facing_target_;
    b_facing_target_ =
        TargetInFiringArc(self, target_, info_.angle_facing_tolerance);

    for (auto* a : Armaments()) {
      if (!target_.IsInRange(self.CenterPosition(), a->MaxRange()))
        continue;

      b_in_attack_range_ = true;
      a->CheckFire(&self, self.Trait<sim::IFacing>(), target_);
    }

    if (Armaments().empty())
      b_in_attack_range_ =
          !b_in_attack_range_ && !b_facing_target_ && was_facing_target;
  }

  if (b_in_attack_range_ && !was_in_attack_range)
    for (auto& handler : vec_on_entered_attack_range_)
      handler(self);

  if (!b_in_attack_range_ && was_in_attack_range)
    for (auto& handler : vec_on_exited_attack_range_)
      handler(self);
}

void AttackBomber::RemovedFromWorld(Actor& self) {
  for (auto& handler : vec_on_removed_from_world_)
    handler(self);
}

void AttackBomber::SetTarget(WPos pos) {
  target_ = sim::Target::FromPos(pos);
}

sim::Activity* AttackBomber::GetAttackActivity(
    Actor&, AttackSource, const sim::Target&, bool, bool,
    std::optional<core::Color>) {
  throw std::runtime_error("AttackBomber requires a scripted target");
}

namespace activities {

FlyAttack::FlyAttack(sim::Actor& self, AttackSource source,
                     const sim::Target& target, bool force_attack,
                     std::optional<core::Color> target_line_color)
    : source_{source},
      b_force_attack_{force_attack},
      opt_target_line_color_{target_line_color},
      target_{target} {
  b_child_has_priority_ = false;

  aircraft_ = self.Trait<Aircraft>();
  attack_aircraft_ = self.Trait<AttackAircraft>();
  rearmable_ = self.TraitOrDefault<Rearmable>();

  dist_strafe_distance_ = attack_aircraft_->AttackAircraftInfo().dist_strafe_run_length;

  if ((target_.Type() == sim::TargetType::Actor &&
       target_.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target_.Type() == sim::TargetType::FrozenActor ||
      target_.Type() == sim::TargetType::Terrain) {
    last_visible_target_ = sim::Target::FromPos(target_.CenterPosition());
    dist_last_visible_maximum_range_ =
        attack_aircraft_->GetMaximumRangeVersusTarget(target_);

    if (target_.Type() == sim::TargetType::Actor) {
      p_last_visible_owner_ = target_.ActorPtr->Owner();
      bitset_last_visible_target_types_ =
          target_.ActorPtr->GetEnabledTargetTypes();
    } else if (target_.Type() == sim::TargetType::FrozenActor) {
      p_last_visible_owner_ = target_.FrozenActorPtr->Owner();
      bitset_last_visible_target_types_ =
          target_.FrozenActorPtr->TargetTypes();
    }
  }
}

bool FlyAttack::HasArmamentsFor(const sim::Target& target) {
  return !attack_aircraft_->IsTraitDisabled() &&
         !attack_aircraft_->ChooseArmamentsForTarget(target, b_force_attack_)
              .empty();
}

bool FlyAttack::Tick(sim::Actor& self) {
  if (!IsCanceling() && !HasArmamentsFor(target_))
    Cancel(self, true);

  if (!TickChild(self))
    return false;

  b_return_to_base_ = false;

  if (aircraft_->ForceLanding())
    Cancel(self);

  if (IsCanceling())
    return true;

  if (b_has_ticked_ &&
      attack_aircraft_->RequestedTarget().Type() == sim::TargetType::Invalid)
    return true;

  if (attack_aircraft_->IsTraitPaused())
    return false;

  bool b_target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
  attack_aircraft_->SetRequestedTarget(target_, b_force_attack_);
  b_has_ticked_ = true;

  if (!b_target_is_hidden_actor && target_.Type() == sim::TargetType::Actor) {
    last_visible_target_ = sim::Target::FromTargetPositions(target_);
    dist_last_visible_maximum_range_ =
        attack_aircraft_->GetMaximumRangeVersusTarget(target_);
    p_last_visible_owner_ = target_.ActorPtr->Owner();
    bitset_last_visible_target_types_ =
        target_.ActorPtr->GetEnabledTargetTypes();
  } else if (target_.Type() == sim::TargetType::FrozenActor &&
             !last_visible_target_.IsValidFor(&self)) {
    last_visible_target_ = sim::Target::FromTargetPositions(target_);
    dist_last_visible_maximum_range_ =
        attack_aircraft_->GetMaximumRangeVersusTarget(target_);
    p_last_visible_owner_ = target_.FrozenActorPtr->Owner();
    bitset_last_visible_target_types_ =
        target_.FrozenActorPtr->TargetTypes();
  }

  b_use_last_visible_target_ =
      b_target_is_hidden_actor || !target_.IsValidFor(&self);

  if (b_use_last_visible_target_ && !last_visible_target_.IsValidFor(&self))
    return true;

  if (rearmable_ != nullptr && !b_use_last_visible_target_) {
    auto all_invalid = true;
    for (auto* x : attack_aircraft_->Armaments()) {
      if (x->IsTraitPaused() ||
          !sim::WeaponIsValidAgainst(*x->Weapon, target_, self.world(), &self))
        continue;
      all_invalid = false;
    }
    if (all_invalid) {
      if (source_ == AttackSource::AttackMove)
        return true;

      if (attack_aircraft_->AttackAircraftInfo().b_abort_on_resupply) {
        if (NextActivity() != nullptr)
          NextActivity()->Cancel(self);
        Queue(self.world().Arena().Create<ReturnToBase>(self));
      } else
        QueueChild(self.world().Arena().Create<ReturnToBase>(self));

      b_return_to_base_ = true;
      return attack_aircraft_->AttackAircraftInfo().b_abort_on_resupply;
    }
  }

  const auto pos = self.CenterPosition();
  const auto& check_target =
      b_use_last_visible_target_ ? last_visible_target_ : target_;

  const auto minimum_range =
      attack_aircraft_->AttackAircraftInfo().kind_attack_type ==
              AirAttackType::Strafe
          ? WDist{0}
          : attack_aircraft_->GetMinimumRangeVersusTarget(target_);

  if (dist_last_visible_maximum_range_ == WDist{0} ||
      dist_last_visible_maximum_range_ < minimum_range)
    return true;

  const auto delta =
      attack_aircraft_->GetTargetPosition(pos, target_) - pos;
  const auto desired_facing =
      delta.HorizontalLengthSquared() != 0 ? WAngle{delta.Yaw()}
                                         : aircraft_->Facing();

  QueueChild(self.world().Arena().Create<TakeOff>(self));

  if (!check_target.IsInRange(pos, dist_last_visible_maximum_range_) ||
      check_target.IsInRange(pos, minimum_range))
    QueueChild(aircraft_->MoveWithinRange(
        target_, minimum_range, dist_last_visible_maximum_range_,
        check_target.CenterPosition(),
        std::optional<core::Color>{core::Color::FromArgb(255, 0, 0)}));

  else if (b_use_last_visible_target_)
    return true;

  else if (attack_aircraft_->AttackAircraftInfo().kind_attack_type ==
           AirAttackType::Strafe)
    QueueChild(self.world().Arena().Create<StrafeAttackRun>(
        *attack_aircraft_, *aircraft_, target_,
        !(dist_strafe_distance_ == WDist{0})
            ? dist_strafe_distance_
            : dist_last_visible_maximum_range_));
  else if (attack_aircraft_->AttackAircraftInfo().kind_attack_type ==
               AirAttackType::Default &&
           !aircraft_->Info().b_can_hover)
    QueueChild(self.world().Arena().Create<FlyAttackRun>(
        target_, dist_last_visible_maximum_range_, *attack_aircraft_));

  else if (!attack_aircraft_->TargetInFiringArc(
          self, target_,
          attack_aircraft_->AttackAircraftInfo().angle_facing_tolerance))
    aircraft_->SetFacing(TickFacing(aircraft_->Facing(), desired_facing,
                                    aircraft_->TurnSpeed()));

  return false;
}

void FlyAttack::OnLastRun(sim::Actor& self) {
  attack_aircraft_->ClearRequestedTarget();
}

void FlyAttack::StanceChanged(sim::Actor& self, AutoTarget* auto_target,
                              UnitStance old_stance, UnitStance new_stance) {
  if (new_stance > old_stance || b_force_attack_)
    return;

  if (!last_visible_target_.IsValidFor(&self) ||
      !auto_target->HasValidTargetPriority(
          self, p_last_visible_owner_, bitset_last_visible_target_types_))
    attack_aircraft_->ClearRequestedTarget();
}

FlyAttackRun::FlyAttackRun(const sim::Target& t, WDist exit_range,
                           AttackAircraft& attack)
    : attack_{attack},
      dist_exit_range_{exit_range},
      target_{t} {
  b_child_has_priority_ = false;
}

void FlyAttackRun::OnFirstRun(sim::Actor& self) {
  if (target_.IsValidFor(&self)) {
    QueueChild(self.world().Arena().Create<Fly>(
        self, target_, target_.CenterPosition()));

    QueueChild(self.world().Arena().Create<FlyForward>(self, 1));
    QueueChild(self.world().Arena().Create<Fly>(
        self, target_, dist_exit_range_, WDist::MaxValue(),
        target_.CenterPosition()));
  } else
    Cancel(self);
}

bool FlyAttackRun::Tick(sim::Actor& self) {
  if (TickChild(self) || IsCanceling())
    return true;

  const auto target_was_visible_actor = b_target_is_visible_actor_;
  bool b_target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
  b_target_is_visible_actor_ =
      target_.Type() == sim::TargetType::Actor && !b_target_is_hidden_actor;

  if (target_was_visible_actor &&
      (!target_.IsValidFor(&self) || !attack_.HasAnyValidWeapons(target_)))
    Cancel(self);

  return false;
}

StrafeAttackRun::StrafeAttackRun(AttackAircraft& attack_aircraft,
                                 Aircraft& aircraft, const sim::Target& t,
                                 WDist exit_range)
    : attack_aircraft_{attack_aircraft},
      aircraft_{aircraft},
      dist_exit_range_{exit_range},
      target_{t} {
  b_child_has_priority_ = false;
}

void StrafeAttackRun::OnFirstRun(sim::Actor& self) {
  if (target_.IsValidFor(&self)) {
    QueueChild(self.world().Arena().Create<Fly>(
        self, target_, target_.CenterPosition()));
    QueueChild(self.world().Arena().Create<FlyForward>(self, dist_exit_range_));

    const WDist distance_to_turn{
        aircraft_.Info().int4_speed * 256 /
        aircraft_.TurnSpeed().Angle};
    QueueChild(self.world().Arena().Create<Fly>(
        self, target_, dist_exit_range_ + distance_to_turn, WDist::MaxValue(),
        target_.CenterPosition()));
  } else
    Cancel(self);
}

bool StrafeAttackRun::Tick(sim::Actor& self) {
  if (TickChild(self) || IsCanceling())
    return true;

  bool b_target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
  if (!b_target_is_hidden_actor && target_.Type() == sim::TargetType::Actor)
    attack_aircraft_.SetRequestedTarget(
        sim::Target::FromTargetPositions(target_), true);

  return false;
}

}  // namespace activities

}  // namespace ora::mods
