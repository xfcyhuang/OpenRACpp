#pragma once
import std;

#include "core/bitset.hpp"
#include "core/color.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "meta/generic_record.hpp"
#include "mods/util.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

class Aircraft;
class Rearmable;
class Repairable;
class Resupply;
class Reservable;

enum class IdleBehaviorType : std::int32_t {
  None,
  Land,
  ReturnToBase,
  LeaveMap,
  LeaveMapAtClosestEdge,
};

struct AircraftInfoData {
  sim::ConditionalTraitData conditional;
  IdleBehaviorType kind_idle_behavior = IdleBehaviorType::None;
  WDist dist_cruise_altitude{1280};
  bool b_repulsable = true;
  WDist dist_ideal_separation{1706};
  int int4_repulsion_speed = -1;
  WAngle angle_initial_facing{0};
  WAngle angle_turn_speed{512};
  std::optional<WAngle> opt_idle_turn_speed;
  WAngle angle_turn_deadzone{2};
  int int4_speed = 1;
  int int4_idle_speed = -1;
  WAngle angle_pitch{0};
  WAngle angle_pitch_speed{0};
  WAngle angle_roll{0};
  std::optional<WAngle> opt_idle_roll;
  WAngle angle_roll_speed{0};
  int int4_min_airborne_altitude = 1;
  std::vector<std::string> vec_landable_terrain_types;
  bool b_move_into_shroud = true;
  core::BitSet<sim::CrushClass> bitset_crushes;
  core::BitSet<sim::DamageType> bitset_crush_damage_types;
  std::string str_voice{"Action"};
  core::Color color_target_line = core::Color::FromArgb(0, 128, 0);
  std::string str_airborne_condition;
  std::string str_cruising_condition;
  bool b_can_hover = false;
  bool b_can_slide = false;
  bool b_vtol = false;
  bool b_turn_to_land = false;
  bool b_take_off_on_resupply = false;
  bool b_take_off_on_creation = true;
  bool b_can_force_land = true;
  WDist dist_land_altitude{0};
  WDist dist_land_range = WDist::FromCells(5);
  WAngle angle_maximum_pitch = WAngle::FromDegrees(10);
  WDist dist_altitude_velocity{43};
  std::vector<std::string> vec_takeoff_sounds;
  std::vector<std::string> vec_landing_sounds;
  WDist dist_wait_distance_from_resupply_base{3072};
  int int4_number_of_ticks_to_verify_available_airport = 150;
  WAngle angle_preview_facing{384};
  int int4_editor_facing_display_order = 3;
  std::optional<expr::BooleanExpression> expr_require_force_move;
  std::string str_cursor{"move"};
  std::string str_blocked_cursor{"move-blocked"};
  std::string str_enter_cursor{"enter"};
  std::string str_enter_blocked_cursor{"enter-blocked"};

  static AircraftInfoData Parse(const meta::RecordObject& rec_info);
};

class Aircraft final : public TraitBase,
                       public sim::ConditionalTraitCore<Aircraft>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::ITick,
                       public sim::ISync,
                       public sim::IFacing,
                       public sim::IPositionable,
                       public sim::IMove,
                       public sim::INotifyAddedToWorld,
                       public sim::INotifyRemovedFromWorld,
                       public sim::INotifyActorDisposing,
                       public sim::INotifyBecomingIdle,
                       public sim::ICreationActivity,
                       public sim::IActorPreviewInitModifier,
                       public sim::IDeathActorInitModifier,
                       public sim::IIssueDeployOrder,
                       public sim::IIssueOrder,
                       public sim::IResolveOrder,
                       public sim::IOrderVoice {
 public:
  Aircraft(ActorInitializer& init, AircraftInfoData info);

  ORA_TRAIT_INTERFACES(
      Aircraft, OpenRA_Mods_Common_Traits_Aircraft, sim::IObservesVariables,
      sim::INotifyCreated, sim::ITick, sim::ISync, sim::IFacing,
      sim::IPositionable, sim::IOccupySpace, sim::IMove,
      sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld,
      sim::INotifyActorDisposing, sim::INotifyBecomingIdle,
      sim::ICreationActivity, sim::IActorPreviewInitModifier,
      sim::IDeathActorInitModifier, sim::IIssueDeployOrder, sim::IIssueOrder,
      sim::IResolveOrder, sim::IOrderVoice)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Aircraft>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Aircraft>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    auto vec_observers = CollectObservers();
    if (info_.expr_require_force_move.has_value()) {
      sim::VariableObserver observer;
      observer.fn_notifier =
          [this](Actor& self, sim::ConditionCacheView vars) {
            RequireForceMoveConditionChanged(self, vars);
          };
      observer.vec_variables.assign(
          info_.expr_require_force_move->Variables().begin(),
          info_.expr_require_force_move->Variables().end());
      vec_observers.push_back(std::move(observer));
    }
    return vec_observers;
  }

  WAngle Facing() const override { return rot_orientation_.Yaw; }
  void SetFacing(const WAngle& facing) override {
    rot_orientation_ = rot_orientation_.WithYaw(facing);
  }
  WAngle Pitch() const { return rot_orientation_.Pitch; }
  void SetPitch(const WAngle& pitch) {
    rot_orientation_ = rot_orientation_.WithPitch(pitch);
  }
  WAngle Roll() const { return rot_orientation_.Roll; }
  void SetRoll(const WAngle& roll) {
    rot_orientation_ = rot_orientation_.WithRoll(roll);
  }
  WRot Orientation() const override { return rot_orientation_; }

  WPos CenterPosition() const override { return wpos_center_; }
  CPos TopLeft() const override {
    return self_->world().Map().CellContaining(wpos_center_);
  }

  WAngle TurnSpeed() const override {
    return IsTraitDisabled() || IsTraitPaused() ? WAngle{0}
                                                : info_.angle_turn_speed;
  }
  std::optional<WAngle> IdleTurnSpeed() const {
    return IsTraitDisabled() || IsTraitPaused() ? std::nullopt
                                                : info_.opt_idle_turn_speed;
  }

  WAngle GetTurnSpeed(bool b_idle_turn);

  Actor* ReservedActor() const { return p_reserved_actor_; }
  bool MayYieldReservation() const { return b_may_yield_reservation_; }
  bool ForceLanding() const { return b_force_landing_; }

  static WPos GroundPosition(Actor& self);

  WDist LandAltitude();

  bool AtLandAltitude() {
    return self_->world().Map().DistanceAboveTerrain(wpos_center_) ==
           LandAltitude();
  }

  void Repulse();
  WVec GetRepulsionForce();
  WVec GetRepulsionForce(Actor& other);

  Actor* GetActorBelow();
  void MakeReservation(Actor& target);
  void AllowYieldingReservation();
  void UnReserve();

  int MovementSpeed() const;
  int IdleMovementSpeed() const;

  std::vector<std::pair<CPos, SubCell>> OccupiedCells() const override {
    return vec_landing_cells_;
  }

  WVec FlyStep(WAngle facing) const { return FlyStep(MovementSpeed(), facing); }
  WVec FlyStep(int speed, WAngle facing) const;

  std::optional<CPos> FindLandingLocation(CPos target_cell,
                                          WDist max_search_distance);
  bool CanLand(std::span<const CPos> cells, Actor* docking_actor = nullptr,
               bool b_blocked_by_mobile = true);
  bool CanLand(CPos cell, Actor* docking_actor = nullptr,
               bool b_blocked_by_mobile = true);
  bool CanRearmAt(Actor& host);
  bool CanRepairAt(Actor& host);

  void ModifyDeathActorInit(Actor& self,
                            sim::TypeDictionary& init) override;

  void OnBecomingIdle(Actor& self) override;

  bool CanExistInCell(CPos cell) override { return true; }
  bool IsLeavingCell(CPos location, SubCell sub_cell) override {
    return false;
  }
  bool CanEnterCell(CPos location, Actor* ignore_actor,
                    sim::BlockedByActor check) override {
    return true;
  }
  SubCell GetValidSubCell(SubCell preferred) override {
    return SubCell::Invalid;
  }
  SubCell GetAvailableSubCell(CPos location, SubCell preferred_sub_cell,
                              Actor* ignore_actor,
                              sim::BlockedByActor check) override {
    return SubCell::Invalid;
  }
  void SetCenterPosition(Actor* self, WPos pos) override {
    SetPosition(self, pos);
  }
  void SetPosition(Actor* self, CPos cell, SubCell sub_cell) override;
  void SetPosition(Actor* self, WPos pos) override;

  void FinishedMoving(Actor& self);
  void EnteringCell(Actor& self);

  void AddInfluence(std::vector<std::pair<CPos, SubCell>> vec_landing_cells);
  void AddInfluence(CPos landing_cell);
  void RemoveInfluence();
  bool HasInfluence() const { return !vec_landing_cells_.empty(); }

  sim::Activity* MoveTo(CPos cell, int near_enough, Actor* ignore_actor,
                        bool evaluate_nearest_movable_cell,
                        std::optional<core::Color> target_line_color) override;
  sim::Activity* MoveWithinRange(
      const sim::Target& target, WDist range,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) override;
  sim::Activity* MoveWithinRange(
      const sim::Target& target, WDist min_range, WDist max_range,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) override;
  sim::Activity* MoveFollow(Actor* self, const sim::Target& target,
                            WDist min_range, WDist max_range,
                            std::optional<WPos> initial_target_position,
                            std::optional<core::Color> target_line_color)
      override;
  sim::Activity* ReturnToCell(Actor* self) override { return nullptr; }
  sim::Activity* MoveToTarget(Actor* self, const sim::Target& target,
                              std::optional<WPos> initial_target_position,
                              std::optional<core::Color> target_line_color)
      override;
  sim::Activity* MoveIntoTarget(Actor* self,
                                const sim::Target& target) override;
  sim::Activity* MoveOntoTarget(Actor* self, const sim::Target& target,
                                const WVec& offset,
                                std::optional<WAngle> facing,
                                std::optional<core::Color> target_line_color)
      override;
  sim::Activity* LocalMove(Actor* self, WPos from_pos, WPos to_pos) override;
  int EstimatedMoveDuration(Actor* self, WPos from_pos,
                            WPos to_pos) override;
  CPos NearestMoveableCell(CPos cell) override { return cell; }
  sim::MovementType CurrentMovementTypes() const override {
    return kind_movement_types_;
  }
  void SetCurrentMovementTypes(sim::MovementType type) override;
  bool CanEnterTargetNow(Actor* self, const sim::Target& target) override;

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;
  net::Order IssueDeployOrder(Actor& self, bool queued) override;
  bool CanIssueDeployOrder(Actor& self, bool queued) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

  void Tick(Actor& self) override;
  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;
  void Disposing(Actor& self) override;
  sim::Activity* GetCreationActivity() override;
  void ModifyActorPreviewInit(Actor& self,
                              sim::TypeDictionary& inits) override;

  const AircraftInfoData& Info() const { return info_; }
  Actor* Self() const { return self_; }

  class AircraftMoveOrderTargeter final : public sim::IOrderTargeter {
   public:
    explicit AircraftMoveOrderTargeter(Aircraft& aircraft)
        : aircraft_{aircraft} {}

    std::string OrderID() const override { return str_order_id_; }
    int OrderPriority() const override { return 4; }
    bool TargetOverridesSelection(
        Actor& self, const sim::Target& target,
        std::span<Actor* const> actors_at, CPos xy,
        sim::TargetModifiers modifiers) override;
    bool CanTarget(Actor& self, const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override;
    bool IsQueued() const override { return b_is_queued_; }

   private:
    Aircraft& aircraft_;
    std::string str_order_id_{"Move"};
    bool b_is_queued_ = false;
  };

  class AssociateWithAirfieldActivity final : public sim::Activity {
   public:
    AssociateWithAirfieldActivity(Aircraft& aircraft_self, int delay,
                                  std::vector<CPos> vec_rally_point,
                                  bool b_creation_by_map);

    void OnFirstRun(Actor& self) override;
    bool Tick(Actor& self) override;

   private:
    bool TryDock(Actor& self, WPos& wpos_pos);

    Aircraft& aircraft_;
    int int4_delay_;
    std::vector<CPos> vec_rally_point_;
    bool b_creation_by_map_;
  };

 private:
  void RequireForceMoveConditionChanged(Actor& self,
                                        sim::ConditionCacheView conditions);
  bool AircraftCanEnter(Actor& a, sim::TargetModifiers modifiers);
  bool AircraftCanEnter(Actor& a);
  bool AircraftCanResupplyAt(Actor& a, bool b_allowed_to_force_enter);
  bool IsBlockedBy(Actor& self, Actor& other_actor, Actor* ignore_actor,
                   bool b_blocked_by_mobile);
  void CrushAction(Actor& self, bool b_on_crush);
  void OnAirborneAltitudeReached();
  void OnAirborneAltitudeLeft();
  void OnCruisingAltitudeReached();
  void OnCruisingAltitudeLeft();

  Actor* self_ = nullptr;
  Repairable* repairable_ = nullptr;
  Rearmable* rearmable_ = nullptr;
  std::vector<sim::IAircraftCenterPositionOffset*> vec_position_offsets_;
  Reservable* ptr_reservation_ = nullptr;
  Actor* p_reserved_actor_ = nullptr;
  bool b_may_yield_reservation_ = false;
  bool b_force_landing_ = false;
  std::vector<int> vec_speed_modifier_cache_;
  std::vector<sim::INotifyMoving*> vec_notify_moving_;
  std::vector<sim::INotifyCenterPositionChanged*>
      vec_notify_center_position_changed_;
  sim::IOverrideAircraftLanding* override_aircraft_landing_ = nullptr;
  std::vector<std::pair<CPos, SubCell>> vec_landing_cells_;
  bool b_require_force_move_ = false;
  int int4_creation_activity_delay_ = 0;
  bool b_creation_by_map_ = false;
  std::vector<CPos> vec_creation_rally_point_;
  bool b_has_creation_rally_point_ = false;
  bool b_notify_ = true;
  bool b_airborne_ = false;
  bool b_cruising_ = false;
  int int4_airborne_token_ = Actor::InvalidConditionToken;
  int int4_cruising_token_ = Actor::InvalidConditionToken;
  sim::MovementType kind_movement_types_ = sim::MovementType::None;
  WPos wpos_cached_position_;
  WAngle angle_cached_facing_{0};
  WRot rot_orientation_ = WRot::None();
  WPos wpos_center_{};
  AircraftInfoData info_;
  std::vector<std::unique_ptr<AircraftMoveOrderTargeter>>
      vec_order_targeters_;
  std::vector<std::unique_ptr<sim::IOrderTargeter>> vec_enter_targeters_;
};

}
