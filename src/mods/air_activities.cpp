// UPSTREAM: OpenRA.Mods.Common/Activities/Air/Fly.cs @e180e3225 +
//          TakeOff.cs + Land.cs + FlyOffMap.cs + FlyIdle.cs +
//          FlyForward.cs + FlyFollow.cs + ReturnToBase.cs + FallToEarth.cs
//          + ../Resupply.cs 的实现面
#include "mods/air_activities.hpp"

#include "map/map.hpp"
#include "mods/actor_exts.hpp"
#include "mods/air_support.hpp"
#include "mods/aircraft.hpp"
#include "mods/concrete_support_powers.hpp"
#include "mods/health.hpp"
#include "mods/mobile.hpp"
#include "mods/player_resources.hpp"
#include "mods/production.hpp"
#include "mods/production_support.hpp"
#include "mods/util.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/shroud.hpp"
#include "sim/world.hpp"

namespace ora::mods::activities {

int ClampInt(int value, int lo, int hi) {
  return value < lo ? lo : (value > hi ? hi : value);
}

std::vector<std::string> FieldStringArray(const meta::RecordObject& rec_info,
                                          std::string_view str_name) {
  std::vector<std::string> vec_out;
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return vec_out;
  if (const auto* list =
          std::get_if<std::vector<meta::GenericValue>>(&gv->val))
    for (const auto& element : *list)
      if (const auto* s = std::get_if<std::string>(&element.val))
        vec_out.push_back(*s);
  return vec_out;
}

Fly::Fly(sim::Actor& self, const sim::Target& t, WDist near_enough,
         std::optional<WPos> initial_target_position,
         std::optional<core::Color> target_line_color)
    : Fly(self, t, initial_target_position, target_line_color) {
  dist_near_enough_ = near_enough;
}

Fly::Fly(sim::Actor& self, const sim::Target& t,
         std::optional<WPos> initial_target_position,
         std::optional<core::Color> target_line_color)
    : aircraft_{self.Trait<Aircraft>()},
      target_{t},
      opt_target_line_color_{target_line_color} {
  if ((target_.Type() == sim::TargetType::Actor &&
       target_.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target_.Type() == sim::TargetType::FrozenActor ||
      target_.Type() == sim::TargetType::Terrain)
    last_visible_target_ = sim::Target::FromPos(target_.CenterPosition());
  else if (initial_target_position.has_value())
    last_visible_target_ = sim::Target::FromPos(*initial_target_position);
}

Fly::Fly(sim::Actor& self, const sim::Target& t, WDist min_range,
         WDist max_range, std::optional<WPos> initial_target_position,
         std::optional<core::Color> target_line_color)
    : Fly(self, t, initial_target_position, target_line_color) {
  dist_max_range_ = max_range;
  dist_min_range_ = min_range;
}

void Fly::FlyTick(sim::Actor& self, Aircraft& aircraft,
                  WAngle desired_facing, WDist desired_altitude,
                  const WVec& move_override, bool b_idle_turn) {
  const auto dat =
      self.world().Map().DistanceAboveTerrain(aircraft.CenterPosition());
  auto move = move_override != WVec{0, 0, 0}
                  ? move_override
                  : (aircraft.Info().b_can_slide
                         ? aircraft.FlyStep(desired_facing)
                         : aircraft.FlyStep(aircraft.Facing()));

  const auto old_facing = aircraft.Facing();
  aircraft.SetFacing(
      TickFacing(aircraft.Facing(), desired_facing,
                 aircraft.GetTurnSpeed(b_idle_turn)));

  const auto roll =
      b_idle_turn ? aircraft.Info().opt_idle_roll.value_or(
                        aircraft.Info().angle_roll)
                  : aircraft.Info().angle_roll;
  if (roll.Angle != 0) {
    const auto desired_roll =
        aircraft.Facing() == desired_facing
            ? WAngle{0}
            : WAngle{static_cast<std::int32_t>(
                  static_cast<std::int64_t>(roll.Angle) *
                  GetTurnDirection(aircraft.Facing(), old_facing))};

    aircraft.SetRoll(TickFacing(aircraft.Roll(), desired_roll,
                                aircraft.Info().angle_roll_speed));
  }

  if (aircraft.Info().angle_pitch.Angle != 0)
    aircraft.SetPitch(TickFacing(aircraft.Pitch(), aircraft.Info().angle_pitch,
                                 aircraft.Info().angle_pitch_speed));

  if (!(dat == desired_altitude) || move.Z != 0) {
    const auto max_delta =
        move.HorizontalLength() * aircraft.Info().angle_maximum_pitch.Tan() / 1024;
    const auto move_z = move.Z != 0 ? move.Z
                                    : (desired_altitude.Length - dat.Length);
    const auto delta_z = ClampInt(move_z, -max_delta, max_delta);
    move = WVec{move.X, move.Y, delta_z};
  }

  aircraft.SetPosition(&self, aircraft.CenterPosition() + move);
}

void Fly::FlyTick(sim::Actor& self, Aircraft& aircraft,
                  WAngle desired_facing, WDist desired_altitude,
                  bool b_idle_turn) {
  FlyTick(self, aircraft, desired_facing, desired_altitude, WVec{0, 0, 0},
          b_idle_turn);
}

bool Fly::VerticalTakeOffOrLandTick(sim::Actor& self, Aircraft& aircraft,
                                    WAngle desired_facing,
                                    WDist desired_altitude,
                                    bool b_idle_turn) {
  const auto turn_speed =
      b_idle_turn
          ? aircraft.IdleTurnSpeed().value_or(aircraft.TurnSpeed())
          : aircraft.TurnSpeed();
  aircraft.SetFacing(
      TickFacing(aircraft.Facing(), desired_facing, turn_speed));

  const auto dat =
      self.world().Map().DistanceAboveTerrain(aircraft.CenterPosition());
  if (dat == desired_altitude)
    return false;

  const auto max_delta = aircraft.Info().dist_altitude_velocity.Length;
  const auto delta_z =
      ClampInt(desired_altitude.Length - dat.Length, -max_delta, max_delta);
  aircraft.SetPosition(&self,
                       aircraft.CenterPosition() + WVec{0, 0, delta_z});
  return true;
}

int Fly::CalculateTurnRadius(int speed, WAngle turn_speed) {
  return turn_speed.Angle > 0 ? 180 * speed / turn_speed.Angle : 0;
}

bool Fly::Tick(sim::Actor& self) {
  if (aircraft_->ForceLanding())
    Cancel(self);

  const auto dat =
      self.world().Map().DistanceAboveTerrain(aircraft_->CenterPosition());
  const auto is_landed = dat <= aircraft_->LandAltitude();

  if (is_landed && aircraft_->IsTraitPaused())
    return false;

  if (IsCanceling()) {
    const auto land_when_idle =
        aircraft_->Info().kind_idle_behavior == IdleBehaviorType::Land;
    const auto skip_height_adjustment =
        land_when_idle && self.CurrentActivity()->IsCanceling() &&
        self.CurrentActivity()->NextActivity() == nullptr;
    if (aircraft_->Info().b_can_hover && !skip_height_adjustment &&
        !(dat == aircraft_->Info().dist_cruise_altitude)) {
      if (is_landed)
        QueueChild(self.world().Arena().Create<TakeOff>(self));
      else
        VerticalTakeOffOrLandTick(self, *aircraft_, aircraft_->Facing(),
                                  aircraft_->Info().dist_cruise_altitude);

      return false;
    }

    return true;
  } else if (is_landed) {
    QueueChild(self.world().Arena().Create<TakeOff>(self));
    return false;
  }

  bool b_target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
  if (!b_target_is_hidden_actor && target_.Type() == sim::TargetType::Actor)
    last_visible_target_ = sim::Target::FromTargetPositions(target_);

  b_use_last_visible_target_ =
      b_target_is_hidden_actor || !target_.IsValidFor(&self);

  if (b_use_last_visible_target_ && !last_visible_target_.IsValidFor(&self))
    return true;

  const auto& check_target =
      b_use_last_visible_target_ ? last_visible_target_ : target_;
  const auto pos = self.CenterPosition();
  const auto delta = check_target.CenterPosition() - pos;

  const auto inside_max_range =
      dist_max_range_.Length > 0 && check_target.IsInRange(pos, dist_max_range_);
  const auto inside_min_range =
      dist_min_range_.Length > 0 && check_target.IsInRange(pos, dist_min_range_);
  if (inside_max_range && !inside_min_range)
    return true;

  const auto is_slider = aircraft_->Info().b_can_slide;

  auto desired_facing = aircraft_->Facing();
  if (delta.HorizontalLengthSquared() != 0) {
    const auto facing = WAngle{delta.Yaw()};

    const auto diff = std::abs(facing.Angle - desired_facing.Angle);
    const auto deadzone = aircraft_->Info().angle_turn_deadzone.Angle;
    if (diff > deadzone && diff < 1024 - deadzone)
      desired_facing = facing;
  }

  const auto move = is_slider ? aircraft_->FlyStep(desired_facing)
                              : aircraft_->FlyStep(aircraft_->Facing());

  if (inside_min_range) {
    if (is_slider)
      FlyTick(self, *aircraft_, desired_facing,
              aircraft_->Info().dist_cruise_altitude, -move);
    else
      FlyTick(self, *aircraft_, desired_facing + WAngle{512},
              aircraft_->Info().dist_cruise_altitude, move);

    return false;
  }

  if (n_previous_positions_count_ == arr_previous_positions_.size() &&
      (arr_previous_positions_[n_previous_positions_head_] -
       arr_previous_positions_[(n_previous_positions_head_ + 4) % 5])
              .LengthSquared() < 4096 &&
      delta.HorizontalLengthSquared() <= dist_near_enough_.LengthSquared())
    return true;

  if (delta.HorizontalLengthSquared() < move.HorizontalLengthSquared()) {
    if (is_slider || aircraft_->Info().b_vtol) {
      if (delta.HorizontalLengthSquared() != 0) {
        const WVec delta_move{delta.X, delta.Y, 0};
        FlyTick(self, *aircraft_, desired_facing, dat, delta_move);
      }

      if (!(dat == aircraft_->Info().dist_cruise_altitude)) {
        VerticalTakeOffOrLandTick(self, *aircraft_, aircraft_->Facing(),
                                  aircraft_->Info().dist_cruise_altitude);
        return false;
      }
    }

    return true;
  }

  if (!is_slider) {
    const auto turn_radius =
        CalculateTurnRadius(aircraft_->MovementSpeed(), aircraft_->TurnSpeed());

    const auto turn_center_facing =
        aircraft_->Facing() +
        WAngle{GetTurnDirection(aircraft_->Facing(), desired_facing) * 256};

    auto turn_center_dir =
        WVec{0, -1024, 0}.Rotate(WRot::FromYaw(turn_center_facing));
    turn_center_dir = turn_radius * turn_center_dir;
    turn_center_dir = turn_center_dir / 1024;

    const auto turn_center = aircraft_->CenterPosition() + turn_center_dir;
    if ((check_target.CenterPosition() - turn_center).HorizontalLengthSquared() <
        turn_radius * turn_radius)
      desired_facing = aircraft_->Facing();
  }

  arr_previous_positions_[n_previous_positions_head_] = self.CenterPosition();
  n_previous_positions_head_ =
      (n_previous_positions_head_ + 1) % arr_previous_positions_.size();
  if (n_previous_positions_count_ < arr_previous_positions_.size())
    n_previous_positions_count_++;

  FlyTick(self, *aircraft_, desired_facing,
          aircraft_->Info().dist_cruise_altitude, WVec{0, 0, 0});

  return false;
}

TakeOff::TakeOff(sim::Actor& self)
    : aircraft_{self.Trait<Aircraft>()} {}

void TakeOff::OnFirstRun(sim::Actor& self) {
  if (aircraft_->ForceLanding())
    return;

  if (!aircraft_->HasInfluence())
    return;

  aircraft_->RemoveInfluence();

  if (self.world().Map().DistanceAboveTerrain(aircraft_->CenterPosition())
          .Length > aircraft_->Info().int4_min_airborne_altitude)
    return;

  if (!aircraft_->Info().vec_takeoff_sounds.empty())
    PlaySupportPowerSoundIfWired(
        aircraft_->Info().vec_takeoff_sounds.front(), self.world(),
        aircraft_->CenterPosition());

  for (auto* notify : self.TraitsImplementing<sim::INotifyTakeOff>())
    notify->TakeOff(self);
}

bool TakeOff::Tick(sim::Actor& self) {
  if (aircraft_->ForceLanding()) {
    Cancel(self);
    return true;
  }

  const auto dat =
      self.world().Map().DistanceAboveTerrain(aircraft_->CenterPosition());
  if (dat < aircraft_->Info().dist_cruise_altitude) {
    if (aircraft_->Info().b_vtol) {
      Fly::VerticalTakeOffOrLandTick(self, *aircraft_, aircraft_->Facing(),
                                      aircraft_->Info().dist_cruise_altitude);
      return false;
    }

    Fly::FlyTick(self, *aircraft_, aircraft_->Facing(),
                 aircraft_->Info().dist_cruise_altitude);
    return false;
  }

  return true;
}

Land::Land(sim::Actor& self, std::optional<WAngle> facing,
           std::optional<core::Color> target_line_color)
    : Land(self, sim::Target::Invalid(), WDist{-1}, WVec{0, 0, 0}, facing,
           std::vector<CPos>{}, target_line_color) {
  b_assign_target_on_first_run_ = true;
}

Land::Land(sim::Actor& self, const sim::Target& target,
           std::optional<WAngle> facing,
           std::optional<core::Color> target_line_color)
    : Land(self, target, WDist{-1}, WVec{0, 0, 0}, facing, std::vector<CPos>{},
           target_line_color) {}

Land::Land(sim::Actor& self, const sim::Target& target, WDist land_range,
           std::optional<WAngle> facing,
           std::optional<core::Color> target_line_color)
    : Land(self, target, land_range, WVec{0, 0, 0}, facing, std::vector<CPos>{},
           target_line_color) {}

Land::Land(sim::Actor& self, const sim::Target& target, const WVec& offset,
           std::optional<WAngle> facing,
           std::optional<core::Color> target_line_color)
    : Land(self, target, WDist{0}, offset, facing, std::vector<CPos>{},
           target_line_color) {}

Land::Land(sim::Actor& self, const sim::Target& target, WDist land_range,
           const WVec& offset, std::optional<WAngle> facing,
           std::vector<CPos> clear_cells,
           std::optional<core::Color> target_line_color)
    : aircraft_{self.Trait<Aircraft>()},
      wvec_offset_{offset},
      vec_clear_cells_{std::move(clear_cells)},
      dist_land_range_{land_range.Length >= 0 ? land_range
                                                : self.Trait<Aircraft>()
                                                      ->Info()
                                                      .dist_land_range},
      opt_target_line_color_{target_line_color} {
  if (!facing.has_value() && aircraft_->Info().b_turn_to_land)
    opt_desired_facing_ = aircraft_->Info().angle_initial_facing;
  else
    opt_desired_facing_ = facing;
}

void Land::OnFirstRun(sim::Actor& self) {
  if (b_assign_target_on_first_run_)
    target_ = sim::Target::FromCell(self.world(), self.Location());
}

bool Land::Tick(sim::Actor& self) {
  if (IsCanceling() || target_.Type() == sim::TargetType::Invalid) {
    if (b_landing_initiated_) {
      const auto should_land =
          aircraft_->Info().kind_idle_behavior == IdleBehaviorType::Land;
      const auto continue_landing =
          should_land && self.CurrentActivity()->IsCanceling() &&
          self.CurrentActivity()->NextActivity() == nullptr;
      if (!continue_landing) {
        const auto dat =
            self.world().Map().DistanceAboveTerrain(
                aircraft_->CenterPosition());
        if (dat > aircraft_->LandAltitude() &&
            dat < aircraft_->Info().dist_cruise_altitude) {
          QueueChild(self.world().Arena().Create<TakeOff>(self));
          return false;
        }

        aircraft_->RemoveInfluence();
        return true;
      }
    } else
      return true;
  }

  auto pos = self.CenterPosition();

  wpos_target_position_ = target_.CenterPosition() + wvec_offset_;
  cell_landing_ = self.world().Map().CellContaining(wpos_target_position_);

  auto delta = pos - wpos_target_position_;
  if (delta.HorizontalLengthSquared() == 0 &&
      delta.Z == aircraft_->LandAltitude().Length)
    return true;

  if (target_.Type() == sim::TargetType::Terrain && !b_landing_initiated_) {
    const auto new_location =
        aircraft_->FindLandingLocation(cell_landing_, dist_land_range_);

    if (!new_location.has_value()) {
      QueueChild(aircraft_->MoveTo(cell_landing_, 0, nullptr, false,
                                   std::nullopt));
      return true;
    }

    if (!(*new_location == cell_landing_)) {
      target_ = sim::Target::FromCell(self.world(), *new_location);
      wpos_target_position_ = target_.CenterPosition() + wvec_offset_;
      cell_landing_ = self.world().Map().CellContaining(wpos_target_position_);

      delta = pos - wpos_target_position_;
      if (delta.HorizontalLengthSquared() == 0 &&
          delta.Z == aircraft_->LandAltitude().Length)
        return true;
    }
  }

  if (aircraft_->Info().b_vtol) {
    if ((pos - wpos_target_position_).HorizontalLengthSquared() != 0) {
      QueueChild(self.world().Arena().Create<Fly>(
          self, sim::Target::FromPos(wpos_target_position_)));
      return false;
    }

    if (opt_desired_facing_.has_value() &&
        !(opt_desired_facing_.value() == aircraft_->Facing())) {
      QueueChild(self.world().Arena().Create<Turn>(
          self, opt_desired_facing_.value()));
      return false;
    }
  }

  if (!aircraft_->Info().b_vtol && !b_finished_approach_) {
    const auto altitude = aircraft_->Info().dist_cruise_altitude.Length;

    const auto land_distance =
        altitude * 1024 / aircraft_->Info().angle_maximum_pitch.Tan();

    auto rotation = WRot::None();
    if (opt_desired_facing_.has_value())
      rotation = WRot::FromYaw(opt_desired_facing_.value());

    const auto approach_start =
        wpos_target_position_ +
        WVec{0, land_distance, altitude}.Rotate(rotation);

    const auto speed = aircraft_->MovementSpeed() * 32 / 35;
    auto turn_radius = Fly::CalculateTurnRadius(speed, aircraft_->TurnSpeed());

    const auto angle = aircraft_->Facing();
    const WVec fwd{-angle.Sin(), -angle.Cos(), 0};

    const WVec side{-fwd.Y, fwd.X, fwd.Z};
    const auto approach_delta = self.CenterPosition() - approach_start;
    const WVec side_toward_base =
        WVec::Dot(side, approach_delta) <= WVec::Dot(-side, approach_delta)
            ? side
            : WVec{-side.X, -side.Y, -side.Z};

    const auto cp = self.CenterPosition() + turn_radius * side_toward_base / 1024;
    const WPos pos_center{cp.X, cp.Y, altitude};
    const auto approach_center =
        approach_start +
        WVec{0, turn_radius * (self.CenterPosition().Y > approach_start.Y ? 1 : -1),
             0};
    const auto tangent_direction = approach_center - pos_center;
    const auto tangent_length = tangent_direction.Length();
    auto tangent_offset = WVec{0, 0, 0};
    if (tangent_length != 0)
      tangent_offset =
          WVec{-tangent_direction.Y, tangent_direction.X, 0} * turn_radius /
          tangent_length;

    if (tangent_offset.X > 0)
      tangent_offset = -tangent_offset;

    const auto w1 = pos_center + tangent_offset;
    const auto w2 = approach_center + tangent_offset;
    const auto w3 = approach_start;

    turn_radius = Fly::CalculateTurnRadius(aircraft_->Info().int4_speed,
                                           aircraft_->TurnSpeed());

    QueueChild(self.world().Arena().Create<Fly>(
        self, sim::Target::FromPos(w1), WDist{0}, WDist{turn_radius * 3}));
    QueueChild(self.world().Arena().Create<Fly>(self,
                                                sim::Target::FromPos(w2)));

    QueueChild(self.world().Arena().Create<Fly>(
        self, sim::Target::FromPos(w3), WDist{0}, WDist{turn_radius / 2}));
    b_finished_approach_ = true;
    return false;
  }

  if (!b_landing_initiated_) {
    std::vector<CPos> vec_blocking_cells = vec_clear_cells_;
    vec_blocking_cells.push_back(cell_landing_);

    if (!aircraft_->CanLand(
            std::span<const CPos>{vec_blocking_cells},
            const_cast<Actor*>(target_.ActorPtr))) {
      QueueChild(self.world().Arena().Create<FlyIdle>(self, 25));

      for (const auto& blocking_cell : vec_blocking_cells)
        NotifyBlocker(self, blocking_cell);
      b_finished_approach_ = false;
      return false;
    }

    if (!aircraft_->Info().vec_landing_sounds.empty())
      PlaySupportPowerSoundIfWired(
          aircraft_->Info().vec_landing_sounds.front(), self.world(),
          aircraft_->CenterPosition());

    for (auto* notify : self.TraitsImplementing<sim::INotifyLanding>())
      notify->Landing(self);

    aircraft_->AddInfluence(cell_landing_);
    aircraft_->EnteringCell(self);
    b_landing_initiated_ = true;
  }

  if (aircraft_->Info().b_vtol) {
    const auto land_altitude =
        self.world().Map().DistanceAboveTerrain(wpos_target_position_) +
        aircraft_->LandAltitude();
    if (Fly::VerticalTakeOffOrLandTick(self, *aircraft_, aircraft_->Facing(),
                                        land_altitude))
      return false;

    return true;
  }

  const auto d = wpos_target_position_ - pos;

  const auto move = aircraft_->FlyStep(aircraft_->Facing());
  if (d.HorizontalLengthSquared() < move.HorizontalLengthSquared()) {
    const WVec landing_alt_vec{WDist{0}, WDist{0}, aircraft_->LandAltitude()};
    aircraft_->SetPosition(&self, wpos_target_position_ + landing_alt_vec);
    return true;
  }

  const auto landing_alt =
      self.world().Map().DistanceAboveTerrain(wpos_target_position_) +
      aircraft_->LandAltitude();
  Fly::FlyTick(self, *aircraft_, WAngle{d.Yaw()}, landing_alt);

  return false;
}

FlyOffMap::FlyOffMap(sim::Actor& self, int ending_delay)
    : aircraft_{self.Trait<Aircraft>()}, int4_ending_delay_{ending_delay} {
  b_child_has_priority_ = false;
}

FlyOffMap::FlyOffMap(sim::Actor& self, const sim::Target& target,
                     int ending_delay)
    : FlyOffMap(self, ending_delay) {
  target_ = target;
  b_has_target_ = true;
}

void FlyOffMap::OnFirstRun(sim::Actor& self) {
  if (b_has_target_) {
    QueueChild(self.world().Arena().Create<Fly>(self, target_));
    QueueChild(self.world().Arena().Create<FlyForward>(self));
    return;
  }

  if (aircraft_->Info().b_vtol &&
      !(self.world().Map().DistanceAboveTerrain(aircraft_->CenterPosition()) ==
        aircraft_->Info().dist_cruise_altitude))
    QueueChild(self.world().Arena().Create<TakeOff>(self));

  QueueChild(self.world().Arena().Create<FlyForward>(self));
}

bool FlyOffMap::Tick(sim::Actor& self) {
  if (aircraft_->ForceLanding())
    Cancel(self);

  if (IsCanceling())
    return true;

  if (!self.world().Map().Contains(self.Location()) &&
      --int4_ending_delay_ < 0) {
    if (ChildActivity() != nullptr)
      ChildActivity()->Cancel(self);
  }

  return TickChild(self);
}

FlyIdle::FlyIdle(sim::Actor& self, int ticks, bool idle_turn)
    : aircraft_{self.Trait<Aircraft>()},
      int4_remaining_ticks_{ticks},
      b_idle_turn_{idle_turn} {
  b_is_idle_turner_ = aircraft_->Info().int4_idle_speed > 0 ||
                      (!aircraft_->Info().b_can_hover &&
                       aircraft_->Info().int4_idle_speed < 0);

  if (idle_turn) {
    for (auto* ti : self.TraitsImplementing<sim::INotifyIdle>())
      vec_tick_idles_.push_back(ti);
    b_tick_idles_loaded_ = true;
  }
}

bool FlyIdle::Tick(sim::Actor& self) {
  if (int4_remaining_ticks_ == 0 ||
      (NextActivity() != nullptr && int4_remaining_ticks_ < 0))
    return true;

  if (aircraft_->ForceLanding() || IsCanceling())
    return true;

  if (int4_remaining_ticks_ > 0)
    int4_remaining_ticks_--;

  if (b_tick_idles_loaded_)
    for (auto* tick_idle : vec_tick_idles_)
      tick_idle->TickIdle(self);

  if (b_is_idle_turner_) {
    const auto move =
        aircraft_->FlyStep(aircraft_->IdleMovementSpeed(), aircraft_->Facing());

    const auto desired_facing = aircraft_->Facing() + WAngle{256};
    Fly::FlyTick(self, *aircraft_, desired_facing,
                 aircraft_->Info().dist_cruise_altitude, move, true);
  }

  return false;
}

FlyForward::FlyForward(sim::Actor& self, int ticks)
    : aircraft_{self.Trait<Aircraft>()},
      dist_cruise_altitude_{self.Trait<Aircraft>()->Info().dist_cruise_altitude},
      int4_fly_ticks_{ticks} {}

FlyForward::FlyForward(sim::Actor& self, WDist distance)
    : aircraft_{self.Trait<Aircraft>()},
      dist_cruise_altitude_{self.Trait<Aircraft>()->Info().dist_cruise_altitude},
      int4_fly_ticks_{0},
      int4_remaining_distance_{distance.Length} {}

bool FlyForward::Tick(sim::Actor& self) {
  if (aircraft_->ForceLanding()) {
    Cancel(self);
    return true;
  }

  if (IsCanceling() || (int4_fly_ticks_ > 0 && int4_ticks_++ >= int4_fly_ticks_) ||
      (int4_fly_ticks_ == 0 && int4_remaining_distance_ <= 0))
    return true;

  if (int4_remaining_distance_ != 0)
    int4_remaining_distance_ -=
        aircraft_->FlyStep(aircraft_->Facing()).HorizontalLength();

  Fly::FlyTick(self, *aircraft_, aircraft_->Facing(), dist_cruise_altitude_);
  return false;
}

FlyFollow::FlyFollow(sim::Actor& self, const sim::Target& target,
                     WDist min_range, WDist max_range,
                     std::optional<WPos> initial_target_position,
                     std::optional<core::Color> target_line_color)
    : aircraft_{self.Trait<Aircraft>()},
      dist_min_range_{min_range},
      dist_max_range_{max_range},
      opt_target_line_color_{target_line_color},
      target_{target} {
  if ((target_.Type() == sim::TargetType::Actor &&
       target_.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target_.Type() == sim::TargetType::FrozenActor ||
      target_.Type() == sim::TargetType::Terrain)
    last_visible_target_ = sim::Target::FromPos(target_.CenterPosition());
  else if (initial_target_position.has_value())
    last_visible_target_ = sim::Target::FromPos(*initial_target_position);
}

bool FlyFollow::Tick(sim::Actor& self) {
  if (aircraft_->ForceLanding())
    Cancel(self);

  if (IsCanceling())
    return true;

  bool b_target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
  if (!b_target_is_hidden_actor && target_.Type() == sim::TargetType::Actor)
    last_visible_target_ = sim::Target::FromTargetPositions(target_);

  b_use_last_visible_target_ =
      b_target_is_hidden_actor || !target_.IsValidFor(&self);

  if (b_was_moving_within_range_ && b_target_is_hidden_actor)
    return true;

  b_was_moving_within_range_ = false;

  if (b_use_last_visible_target_ && !last_visible_target_.IsValidFor(&self))
    return true;

  const auto pos = self.CenterPosition();
  const auto& check_target =
      b_use_last_visible_target_ ? last_visible_target_ : target_;

  if (check_target.IsInRange(pos, dist_max_range_) &&
      !check_target.IsInRange(pos, dist_min_range_)) {
    if (!aircraft_->Info().b_can_hover)
      Fly::FlyTick(self, *aircraft_, aircraft_->Facing(),
                   aircraft_->Info().dist_cruise_altitude);

    return b_use_last_visible_target_;
  }

  b_was_moving_within_range_ = true;
  QueueChild(aircraft_->MoveWithinRange(
      target_, dist_min_range_, dist_max_range_,
      check_target.CenterPosition(), opt_target_line_color_));
  return false;
}

ReturnToBase::ReturnToBase(sim::Actor& self, Actor* dest, bool always_land)
    : b_always_land_{always_land},
      p_dest_{dest},
      aircraft_{self.Trait<Aircraft>()},
      rearmable_{self.TraitOrDefault<Rearmable>()} {
  const auto* repairable_rec = sim::FindTraitInfoOfInterface(
      *self.Info(), "OpenRA.Mods.Common.Traits.RepairableInfo");
  if (repairable_rec != nullptr) {
    has_repairable_info_ = true;
    vec_repair_actors_info_ = FieldStringArray(*repairable_rec, "RepairActors");
  }
}

Actor* ReturnToBase::ChooseResupplier(sim::Actor& self, bool unreserved_only) {
  const auto* rearm_rec = sim::FindTraitInfoOfInterface(
      *self.Info(), "OpenRA.Mods.Common.Traits.RearmableInfo");
  if (rearm_rec == nullptr)
    return nullptr;

  std::vector<std::string> vec_rearm_actors;
  vec_rearm_actors = FieldStringArray(*rearm_rec, "RearmActors");

  Actor* best = nullptr;
  std::int64_t best_dist = 0;
  bool has_best = false;
  for (auto* a : self.world().ActorsHavingTrait<Reservable>()) {
    if (!a->IsDead() && a->Owner() == self.Owner() &&
        std::find(vec_rearm_actors.begin(), vec_rearm_actors.end(),
                  a->InfoName()) != vec_rearm_actors.end() &&
        (!unreserved_only || Reservable::IsAvailableFor(*a, &self))) {
      const auto dist =
          (a->CenterPosition() - self.CenterPosition()).LengthSquared();
      if (!has_best || dist < best_dist) {
        best = a;
        best_dist = dist;
        has_best = true;
      }
    }
  }
  return best;
}

bool ReturnToBase::ShouldLandAtBuilding(sim::Actor& self, Actor& dest) {
  if (b_always_land_)
    return true;

  if (has_repairable_info_ &&
      std::find(vec_repair_actors_info_.begin(),
                vec_repair_actors_info_.end(),
                dest.InfoName()) != vec_repair_actors_info_.end() &&
      self.Trait<Health>()->DamageState() != sim::DamageState::Undamaged)
    return true;

  if (rearmable_ != nullptr &&
      std::find(rearmable_->Info().vec_rearm_actors.begin(),
                rearmable_->Info().vec_rearm_actors.end(),
                dest.InfoName()) !=
          rearmable_->Info().vec_rearm_actors.end()) {
    for (const auto* pool : rearmable_->RearmableAmmoPools())
      if (!pool->HasFullAmmo())
        return true;
  }
  return false;
}

bool ReturnToBase::Tick(sim::Actor& self) {
  if (aircraft_->ForceLanding())
    return true;

  if (IsCanceling() || self.IsDead())
    return true;

  if (p_dest_ == nullptr || p_dest_->IsDead() ||
      !Reservable::IsAvailableFor(*p_dest_, &self))
    p_dest_ = ChooseResupplier(self, true);

  if (p_dest_ == nullptr) {
    auto* nearest_resupplier = ChooseResupplier(self, false);

    if (nearest_resupplier != nullptr) {
      if (aircraft_->Info().b_can_hover) {
        const auto distance_from_resupplier =
            (nearest_resupplier->CenterPosition() - self.CenterPosition())
                .HorizontalLength();
        const auto distance_length =
            aircraft_->Info().dist_wait_distance_from_resupply_base.Length;

        if (distance_from_resupplier > distance_length) {
          const auto random_position =
              WVec::FromPDF(self.world().SharedRandom(), 2) * distance_length /
              1024;
          const auto target = sim::Target::FromPos(
              nearest_resupplier->CenterPosition() + random_position);

          QueueChild(self.world().Arena().Create<Fly>(
              self, target, WDist{0},
              aircraft_->Info().dist_wait_distance_from_resupply_base,
              std::nullopt, std::optional<core::Color>{
                               core::Color::FromArgb(0, 128, 0)}));
        }

        return false;
      }

      QueueChild(self.world().Arena().Create<Fly>(
          self, sim::Target::FromActor(nearest_resupplier), WDist{0},
          aircraft_->Info().dist_wait_distance_from_resupply_base,
          std::nullopt,
          std::optional<core::Color>{core::Color::FromArgb(0, 128, 0)}));
      QueueChild(self.world().Arena().Create<FlyIdle>(
          self, aircraft_->Info().int4_number_of_ticks_to_verify_available_airport));
      return false;
    }

    QueueChild(self.world().Arena().Create<FlyIdle>(
        self, aircraft_->Info().int4_number_of_ticks_to_verify_available_airport));
    return true;
  }

  if (ShouldLandAtBuilding(self, *p_dest_)) {
    auto* exit = NearestExitOrDefault(*p_dest_, self.CenterPosition(), "");
    auto offset = WVec{0, 0, 0};
    if (exit != nullptr) {
      offset = exit->Info().spawn_offset;
      opt_facing_ = exit->Info().opt_facing;
    }

    aircraft_->MakeReservation(*p_dest_);
    QueueChild(aircraft_->MoveOntoTarget(
        &self, sim::Target::FromActor(p_dest_), offset, opt_facing_,
        std::optional<core::Color>{core::Color::FromArgb(0, 128, 0)}));
    QueueChild(self.world().Arena().Create<Resupply>(
        self, *p_dest_, WDist{0}, b_always_land_));
    return true;
  }

  QueueChild(self.world().Arena().Create<Fly>(
      self, sim::Target::FromActor(p_dest_), std::nullopt,
      std::optional<core::Color>{core::Color::FromArgb(0, 128, 0)}));
  return true;
}

FallToEarth::FallToEarth(sim::Actor& self,
                         const FallsToEarthInfoData& info)
    : aircraft_{self.Trait<Aircraft>()}, info_{info} {
  b_is_interruptible_ = false;
  if (!info_.opt_maximum_spin_speed.has_value() ||
      info_.opt_maximum_spin_speed->Angle != 0)
    int4_acceleration_ = self.world().SharedRandom().Next(2) * 2 - 1;
}

bool FallToEarth::Tick(sim::Actor& self) {
  if (self.world().Map().DistanceAboveTerrain(self.CenterPosition())
          .Length <= 0) {
    if (info_.ptr_explosion_weapon != nullptr)
      sim::WeaponImpact(*info_.ptr_explosion_weapon,
                      sim::Target::FromPos(self.CenterPosition()), &self);

    self.Kill(&self);
    Cancel(self);
    return true;
  }

  if (int4_acceleration_ != 0) {
    if (!info_.opt_maximum_spin_speed.has_value() ||
        std::abs(int4_spin_) < info_.opt_maximum_spin_speed->Angle)
      int4_spin_ += 4 * int4_acceleration_;

    aircraft_->SetFacing(
        WAngle{aircraft_->Facing().Angle + int4_spin_});
  }

  auto move = info_.b_moves ? aircraft_->FlyStep(aircraft_->Facing())
                            : WVec{0, 0, 0};
  move = move - WVec{WDist{0}, WDist{0}, info_.dist_velocity};
  aircraft_->SetPosition(&self, aircraft_->CenterPosition() + move);

  return false;
}

Resupply::Resupply(sim::Actor& self, Actor& host, WDist close_enough,
                   bool stay_on_resupplier)
    : host_{sim::Target::FromActor(&host)},
      dist_close_enough_{close_enough},
      b_stay_on_resupplier_{stay_on_resupplier},
      move_cooldown_helper_{self.world(), nullptr} {
  for (auto* r : host.TraitsImplementing<RepairsUnits>())
    vec_repairs_units_.push_back(r);
  health_ = self.TraitOrDefault<sim::IHealth>();
  repairable_ = self.TraitOrDefault<Repairable>();
  rearmable_ = self.TraitOrDefault<Rearmable>();
  for (auto* n : host.TraitsImplementing<sim::INotifyResupply>())
    vec_notify_resupplies_.push_back(n);
  for (auto* n : host.TraitsImplementing<sim::INotifyDockHost>())
    vec_notify_dock_hosts_.push_back(n);
  for (auto* n : self.TraitsImplementing<sim::INotifyDockClient>())
    vec_notify_dock_clients_.push_back(n);
  move_ = self.Trait<sim::IMove>();
  aircraft_ = dynamic_cast<Aircraft*>(move_);
  player_resources_ =
      self.Owner()->PlayerActor()->Trait<PlayerResources>();
  move_cooldown_helper_.SetMobile(dynamic_cast<Mobile*>(move_));
  move_cooldown_helper_.SetRetryIfDestinationBlocked(true);

  const auto* valued_rec = sim::FindTraitInfoOfInterface(
      *self.Info(), "OpenRA.Mods.Common.Traits.ValuedInfo");
  int4_unit_cost_ =
      valued_rec != nullptr
          ? static_cast<int>(sim::RecordFieldInt(*valued_rec, "Cost").value_or(0))
          : 0;

  const auto cannot_repair_at_host =
      health_ == nullptr ||
      health_->DamageState() == sim::DamageState::Undamaged ||
      vec_repairs_units_.empty() ||
      ((repairable_ == nullptr ||
        std::find(repairable_->Info().vec_repair_actors.begin(),
                  repairable_->Info().vec_repair_actors.end(),
                  host.InfoName()) ==
            repairable_->Info().vec_repair_actors.end()));

  if (!cannot_repair_at_host) {
    kind_active_resupply_types_ =
        kind_active_resupply_types_ | sim::ResupplyType::Repair;
    b_was_repaired_ = true;
  }

  auto cannot_rearm_at_host =
      rearmable_ == nullptr ||
      std::find(rearmable_->Info().vec_rearm_actors.begin(),
                rearmable_->Info().vec_rearm_actors.end(),
                host.InfoName()) ==
          rearmable_->Info().vec_rearm_actors.end();
  if (!cannot_rearm_at_host) {
    for (const auto* pool : rearmable_->RearmableAmmoPools())
      if (!pool->HasFullAmmo())
        cannot_rearm_at_host = false;
  }
  if (!cannot_rearm_at_host)
    kind_active_resupply_types_ =
        kind_active_resupply_types_ | sim::ResupplyType::Rearm;
}

bool Resupply::Tick(sim::Actor& self) {
  if (IsCanceling() && int4_remaining_ticks_ > 0) {
    int4_remaining_ticks_--;
    return false;
  }

  const auto is_host_invalid =
      host_.Type() != sim::TargetType::Actor || !host_.ActorPtr->IsInWorld();
  auto is_close_enough = false;
  if (!is_host_invalid) {
    if (dist_close_enough_ < WDist{0})
      is_close_enough = true;
    else
      is_close_enough =
          (host_.CenterPosition() - self.CenterPosition())
              .HorizontalLengthSquared() <= dist_close_enough_.LengthSquared();
  }

  if (!IsCanceling() && is_host_invalid)
    Cancel(self, true);

  if (IsCanceling() || is_host_invalid) {
    if (!is_host_invalid)
      for (auto* notify_resupply : vec_notify_resupplies_)
        notify_resupply->ResupplyTick(*const_cast<Actor*>(host_.ActorPtr), self,
                                      sim::ResupplyType::None);

    if (is_close_enough || is_host_invalid)
      OnResupplyEnding(self, is_host_invalid);

    return true;
  }

  const auto result = move_cooldown_helper_.Tick(false);
  if (result.has_value())
    return *result;

  if (kind_active_resupply_types_ != sim::ResupplyType::None &&
      aircraft_ == nullptr && !is_close_enough) {
    move_cooldown_helper_.NotifyMoveQueued();
    QueueChild(move_->MoveOntoTarget(&self, host_, WVec{0, 0, 0},
                                     std::nullopt, std::nullopt));

    return false;
  }

  if (!b_actual_resupply_started_ &&
      static_cast<std::int32_t>(kind_active_resupply_types_) > 0) {
    b_actual_resupply_started_ = true;

    for (auto* notify_resupply : vec_notify_resupplies_)
      notify_resupply->BeforeResupply(*const_cast<Actor*>(host_.ActorPtr), self,
                                      kind_active_resupply_types_);

    for (auto* nd : vec_notify_dock_clients_)
      nd->Docked(self, *const_cast<Actor*>(host_.ActorPtr));

    for (auto* nd : vec_notify_dock_hosts_)
      nd->Docked(*const_cast<Actor*>(host_.ActorPtr), self);
  }

  if ((static_cast<std::int32_t>(kind_active_resupply_types_) &
       static_cast<std::int32_t>(sim::ResupplyType::Repair)) != 0)
    RepairTick(self);

  if ((static_cast<std::int32_t>(kind_active_resupply_types_) &
       static_cast<std::int32_t>(sim::ResupplyType::Rearm)) != 0 &&
      rearmable_->RearmTick(self))
    kind_active_resupply_types_ =
        kind_active_resupply_types_ & ~sim::ResupplyType::Rearm;

  for (auto* notify_resupply : vec_notify_resupplies_)
    notify_resupply->ResupplyTick(*const_cast<Actor*>(host_.ActorPtr), self,
                                  kind_active_resupply_types_);

  if (kind_active_resupply_types_ == sim::ResupplyType::None) {
    OnResupplyEnding(self);
    return true;
  }

  return false;
}

void Resupply::Cancel(sim::Actor& self, bool keep_queue) {
  if (ChildActivity() != nullptr)
    for (auto* a = ChildActivity(); a != nullptr; a = a->NextActivity())
      if (auto* m = dynamic_cast<activities::Move*>(a))
        m->Cancel(self, false, true);

  sim::Activity::Cancel(self, keep_queue);
}

void Resupply::OnResupplyEnding(sim::Actor& self, bool b_host_invalid) {
  auto* rp = !b_host_invalid
                 ? const_cast<Actor*>(host_.ActorPtr)->TraitOrDefault<RallyPoint>()
                 : nullptr;
  if (aircraft_ != nullptr) {
    if (b_was_repaired_ || b_host_invalid ||
        (!b_stay_on_resupplier_ && aircraft_->Info().b_take_off_on_resupply)) {
      if (self.CurrentActivity()->NextActivity() == nullptr &&
          rp != nullptr && !rp->Path.empty()) {
        move_cooldown_helper_.NotifyMoveQueued();
        for (const auto& cell : rp->Path)
          QueueChild(self.world().Arena().Create<AttackMoveActivity>(
              self,
              [this, cell]() {
                return move_->MoveTo(
                    cell, 1,
                    repairable_near_ != nullptr
                        ? nullptr
                        : const_cast<Actor*>(host_.ActorPtr),
                    false,
                    std::optional<core::Color>{
                        aircraft_->Info().color_target_line});
              }));
      } else {
        QueueChild(self.world().Arena().Create<TakeOff>(self));
      }

      aircraft_->UnReserve();
    } else
      aircraft_->AllowYieldingReservation();
  } else if (!b_stay_on_resupplier_ && !b_host_invalid) {
    move_cooldown_helper_.NotifyMoveQueued();
    if (self.CurrentActivity()->NextActivity() == nullptr) {
      if (rp != nullptr && !rp->Path.empty())
        for (const auto& cell : rp->Path)
          QueueChild(self.world().Arena().Create<AttackMoveActivity>(
              self,
              [this, cell]() {
                return move_->MoveTo(
                    cell, 1,
                    repairable_near_ != nullptr
                        ? nullptr
                        : const_cast<Actor*>(host_.ActorPtr),
                    false, std::nullopt);
              }));
      else if (repairable_near_ == nullptr)
        QueueChild(move_->MoveToTarget(&self, host_));
    } else if (repairable_near_ == nullptr &&
               dynamic_cast<activities::Move*>(self.CurrentActivity()->NextActivity()) ==
                   nullptr)
      QueueChild(move_->MoveToTarget(&self, host_));
  }

  for (auto* nd : vec_notify_dock_clients_)
    nd->Undocked(self, *const_cast<Actor*>(host_.ActorPtr));

  if (!b_host_invalid)
    for (auto* nd : vec_notify_dock_hosts_)
      nd->Undocked(*const_cast<Actor*>(host_.ActorPtr), self);
}

void Resupply::RepairTick(sim::Actor& self) {
  RepairsUnits* repairs_units = nullptr;
  for (auto* r : vec_repairs_units_)
    if (!r->IsTraitDisabled() && !r->IsTraitPaused())
      repairs_units = r;
  if (repairs_units == nullptr) {
    auto any_paused = false;
    for (auto* r : vec_repairs_units_)
      if (r->IsTraitPaused())
        any_paused = true;

    if (!any_paused)
      kind_active_resupply_types_ =
          kind_active_resupply_types_ & ~sim::ResupplyType::Repair;

    return;
  }

  if (health_->DamageState() == sim::DamageState::Undamaged) {
    kind_active_resupply_types_ =
        kind_active_resupply_types_ & ~sim::ResupplyType::Repair;
    return;
  }

  if (int4_remaining_ticks_ == 0) {
    const auto hp_to_repair =
        repairable_ != nullptr && repairable_->Info().int4_hp_per_step > 0
            ? repairable_->Info().int4_hp_per_step
            : repairs_units->Info().int4_hp_per_step;

    const auto value =
        static_cast<std::int64_t>(int4_unit_cost_) *
        repairs_units->Info().int4_value_percentage;
    const auto cost =
        value == 0
            ? 0
            : std::max<std::int64_t>(
                  1, hp_to_repair * value / (health_->MaxHP() * 100LL));

    if (!player_resources_->TakeCash(static_cast<int>(cost), true)) {
      int4_remaining_ticks_ = 1;
      return;
    }

    self.Trait<Health>()->InflictDamage(
        self, const_cast<Actor*>(host_.ActorPtr),
        sim::Damage{-hp_to_repair,
                    repairs_units->Info().bitset_repair_damage_types},
        false);
    int4_remaining_ticks_ = repairs_units->Info().int4_interval;
  } else
    --int4_remaining_ticks_;
}

}  // namespace ora::mods::activities
