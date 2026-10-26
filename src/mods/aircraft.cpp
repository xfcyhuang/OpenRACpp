// UPSTREAM: OpenRA.Mods.Common/Traits/Air/Aircraft.cs @1f594e320 的实现面
#include "mods/aircraft.hpp"

#include "game/actor_info.hpp"
#include "map/map.hpp"
#include "mods/actor_exts.hpp"
#include "mods/air_activities.hpp"
#include "mods/air_support.hpp"
#include "mods/armament.hpp"
#include "mods/building.hpp"
#include "mods/concrete_support_powers.hpp"
#include "mods/health.hpp"
#include "mods/selectable.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "mods/production.hpp"
#include "mods/production_support.hpp"
#include "mods/util.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/shroud.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec_info,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec_info, str_name);
}

std::optional<std::string> FieldString(const meta::RecordObject& rec_info,
                                       std::string_view str_name) {
  if (const auto v = sim::RecordFieldString(rec_info, str_name))
    return std::string{*v};
  return std::nullopt;
}

std::optional<bool> FieldBool(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* b = std::get_if<bool>(&gv->val))
    return *b;
  return std::nullopt;
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

std::optional<WVec> FieldWVec(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
    return WVec{static_cast<std::int32_t>(tuple->arr_ints[0]),
                static_cast<std::int32_t>(tuple->arr_ints[1]),
                static_cast<std::int32_t>(tuple->arr_ints[2])};
  return std::nullopt;
}

std::optional<core::Color> FieldColor(const meta::RecordObject& rec_info,
                                      std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* n = std::get_if<std::int64_t>(&gv->val))
    return core::Color::FromArgbRaw(static_cast<std::uint32_t>(*n));
  return std::nullopt;
}

std::optional<WAngle> FieldWAngle(const meta::RecordObject& rec_info,
                                  std::string_view str_name) {
  const auto v = FieldInt(rec_info, str_name);
  if (!v.has_value())
    return std::nullopt;
  return WAngle{static_cast<std::int32_t>(*v)};
}

std::optional<WDist> FieldWDist(const meta::RecordObject& rec_info,
                                std::string_view str_name) {
  const auto v = FieldInt(rec_info, str_name);
  if (!v.has_value())
    return std::nullopt;
  return WDist{static_cast<std::int32_t>(*v)};
}

int ClampInt(int value, int lo, int hi) {
  return value < lo ? lo : (value > hi ? hi : value);
}

}  // namespace

AircraftInfoData AircraftInfoData::Parse(const meta::RecordObject& rec_info) {
  AircraftInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldInt(rec_info, "IdleBehavior"))
    data.kind_idle_behavior = static_cast<IdleBehaviorType>(*v);
  if (const auto v = FieldWDist(rec_info, "CruiseAltitude"))
    data.dist_cruise_altitude = *v;
  if (const auto v = FieldBool(rec_info, "Repulsable"))
    data.b_repulsable = *v;
  if (const auto v = FieldWDist(rec_info, "IdealSeparation"))
    data.dist_ideal_separation = *v;
  if (const auto v = FieldInt(rec_info, "RepulsionSpeed"))
    data.int4_repulsion_speed = static_cast<int>(*v);
  if (const auto v = FieldWAngle(rec_info, "InitialFacing"))
    data.angle_initial_facing = *v;
  if (const auto v = FieldWAngle(rec_info, "TurnSpeed"))
    data.angle_turn_speed = *v;
  if (const auto v = FieldWAngle(rec_info, "IdleTurnSpeed"))
    data.opt_idle_turn_speed = v;
  if (const auto v = FieldWAngle(rec_info, "TurnDeadzone"))
    data.angle_turn_deadzone = *v;
  if (const auto v = FieldInt(rec_info, "Speed"))
    data.int4_speed = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "IdleSpeed"))
    data.int4_idle_speed = static_cast<int>(*v);
  if (const auto v = FieldWAngle(rec_info, "Pitch"))
    data.angle_pitch = *v;
  if (const auto v = FieldWAngle(rec_info, "PitchSpeed"))
    data.angle_pitch_speed = *v;
  if (const auto v = FieldWAngle(rec_info, "Roll"))
    data.angle_roll = *v;
  if (const auto v = FieldWAngle(rec_info, "IdleRoll"))
    data.opt_idle_roll = v;
  if (const auto v = FieldWAngle(rec_info, "RollSpeed"))
    data.angle_roll_speed = *v;
  if (const auto v = FieldInt(rec_info, "MinAirborneAltitude"))
    data.int4_min_airborne_altitude = static_cast<int>(*v);
  data.vec_landable_terrain_types = FieldStringArray(rec_info, "LandableTerrainTypes");
  if (const auto v = FieldBool(rec_info, "MoveIntoShroud"))
    data.b_move_into_shroud = *v;
  if (const auto v = FieldInt(rec_info, "Crushes"))
    data.bitset_crushes = core::BitSet<sim::CrushClass>::FromRawBits(
        static_cast<std::uint64_t>(*v));
  if (const auto v = FieldInt(rec_info, "CrushDamageTypes"))
    data.bitset_crush_damage_types =
        core::BitSet<sim::DamageType>::FromRawBits(
            static_cast<std::uint64_t>(*v));
  if (const auto v = FieldString(rec_info, "Voice"))
    data.str_voice = *v;
  if (const auto v = FieldColor(rec_info, "TargetLineColor"))
    data.color_target_line = *v;
  if (const auto v = FieldString(rec_info, "AirborneCondition"))
    data.str_airborne_condition = *v;
  if (const auto v = FieldString(rec_info, "CruisingCondition"))
    data.str_cruising_condition = *v;
  if (const auto v = FieldBool(rec_info, "CanHover"))
    data.b_can_hover = *v;
  if (const auto v = FieldBool(rec_info, "CanSlide"))
    data.b_can_slide = *v;
  if (const auto v = FieldBool(rec_info, "VTOL"))
    data.b_vtol = *v;
  if (const auto v = FieldBool(rec_info, "TurnToLand"))
    data.b_turn_to_land = *v;
  if (const auto v = FieldBool(rec_info, "TakeOffOnResupply"))
    data.b_take_off_on_resupply = *v;
  if (const auto v = FieldBool(rec_info, "TakeOffOnCreation"))
    data.b_take_off_on_creation = *v;
  if (const auto v = FieldBool(rec_info, "CanForceLand"))
    data.b_can_force_land = *v;
  if (const auto v = FieldWDist(rec_info, "LandAltitude"))
    data.dist_land_altitude = *v;
  if (const auto v = FieldWDist(rec_info, "LandRange"))
    data.dist_land_range = *v;
  if (const auto v = FieldWAngle(rec_info, "MaximumPitch"))
    data.angle_maximum_pitch = *v;
  if (const auto v = FieldWDist(rec_info, "AltitudeVelocity"))
    data.dist_altitude_velocity = *v;
  data.vec_takeoff_sounds = FieldStringArray(rec_info, "TakeoffSounds");
  data.vec_landing_sounds = FieldStringArray(rec_info, "LandingSounds");
  if (const auto v = FieldWDist(rec_info, "WaitDistanceFromResupplyBase"))
    data.dist_wait_distance_from_resupply_base = *v;
  if (const auto v = FieldInt(rec_info, "NumberOfTicksToVerifyAvailableAirport"))
    data.int4_number_of_ticks_to_verify_available_airport = static_cast<int>(*v);
  if (const auto v = FieldWAngle(rec_info, "PreviewFacing"))
    data.angle_preview_facing = *v;
  if (const auto v = FieldInt(rec_info, "EditorFacingDisplayOrder"))
    data.int4_editor_facing_display_order = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "RequireForceMoveCondition");
      v.has_value() && !v->empty())
    data.expr_require_force_move = expr::BooleanExpression{std::string{*v}};
  if (const auto v = FieldString(rec_info, "Cursor"))
    data.str_cursor = *v;
  if (const auto v = FieldString(rec_info, "BlockedCursor"))
    data.str_blocked_cursor = *v;
  if (const auto v = FieldString(rec_info, "EnterCursor"))
    data.str_enter_cursor = *v;
  if (const auto v = FieldString(rec_info, "EnterBlockedCursor"))
    data.str_enter_blocked_cursor = *v;
  return data;
}

Aircraft::Aircraft(ActorInitializer& init, AircraftInfoData info)
    : sim::ConditionalTraitCore<Aircraft>(info.conditional),
      info_{std::move(info)} {
  self_ = &init.Self();

  const auto* location_init = init.GetOrDefault<sim::LocationInit>("");
  const auto* center_position_init = init.GetOrDefault<sim::CenterPositionInit>("");
  if (location_init != nullptr || center_position_init != nullptr) {
    const WPos pos =
        center_position_init != nullptr
            ? center_position_init->Value()
            : self_->world().Map().CenterOfCell(location_init->Value());
    b_creation_by_map_ = init.Contains<sim::SpawnedByMapInit>("");
    SetPosition(self_, pos);
  }

  SetFacing(
      init.GetValue<sim::FacingInit, WAngle>("", info_.angle_initial_facing));
  int4_creation_activity_delay_ =
      init.GetValue<sim::CreationActivityDelayInit, int>("", 0);
  if (const auto* rally = init.GetOrDefault<sim::RallyPointInit>("")) {
    vec_creation_rally_point_ = rally->Value();
    b_has_creation_rally_point_ = true;
  }
}

WDist Aircraft::LandAltitude() {
  auto alt = info_.dist_land_altitude;
  for (const auto* offset : vec_position_offsets_)
    alt = WDist{alt.Length - offset->PositionOffset().Z};
  return alt;
}

void Aircraft::Created(Actor& self) {
  repairable_ = self.TraitOrDefault<Repairable>();
  rearmable_ = self.TraitOrDefault<Rearmable>();
  vec_speed_modifier_cache_.clear();
  for (auto* sm : self.TraitsImplementing<sim::ISpeedModifier>())
    vec_speed_modifier_cache_.push_back(sm->GetSpeedModifier());
  wpos_cached_position_ = self.CenterPosition();
  vec_notify_moving_.clear();
  for (auto* nm : self.TraitsImplementing<sim::INotifyMoving>())
    vec_notify_moving_.push_back(nm);
  vec_position_offsets_.clear();
  for (auto* po : self.TraitsImplementing<sim::IAircraftCenterPositionOffset>())
    vec_position_offsets_.push_back(po);
  override_aircraft_landing_ = self.TraitOrDefault<sim::IOverrideAircraftLanding>();
  vec_notify_center_position_changed_.clear();
  for (auto* nc : self.TraitsImplementing<sim::INotifyCenterPositionChanged>())
    vec_notify_center_position_changed_.push_back(nc);
  sim::ConditionalTraitCore<Aircraft>::CoreCreated(self);
}

void Aircraft::AddedToWorld(Actor& self) {
  self.world().AddToMaps(&self, this);

  const auto altitude = self.world().Map().DistanceAboveTerrain(wpos_center_);
  if (altitude.Length >= info_.int4_min_airborne_altitude)
    OnAirborneAltitudeReached();
  if (altitude == info_.dist_cruise_altitude)
    OnCruisingAltitudeReached();
}

void Aircraft::RemovedFromWorld(Actor& self) {
  UnReserve();
  self.world().RemoveFromMaps(&self, this);

  OnCruisingAltitudeLeft();
  OnAirborneAltitudeLeft();
}

void Aircraft::Tick(Actor& self) {
  if (!b_force_landing_ && IsTraitPaused() && b_airborne_ &&
      CanLand(self.Location()) &&
      !(self.CurrentActivity() != nullptr &&
        (dynamic_cast<activities::Land*>(self.CurrentActivity()) != nullptr ||
         dynamic_cast<activities::Turn*>(self.CurrentActivity()) != nullptr))) {
    self.QueueActivity(
        false, self.world().Arena().Create<activities::Land>(
                   self, std::nullopt, std::nullopt));
    b_force_landing_ = true;
  }

  if (b_force_landing_ && !IsTraitPaused() && !b_cruising_ &&
      (self.CurrentActivity() == nullptr ||
       dynamic_cast<activities::TakeOff*>(self.CurrentActivity()) == nullptr)) {
    b_force_landing_ = false;

    if (info_.kind_idle_behavior != IdleBehaviorType::Land)
      self.QueueActivity(false, self.world().Arena().Create<activities::TakeOff>(self));
  }

  const auto old_cached_facing = angle_cached_facing_;
  angle_cached_facing_ = Facing();

  const auto old_cached_position = wpos_cached_position_;
  wpos_cached_position_ = self.CenterPosition();

  auto new_movement_types = sim::MovementType::None;
  if (old_cached_facing != Facing())
    new_movement_types = new_movement_types | sim::MovementType::Turn;

  if ((old_cached_position - wpos_cached_position_).HorizontalLengthSquared() != 0)
    new_movement_types = new_movement_types | sim::MovementType::Horizontal;

  if ((old_cached_position - wpos_cached_position_).VerticalLengthSquared() != 0)
    new_movement_types = new_movement_types | sim::MovementType::Vertical;

  SetCurrentMovementTypes(new_movement_types);

  const auto hor = static_cast<std::int32_t>(kind_movement_types_) &
                   static_cast<std::int32_t>(sim::MovementType::Horizontal);
  if (hor == 0) {
    if (info_.angle_roll.Angle != 0 && Roll().Angle != 0)
      SetRoll(TickFacing(Roll(), WAngle{0}, info_.angle_roll_speed));

    if (info_.angle_pitch.Angle != 0 && Pitch().Angle != 0)
      SetPitch(TickFacing(Pitch(), WAngle{0}, info_.angle_pitch_speed));
  }

  Repulse();
}

void Aircraft::Repulse() {
  const auto repulsion_force = GetRepulsionForce();
  if (repulsion_force == WVec{0, 0, 0})
    return;

  const auto speed =
      info_.int4_repulsion_speed != -1 ? info_.int4_repulsion_speed : MovementSpeed();

  b_notify_ = false;
  SetPosition(self_, wpos_center_ + FlyStep(speed, repulsion_force.Yaw()));
  b_notify_ = true;
}

WVec Aircraft::GetRepulsionForce() {
  if (!info_.b_repulsable)
    return WVec{0, 0, 0};

  if (ptr_reservation_ != nullptr) {
    const auto distance_from_reservation_actor =
        (p_reserved_actor_->CenterPosition() - self_->CenterPosition()).HorizontalLength();
    if (distance_from_reservation_actor <
        info_.dist_wait_distance_from_resupply_base.Length)
      return WVec{0, 0, 0};
  }

  if (!b_cruising_)
    return WVec{0, 0, 0};

  auto repulsion_force = WVec{0, 0, 0};
  for (auto* actor : self_->world().FindActorsInCircle(
           self_->CenterPosition(), info_.dist_ideal_separation)) {
    if (actor->IsDead())
      continue;

    const auto* ai = sim::FindTraitInfoOfInterface(
        *actor->Info(), "OpenRA.Mods.Common.Traits.AircraftInfo");
    if (ai == nullptr)
      continue;
    const auto repulsable = FieldBool(*ai, "Repulsable").value_or(true);
    const auto cruise_altitude =
        FieldWDist(*ai, "CruiseAltitude").value_or(WDist{1280});
    if (!repulsable || !(cruise_altitude == info_.dist_cruise_altitude))
      continue;

    repulsion_force = repulsion_force + GetRepulsionForce(*actor);
  }

  if (!self_->world().Map().Contains(self_->Location())) {
    const auto center = WPos::Lerp(self_->world().Map().ProjectedTopLeft(),
                                   self_->world().Map().ProjectedBottomRight(),
                                   1, 2);
    repulsion_force =
        repulsion_force +
        WVec{0, 1024, 0}.Rotate(WRot::FromYaw(WAngle{(self_->CenterPosition() - center).Yaw()}));
  }

  if (info_.b_can_slide)
    return repulsion_force;

  const auto current_dir = FlyStep(Facing());
  const auto length = current_dir.HorizontalLength() * repulsion_force.HorizontalLength();
  if (length == 0)
    return WVec{0, 0, 0};

  const auto dot = WVec::Dot(current_dir, repulsion_force) / length;

  return dot >= 0 ? repulsion_force : WVec{0, 0, 0};
}

WVec Aircraft::GetRepulsionForce(Actor& other) {
  if (self_ == &other || other.CenterPosition().Z < self_->CenterPosition().Z)
    return WVec{0, 0, 0};

  const auto d = self_->CenterPosition() - other.CenterPosition();
  const auto dist_sq = d.HorizontalLengthSquared();
  if (dist_sq > info_.dist_ideal_separation.LengthSquared())
    return WVec{0, 0, 0};

  if (dist_sq < 1) {
    const auto yaw = self_->world().SharedRandom().Next(0, 1023);
    const WRot rot{WAngle{0}, WAngle{0}, WAngle{yaw}};
    return WVec{1024, 0, 0}.Rotate(rot);
  }

  return d * 1024 * 8 / static_cast<int>(dist_sq);
}

Actor* Aircraft::GetActorBelow() {
  if (self_->world().Map().DistanceAboveTerrain(wpos_center_) != LandAltitude())
    return nullptr;

  for (auto* a : self_->world().ActorMapFace()->GetActorsAt(self_->Location()))
    if (a->Info()->HasTraitInfoOfInterface(
            "OpenRA.Mods.Common.Traits.ReservableInfo"))
      return a;
  return nullptr;
}

void Aircraft::MakeReservation(Actor& target) {
  UnReserve();
  auto* reservable = target.TraitOrDefault<Reservable>();
  if (reservable != nullptr) {
    reservable->Reserve(&target);
    ptr_reservation_ = reservable;
    p_reserved_actor_ = &target;
  }
}

void Aircraft::AllowYieldingReservation() {
  if (ptr_reservation_ == nullptr)
    return;

  b_may_yield_reservation_ = true;
}

void Aircraft::UnReserve() {
  if (ptr_reservation_ == nullptr)
    return;

  ptr_reservation_->UnReserve();
  ptr_reservation_ = nullptr;
  p_reserved_actor_ = nullptr;
  b_may_yield_reservation_ = false;
}

WAngle Aircraft::GetTurnSpeed(bool b_idle_turn) {
  if ((b_idle_turn && IdleMovementSpeed() == 0) || MovementSpeed() == 0)
    return WAngle{0};

  const auto turn_speed = b_idle_turn
                              ? IdleTurnSpeed().value_or(TurnSpeed())
                              : TurnSpeed();

  return WAngle{ClampInt(
      ApplyPercentageModifiers(turn_speed.Angle,
                                     vec_speed_modifier_cache_),
      1, 1024)};
}

bool Aircraft::AircraftCanEnter(Actor& a, sim::TargetModifiers modifiers) {
  if (b_require_force_move_ &&
      (static_cast<std::int32_t>(modifiers) &
       static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) == 0)
    return false;

  return AircraftCanEnter(a);
}

bool Aircraft::AircraftCanEnter(Actor& a) {
  if (AppearsHostileTo(*self_, a))
    return false;

  const auto can_rearm_at_actor =
      rearmable_ != nullptr &&
      std::find(rearmable_->Info().vec_rearm_actors.begin(),
                rearmable_->Info().vec_rearm_actors.end(),
                a.InfoName()) != rearmable_->Info().vec_rearm_actors.end();
  const auto can_repair_at_actor =
      repairable_ != nullptr &&
      std::find(repairable_->Info().vec_repair_actors.begin(),
                repairable_->Info().vec_repair_actors.end(),
                a.InfoName()) != repairable_->Info().vec_repair_actors.end();

  return can_rearm_at_actor || can_repair_at_actor;
}

bool Aircraft::AircraftCanResupplyAt(Actor& a, bool b_allowed_to_force_enter) {
  if (AppearsHostileTo(*self_, a))
    return false;

  const auto can_rearm_at_actor =
      rearmable_ != nullptr &&
      std::find(rearmable_->Info().vec_rearm_actors.begin(),
                rearmable_->Info().vec_rearm_actors.end(),
                a.InfoName()) != rearmable_->Info().vec_rearm_actors.end();
  const auto can_repair_at_actor =
      repairable_ != nullptr &&
      std::find(repairable_->Info().vec_repair_actors.begin(),
                repairable_->Info().vec_repair_actors.end(),
                a.InfoName()) != repairable_->Info().vec_repair_actors.end();

  auto any_pool_not_full = false;
  for (const auto* pool : rearmable_ != nullptr
                              ? rearmable_->RearmableAmmoPools()
                              : std::span<AmmoPool* const>{})
    if (!pool->HasFullAmmo())
      any_pool_not_full = true;

  const auto allowed_to_enter_rearmer =
      can_rearm_at_actor && (b_allowed_to_force_enter || any_pool_not_full);
  const auto allowed_to_enter_repairer =
      can_repair_at_actor &&
      (b_allowed_to_force_enter ||
       self_->Trait<Health>()->DamageState() != sim::DamageState::Undamaged);

  return allowed_to_enter_rearmer || allowed_to_enter_repairer;
}

int Aircraft::MovementSpeed() const {
  return !IsTraitDisabled() && !IsTraitPaused()
             ? ApplyPercentageModifiers(info_.int4_speed,
                                              vec_speed_modifier_cache_)
             : 0;
}

int Aircraft::IdleMovementSpeed() const {
  return info_.int4_idle_speed < 0
             ? MovementSpeed()
             : (!IsTraitDisabled() && !IsTraitPaused()
                    ? ApplyPercentageModifiers(
                          info_.int4_idle_speed, vec_speed_modifier_cache_)
                    : 0);
}

WVec Aircraft::FlyStep(int speed, WAngle facing) const {
  const auto dir = WVec{0, -1024, 0}.Rotate(WRot::FromYaw(facing));
  return speed * dir / 1024;
}

std::optional<CPos> Aircraft::FindLandingLocation(CPos target_cell,
                                                  WDist max_search_distance) {
  if (CanLand(target_cell, nullptr, false))
    return target_cell;

  const auto cell_range =
      (max_search_distance.Length + 1023) / 1024;
  const auto center_position = self_->world().Map().CenterOfCell(target_cell);
  for (const auto& c :
       self_->world().Map().FindTilesInCircle(target_cell, cell_range)) {
    if (!CanLand(c, nullptr, false))
      continue;

    const auto delta = self_->world().Map().CenterOfCell(c) - center_position;
    if (delta.LengthSquared() < max_search_distance.LengthSquared())
      return c;
  }

  return std::nullopt;
}

bool Aircraft::CanLand(std::span<const CPos> cells, Actor* docking_actor,
                       bool b_blocked_by_mobile) {
  for (const auto& c : cells)
    if (!CanLand(c, docking_actor, b_blocked_by_mobile))
      return false;

  return true;
}

bool Aircraft::CanLand(CPos cell, Actor* docking_actor,
                       bool b_blocked_by_mobile) {
  if (!self_->world().Map().Contains(cell))
    return false;

  for (auto* other_actor : self_->world().ActorMapFace()->GetActorsAt(cell))
    if (IsBlockedBy(*self_, *other_actor, docking_actor, b_blocked_by_mobile))
      return false;

  if (docking_actor != nullptr)
    return true;

  const auto& landable_terrain =
      override_aircraft_landing_ != nullptr
          ? override_aircraft_landing_->LandableTerrainTypes()
          : std::span<const std::string>{info_.vec_landable_terrain_types};
  const auto type = self_->world().Map().GetTerrainInfo(cell).Type;
  return std::find(landable_terrain.begin(), landable_terrain.end(), type) !=
         landable_terrain.end();
}

bool Aircraft::IsBlockedBy(Actor& self, Actor& other_actor, Actor* ignore_actor,
                           bool b_blocked_by_mobile) {
  if (&other_actor == &self || &other_actor == ignore_actor)
    return false;

  if (!b_blocked_by_mobile &&
      self.Owner()->RelationshipWith(other_actor.Owner()) ==
          sim::PlayerRelationship::Ally &&
      other_actor.TraitOrDefault<Mobile>() != nullptr &&
      other_actor.CurrentActivity() == nullptr)
    return false;

  if (self.world().RulesContainTemporaryBlocker()) {
    auto* temporary_blocker = other_actor.TraitOrDefault<sim::ITemporaryBlocker>();
    if (temporary_blocker != nullptr &&
        temporary_blocker->CanRemoveBlockage(other_actor, self))
      return false;
  }

  if (info_.bitset_crushes.IsEmpty())
    return true;

  for (auto* crushable : other_actor.Crushables())
    if (crushable->CrushableBy(other_actor, self, info_.bitset_crushes))
      return false;

  return true;
}

bool Aircraft::CanRearmAt(Actor& host) {
  auto any_pool_not_full = false;
  for (const auto* pool : rearmable_ != nullptr
                              ? rearmable_->RearmableAmmoPools()
                              : std::span<AmmoPool* const>{})
    if (!pool->HasFullAmmo())
      any_pool_not_full = true;
  return rearmable_ != nullptr &&
         std::find(rearmable_->Info().vec_rearm_actors.begin(),
                   rearmable_->Info().vec_rearm_actors.end(),
                   host.InfoName()) != rearmable_->Info().vec_rearm_actors.end() &&
         any_pool_not_full;
}

bool Aircraft::CanRepairAt(Actor& host) {
  return repairable_ != nullptr &&
         std::find(repairable_->Info().vec_repair_actors.begin(),
                   repairable_->Info().vec_repair_actors.end(),
                   host.InfoName()) != repairable_->Info().vec_repair_actors.end() &&
         self_->Trait<Health>()->DamageState() != sim::DamageState::Undamaged;
}

void Aircraft::ModifyDeathActorInit(Actor& self,
                                    sim::TypeDictionary& init) {
  init.Add(std::make_unique<sim::FacingInit>(Facing()));
}

WPos Aircraft::GroundPosition(Actor& self) {
  return self.CenterPosition() -
         WVec{WDist{0}, WDist{0},
              self.world().Map().DistanceAboveTerrain(self.CenterPosition())};
}

void Aircraft::OnBecomingIdle(Actor& self) {
  if (info_.kind_idle_behavior == IdleBehaviorType::LeaveMap) {
    self.QueueActivity(self.world().Arena().Create<activities::FlyOffMap>(self));
    self.QueueActivity(self.world().Arena().Create<RemoveSelf>());
  } else if (info_.kind_idle_behavior ==
             IdleBehaviorType::LeaveMapAtClosestEdge) {
    const auto edge_cell = self.world().Map().ChooseClosestEdgeCell(self.Location());
    self.QueueActivity(self.world().Arena().Create<activities::FlyOffMap>(
        self, sim::Target::FromCell(self.world(), edge_cell)));
    self.QueueActivity(self.world().Arena().Create<RemoveSelf>());
  } else if (info_.kind_idle_behavior == IdleBehaviorType::ReturnToBase &&
             GetActorBelow() == nullptr) {
    self.QueueActivity(self.world().Arena().Create<activities::ReturnToBase>(
        self, nullptr, !info_.b_take_off_on_resupply));
  } else {
    const auto dat = self.world().Map().DistanceAboveTerrain(wpos_center_);
    if (dat == LandAltitude()) {
      if (!CanLand(self.Location()) && p_reserved_actor_ == nullptr)
        self.QueueActivity(self.world().Arena().Create<activities::TakeOff>(self));

      return;
    }

    if (info_.kind_idle_behavior == IdleBehaviorType::Land &&
        !info_.vec_landable_terrain_types.empty())
      self.QueueActivity(self.world().Arena().Create<activities::Land>(
          self, std::nullopt, std::nullopt));
    else
      self.QueueActivity(self.world().Arena().Create<activities::FlyIdle>(self));
  }
}

void Aircraft::SetPosition(Actor* self, CPos cell, SubCell) {
  SetPosition(self,
              self->world().Map().CenterOfCell(cell) +
                  WVec{0, 0, wpos_center_.Z});
}

void Aircraft::SetPosition(Actor* self, WPos pos) {
  wpos_center_ = pos;

  if (!self->IsInWorld())
    return;

  const auto altitude = self->world().Map().DistanceAboveTerrain(wpos_center_);

  if (HasInfluence() && altitude.Length <= info_.int4_min_airborne_altitude) {
    const std::vector<std::pair<CPos, SubCell>> current_pos{
        {TopLeft(), SubCell::FullCell}};
    if (vec_landing_cells_ != current_pos) {
      self->world().ActorMapFace()->RemoveInfluence(self, this);
      vec_landing_cells_ = current_pos;
      self->world().ActorMapFace()->AddInfluence(self, this);
    }
  }

  self->world().UpdateMaps(self, this);

  const bool is_airborne = altitude.Length >= info_.int4_min_airborne_altitude;
  if (is_airborne && !b_airborne_)
    OnAirborneAltitudeReached();
  else if (!is_airborne && b_airborne_)
    OnAirborneAltitudeLeft();

  const bool is_cruising = altitude == info_.dist_cruise_altitude;
  if (is_cruising && !b_cruising_)
    OnCruisingAltitudeReached();
  else if (!is_cruising && b_cruising_)
    OnCruisingAltitudeLeft();

  if (b_notify_)
    for (auto* n : vec_notify_center_position_changed_)
      n->CenterPositionChanged(*self, 0, 0);

  FinishedMoving(*self);
}

void Aircraft::FinishedMoving(Actor& self) {
  if (!IsAtGroundLevel(self))
    return;

  CrushAction(self, true);
}

void Aircraft::EnteringCell(Actor& self) {
  CrushAction(self, false);
}

void Aircraft::CrushAction(Actor& self, bool b_on_crush) {
  std::vector<std::pair<Actor*, sim::ICrushable*>> vec_crushables;
  for (auto* a : self.world().ActorMapFace()->GetActorsAt(TopLeft())) {
    if (a == &self)
      continue;
    for (auto* t : a->Crushables())
      vec_crushables.emplace_back(a, t);
  }

  for (auto& [crushable_actor, crushable_trait] : vec_crushables)
    if (crushable_trait->CrushableBy(*crushable_actor, self,
                                     info_.bitset_crushes) &&
        IsAtGroundLevel(*crushable_actor))
      for (auto* notify_crushed :
           crushable_actor->TraitsImplementing<sim::INotifyCrushed>()) {
        if (b_on_crush)
          notify_crushed->OnCrush(*crushable_actor, self,
                                  info_.bitset_crushes);
        else
          notify_crushed->WarnCrush(*crushable_actor, self,
                                    info_.bitset_crushes);
      }
}

void Aircraft::AddInfluence(
    std::vector<std::pair<CPos, SubCell>> vec_landing_cells) {
  if (HasInfluence())
    throw std::runtime_error(
        "Cannot AddInfluence until previous influence is removed with "
        "RemoveInfluence");

  vec_landing_cells_ = std::move(vec_landing_cells);
  if (self_->IsInWorld())
    self_->world().ActorMapFace()->AddInfluence(self_, this);
}

void Aircraft::AddInfluence(CPos landing_cell) {
  AddInfluence(std::vector<std::pair<CPos, SubCell>>{
      {landing_cell, SubCell::FullCell}});
}

void Aircraft::RemoveInfluence() {
  if (self_->IsInWorld())
    self_->world().ActorMapFace()->RemoveInfluence(self_, this);

  vec_landing_cells_.clear();
}

sim::Activity* Aircraft::MoveTo(CPos cell, int near_enough, Actor*,
                                bool,
                                std::optional<core::Color> target_line_color) {
  return self_->world().Arena().Create<activities::Fly>(
      *self_, sim::Target::FromCell(self_->world(), cell),
      WDist::FromCells(near_enough), std::nullopt, target_line_color);
}

sim::Activity* Aircraft::MoveWithinRange(
    const sim::Target& target, WDist range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {
  return self_->world().Arena().Create<activities::Fly>(
      *self_, target, WDist{0}, range, initial_target_position,
      target_line_color);
}

sim::Activity* Aircraft::MoveWithinRange(
    const sim::Target& target, WDist min_range, WDist max_range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {
  return self_->world().Arena().Create<activities::Fly>(
      *self_, target, min_range, max_range, initial_target_position,
      target_line_color);
}

sim::Activity* Aircraft::MoveFollow(Actor* self, const sim::Target& target,
                                    WDist min_range, WDist max_range,
                                    std::optional<WPos> initial_target_position,
                                    std::optional<core::Color> target_line_color) {
  return self->world().Arena().Create<activities::FlyFollow>(
      *self, target, min_range, max_range, initial_target_position,
      target_line_color);
}

sim::Activity* Aircraft::MoveToTarget(Actor* self, const sim::Target& target,
                                      std::optional<WPos> initial_target_position,
                                      std::optional<core::Color> target_line_color) {
  return self->world().Arena().Create<activities::Fly>(
      *self, target, initial_target_position, target_line_color);
}

sim::Activity* Aircraft::MoveIntoTarget(Actor* self,
                                        const sim::Target& target) {
  return self->world().Arena().Create<activities::Land>(*self, target);
}

sim::Activity* Aircraft::MoveOntoTarget(Actor* self, const sim::Target& target,
                                        const WVec& offset,
                                        std::optional<WAngle> facing,
                                        std::optional<core::Color> target_line_color) {
  return self->world().Arena().Create<activities::Land>(
      *self, target, offset, facing, target_line_color);
}

sim::Activity* Aircraft::LocalMove(Actor* self, WPos from_pos, WPos to_pos) {
  auto* activities = self->world().Arena().Create<activities::CallFunc>(
      [this, self, from_pos]() {
        SetPosition(self, from_pos);
      });
  activities->Queue(self->world().Arena().Create<activities::Fly>(
      *self, sim::Target::FromPos(to_pos)));
  return activities;
}

int Aircraft::EstimatedMoveDuration(Actor* self, WPos from_pos, WPos to_pos) {
  const auto speed = MovementSpeed();
  return speed > 0 ? (to_pos - from_pos).Length() / speed : 0;
}

void Aircraft::SetCurrentMovementTypes(sim::MovementType type) {
  const auto old_value = kind_movement_types_;
  kind_movement_types_ = type;
  if (type != old_value)
    for (auto* n : vec_notify_moving_)
      n->MovementTypeChanged(*self_, type);
}

bool Aircraft::CanEnterTargetNow(Actor* self, const sim::Target& target) {
  for (const auto& p : target.Positions())
    for (auto* a :
         self->world().ActorMapFace()->GetActorsAt(self->world().Map().CellContaining(p)))
      if (a != self && a != target.ActorPtr)
        return false;

  MakeReservation(*const_cast<Actor*>(target.ActorPtr));
  return true;
}

std::vector<sim::IOrderTargeter*> Aircraft::Orders() {
  if (!vec_enter_targeters_.empty() || vec_order_targeters_.empty())
    ;
  if (vec_order_targeters_.empty())
    vec_order_targeters_.push_back(
        std::make_unique<AircraftMoveOrderTargeter>(*this));
  std::vector<sim::IOrderTargeter*> vec_orders;
  for (auto& t : vec_enter_targeters_)
    vec_orders.push_back(t.get());
  for (auto& t : vec_order_targeters_)
    vec_orders.push_back(t.get());
  return vec_orders;
}

std::unique_ptr<net::Order> Aircraft::IssueOrder(
    Actor& self, sim::IOrderTargeter* order,
    const sim::Target& target, bool queued) {
  if (!IsTraitDisabled() &&
      (order->OrderID() == "Enter" || order->OrderID() == "Move" ||
       order->OrderID() == "Land" || order->OrderID() == "ForceEnter"))
    return std::make_unique<net::Order>(order->OrderID(), &self, target,
                                        queued);

  return nullptr;
}

net::Order Aircraft::IssueDeployOrder(Actor& self, bool queued) {
  if (IsTraitDisabled() || rearmable_ == nullptr ||
      rearmable_->Info().vec_rearm_actors.empty())
    return net::Order{};

  return net::Order{"ReturnToBase", &self, queued};
}

bool Aircraft::CanIssueDeployOrder(Actor& self, bool queued) {
  return rearmable_ != nullptr && !rearmable_->Info().vec_rearm_actors.empty();
}

std::string Aircraft::VoicePhraseForOrder(Actor& self,
                                          const net::Order& order) {
  if (IsTraitDisabled())
    return {};

  const auto& order_string = order.str_order_string;
  if (order_string == "Land" || order_string == "Move") {
    if (!info_.b_move_into_shroud && order.target.Type() != sim::TargetType::Invalid) {
      const auto cell =
          self.world().Map().CellContaining(order.target.CenterPosition());
      if (!self.Owner()->GetShroud()->IsExplored(cell))
        return {};
    }

    return info_.str_voice;
  }
  if (order_string == "Enter" || order_string == "ForceEnter" ||
      order_string == "Stop" || order_string == "Scatter")
    return info_.str_voice;
  if (order_string == "ReturnToBase")
    return rearmable_ != nullptr && !rearmable_->Info().vec_rearm_actors.empty()
               ? info_.str_voice
               : std::string{};
  return {};
}

void Aircraft::ResolveOrder(Actor& self, const net::Order& order) {
  if (IsTraitDisabled())
    return;

  const auto& order_string = order.str_order_string;
  if (order_string == "Move") {
    if (!order.target.IsValidFor(&self))
      return;

    const auto cell = self.world().Map().Clamp(
        self.world().Map().CellContaining(order.target.CenterPosition()));
    if (!info_.b_move_into_shroud && !self.Owner()->GetShroud()->IsExplored(cell))
      return;

    if (!order.b_queued)
      UnReserve();

    const auto target = sim::Target::FromCell(self.world(), cell);

    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::Fly>(
            self, target, WDist::FromCells(8), std::nullopt,
            std::optional<core::Color>{info_.color_target_line}));
    self.ShowTargetLines();
  } else if (order_string == "Land") {
    if (!order.target.IsValidFor(&self))
      return;

    const auto cell = self.world().Map().Clamp(
        self.world().Map().CellContaining(order.target.CenterPosition()));
    if (!info_.b_move_into_shroud && !self.Owner()->GetShroud()->IsExplored(cell))
      return;

    if (!order.b_queued)
      UnReserve();

    const auto target = sim::Target::FromCell(self.world(), cell);

    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::Land>(
            self, target, std::nullopt,
            std::optional<core::Color>{info_.color_target_line}));
    self.ShowTargetLines();
  } else if (order_string == "Enter" || order_string == "ForceEnter" ||
             order_string == "Repair") {
    if (order.target.Type() != sim::TargetType::Actor)
      return;

    auto* target_actor = const_cast<Actor*>(order.target.ActorPtr);
    const auto is_force_enter = order_string == "ForceEnter";
    const auto can_resupply_at = AircraftCanResupplyAt(
        *target_actor, is_force_enter || !info_.b_take_off_on_resupply);

    if (!can_resupply_at || !Reservable::IsAvailableFor(*target_actor, &self))
      return;

    if (!order.b_queued)
      UnReserve();

    const auto force_land = is_force_enter || !info_.b_take_off_on_resupply;
    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::ReturnToBase>(self, target_actor,
                                                              force_land));
    self.ShowTargetLines();
  } else if (order_string == "Stop") {
    if ((self.CurrentActivity() != nullptr &&
         dynamic_cast<activities::Resupply*>(self.CurrentActivity()) != nullptr) ||
        (self.CurrentActivity() != nullptr &&
         dynamic_cast<activities::ReturnToBase*>(self.CurrentActivity()) !=
             nullptr &&
         GetActorBelow() != nullptr))
      return;

    self.CancelActivity();
    UnReserve();
  } else if (order_string == "ReturnToBase") {
    if (rearmable_ == nullptr || rearmable_->Info().vec_rearm_actors.empty() ||
        (self.CurrentActivity() != nullptr &&
         dynamic_cast<activities::ReturnToBase*>(self.CurrentActivity()) !=
             nullptr) ||
        GetActorBelow() != nullptr)
      return;

    if (!order.b_queued)
      UnReserve();

    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::ReturnToBase>(
            self, nullptr, !info_.b_take_off_on_resupply));
    self.ShowTargetLines();
  } else if (order_string == "Scatter") {
    self.QueueActivity(order.b_queued,
                       self.world().Arena().Create<activities::Nudge>(&self));
    self.ShowTargetLines();
  }
}

void Aircraft::OnAirborneAltitudeReached() {
  if (b_airborne_)
    return;

  b_airborne_ = true;
  if (int4_airborne_token_ == Actor::InvalidConditionToken)
    int4_airborne_token_ = self_->GrantCondition(info_.str_airborne_condition);
}

void Aircraft::OnAirborneAltitudeLeft() {
  if (!b_airborne_)
    return;

  b_airborne_ = false;
  if (int4_airborne_token_ != Actor::InvalidConditionToken)
    int4_airborne_token_ = self_->RevokeCondition(int4_airborne_token_);
}

void Aircraft::OnCruisingAltitudeReached() {
  if (b_cruising_)
    return;

  b_cruising_ = true;
  if (int4_cruising_token_ == Actor::InvalidConditionToken)
    int4_cruising_token_ = self_->GrantCondition(info_.str_cruising_condition);
}

void Aircraft::OnCruisingAltitudeLeft() {
  if (!b_cruising_)
    return;

  b_cruising_ = false;
  if (int4_cruising_token_ != Actor::InvalidConditionToken)
    int4_cruising_token_ = self_->RevokeCondition(int4_cruising_token_);
}

void Aircraft::Disposing(Actor& self) {
  UnReserve();
}

void Aircraft::ModifyActorPreviewInit(Actor& self,
                                      sim::TypeDictionary& inits) {
  if (!inits.Contains<sim::DynamicFacingInit>() &&
      !inits.Contains<sim::FacingInit>())
    inits.Add(std::make_unique<sim::DynamicFacingInit>([this]() { return Facing(); }));
}

sim::Activity* Aircraft::GetCreationActivity() {
  if (b_has_creation_rally_point_ || int4_creation_activity_delay_ > 0 ||
      b_creation_by_map_)
    return self_->world().Arena().Create<AssociateWithAirfieldActivity>(
        *this, int4_creation_activity_delay_, vec_creation_rally_point_,
        b_creation_by_map_);

  return nullptr;
}

Aircraft::AssociateWithAirfieldActivity::AssociateWithAirfieldActivity(
    Aircraft& aircraft_self, int delay, std::vector<CPos> vec_rally_point,
    bool b_creation_by_map)
    : aircraft_{aircraft_self},
      int4_delay_{delay},
      vec_rally_point_{std::move(vec_rally_point)},
      b_creation_by_map_{b_creation_by_map} {}

bool Aircraft::AssociateWithAirfieldActivity::TryDock(Actor& self,
                                                      WPos& wpos_pos) {
  auto* host = aircraft_.GetActorBelow();
  if (host == nullptr)
    return false;

  auto* exit = NearestExitOrDefault(*host, wpos_pos, "");
  wpos_pos = host->CenterPosition();
  wpos_pos = WPos{wpos_pos.X, wpos_pos.Y,
                  wpos_pos.Z - self.world().Map().DistanceAboveTerrain(wpos_pos).Length};
  if (exit != nullptr) {
    wpos_pos = wpos_pos + exit->Info().spawn_offset;
    if (exit->Info().opt_facing.has_value())
      aircraft_.SetFacing(exit->Info().opt_facing.value());
  }

  const auto cpos = self.Location();
  aircraft_.AddInfluence(cpos);
  aircraft_.SetPosition(&self, wpos_pos);
  aircraft_.MakeReservation(*host);

  aircraft_.AllowYieldingReservation();
  return true;
}

void Aircraft::AssociateWithAirfieldActivity::OnFirstRun(Actor& self) {
  auto cpos = self.Location();
  auto pos = self.CenterPosition();

  if (b_creation_by_map_) {
    if (TryDock(self, pos))
      return;

    pos = WPos{pos.X, pos.Y,
               pos.Z - self.world().Map().DistanceAboveTerrain(pos).Length};

    if (!aircraft_.info_.b_take_off_on_creation && aircraft_.CanLand(cpos)) {
      aircraft_.AddInfluence(cpos);
      aircraft_.SetPosition(&self, pos);
    } else {
      aircraft_.SetPosition(
          &self, WPos{pos.X, pos.Y,
                      pos.Z + aircraft_.info_.dist_cruise_altitude.Length});
    }
  } else {
    TryDock(self, pos);
    if (int4_delay_ > 0)
      QueueChild(self.world().Arena().Create<activities::Wait>(int4_delay_));
  }
}

bool Aircraft::AssociateWithAirfieldActivity::Tick(Actor& self) {
  if (!aircraft_.info_.b_take_off_on_creation || IsCanceling())
    return true;

  if (!vec_rally_point_.empty())
    for (const auto& cell : vec_rally_point_)
      QueueChild(self.world().Arena().Create<activities::AttackMoveActivity>(
          self,
          [this, cell]() {
            return aircraft_.MoveTo(cell, 1, nullptr, true,
                                    std::optional<core::Color>{
                                        core::Color::FromArgb(255, 69, 0)});
          }));
  if (!b_creation_by_map_)
    aircraft_.UnReserve();

  return true;
}

void Aircraft::RequireForceMoveConditionChanged(
    Actor&, sim::ConditionCacheView conditions) {
  b_require_force_move_ =
      info_.expr_require_force_move->Evaluate(sim::ConditionSymbols(conditions));
}

bool Aircraft::AircraftMoveOrderTargeter::TargetOverridesSelection(
    Actor& self, const sim::Target& target, std::span<Actor* const>,
    CPos, sim::TargetModifiers modifiers) {
  if (target.Type() == sim::TargetType::Actor &&
      (target.ActorPtr->Owner() != self.Owner() ||
       self.world().Selection()->Contains(const_cast<Actor*>(target.ActorPtr))))
    return true;

  return (static_cast<std::int32_t>(modifiers) &
          static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) != 0;
}

bool Aircraft::AircraftMoveOrderTargeter::CanTarget(
    Actor& self, const sim::Target& target, sim::TargetModifiers& modifiers,
    std::string& cursor) {
  if (target.Type() != sim::TargetType::Terrain ||
      (aircraft_.b_require_force_move_ &&
       (static_cast<std::int32_t>(modifiers) &
        static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) == 0))
    return false;

  const auto location = self.world().Map().CellContaining(target.CenterPosition());

  if ((static_cast<std::int32_t>(modifiers) &
       static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) != 0 &&
      aircraft_.info_.b_can_force_land) {
    auto building_at_location = false;
    for (auto* a : self.world().ActorMapFace()->GetActorsAt(location))
      if (a->TraitOrDefault<Building>() != nullptr &&
          a->TraitOrDefault<Selectable>() != nullptr)
        building_at_location = true;
    if (!building_at_location || aircraft_.CanLand(location, nullptr, false))
      str_order_id_ = "Land";
  }

  b_is_queued_ = (static_cast<std::int32_t>(modifiers) &
                  static_cast<std::int32_t>(sim::TargetModifiers::ForceQueue)) != 0;

  const auto explored = self.Owner()->GetShroud()->IsExplored(location);
  cursor = !aircraft_.IsTraitPaused() &&
                   (explored || aircraft_.info_.b_move_into_shroud) &&
                   self.world().Map().Contains(location)
               ? aircraft_.info_.str_cursor
               : aircraft_.info_.str_blocked_cursor;

  return true;
}

}  // namespace ora::mods
