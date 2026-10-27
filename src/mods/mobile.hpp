#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "meta/variable_expression.hpp"
#include "mods/pathfinding/locomotor.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::pathfinding {
class PathFinder;
class IPathFinder;
}

namespace ora::mods {

namespace activities {
class Move;

}

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct MobileInfoData {
  std::string str_locomotor;
  WAngle angle_initial_facing{0};
  WAngle angle_turn_speed{512};
  int int4_speed = 1;
  bool b_always_turn_in_place = false;
  bool b_turns_while_moving = false;
  std::string str_cursor{"move"};
  std::vector<std::pair<std::string, std::string>> vec_terrain_cursors;
  std::string str_blocked_cursor{"move-blocked"};
  std::string str_voice{"Action"};
  core::Color color_target_line =
      core::Color::FromArgb(0, 128, 0);
  WAngle angle_preview_facing{384};
  int int4_editor_facing_display_order = 3;
  bool b_can_move_backward = false;
  int int4_backward_duration = 40;
  int int4_max_backward_cells = 15;
  std::optional<expr::BooleanExpression> expr_require_force_move;
  std::optional<expr::BooleanExpression> expr_immovable;
  WDist dist_terrain_orientation_adjustment_margin{-1};

  const pathfinding::LocomotorInfo* locomotor_info = nullptr;
  sim::ConditionalTraitData conditional;

  static MobileInfoData Parse(const meta::RecordObject& rec_info);

  bool CanEnterCell(sim::World& world, Actor* self, CPos cell,
                    sim::SubCell sub_cell, Actor* ignore_actor,
                    sim::BlockedByActor check) const;

  bool CanStayInCell(sim::World& world, CPos cell) const;
};

class Mobile final : public TraitBase,
                     public sim::ConditionalTraitCore<Mobile>,
                     public sim::IObservesVariables,
                     public sim::INotifyCreated,
                     public sim::IIssueOrder,
                     public sim::IResolveOrder,
                     public sim::IOrderVoice,
                     public sim::IPositionable,
                     public sim::IMove,
                     public sim::ITick,
                     public sim::ICreationActivity,
                     public sim::IFacing,
                     public sim::IDeathActorInitModifier,
                     public sim::INotifyAddedToWorld,
                     public sim::INotifyRemovedFromWorld,
                     public sim::INotifyBlockingMove,
                     public sim::IActorPreviewInitModifier,
                     public sim::INotifyBecomingIdle,
                     public sim::ISync {
 public:
  Mobile(ActorInitializer& init, const MobileInfoData& info);

  ORA_TRAIT_INTERFACES(
      Mobile, OpenRA_Mods_Common_Traits_Mobile, sim::IObservesVariables,
      sim::INotifyCreated, sim::IIssueOrder, sim::IResolveOrder,
      sim::IOrderVoice, sim::IPositionable, sim::IOccupySpace, sim::IMove,
      sim::ITick,
      sim::ICreationActivity, sim::IFacing, sim::IDeathActorInitModifier,
      sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld,
      sim::INotifyBlockingMove, sim::IActorPreviewInitModifier,
      sim::INotifyBecomingIdle, sim::ISync)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<Mobile>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<Mobile>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() override;

  void TraitEnabledHook(Actor& ) { UpdateOccupiedCellsForCondition(); }
  void TraitDisabledHook(Actor& ) { UpdateOccupiedCellsForCondition(); }
  void TraitResumedHook(Actor& ) { UpdateOccupiedCellsForCondition(); }
  void TraitPausedHook(Actor& ) { UpdateOccupiedCellsForCondition(); }

  sim::MovementType CurrentMovementTypes() const override {
    return movement_types_;
  }
  void SetCurrentMovementTypes(sim::MovementType value) override;

  WAngle Facing() const override { return orientation_.Yaw; }
  void SetFacing(const WAngle& facing) override {
    orientation_ = orientation_.WithYaw(facing);
  }
  WRot Orientation() const override {
    return orientation_.Rotate(rot_terrain_ramp_);
  }
  WAngle TurnSpeed() const override {
    return info_.angle_turn_speed;
  }

  CPos FromCell() const { return cell_from_; }
  CPos ToCell() const { return cell_to_; }
  WPos CenterPosition() const override {
    return pos_center_;
  }
  CPos TopLeft() const override { return cell_to_; }

  std::vector<std::pair<CPos, sim::SubCell>> OccupiedCells()
      const override;

  const MobileInfoData& Info() const { return info_; }
  pathfinding::Locomotor* Locomotor() const { return p_locomotor_; }
  pathfinding::IPathFinder* PathFinder() const { return p_path_finder_; }

  std::optional<CPos> GetAdjacentEnterableCell(
      CPos next_cell,
      const std::function<bool(CPos)>& prefer_to_avoid = nullptr);
  std::optional<CPos> GetAdjacentCell(
      CPos next_cell,
      const std::function<bool(CPos)>& prefer_to_avoid = nullptr);
  bool IsLeaving();
  bool CanInteractWithGroundLayer(Actor* self);

  sim::SubCell GetValidSubCell(sim::SubCell preferred) override;
  void SetPosition(Actor* self, CPos cell,
                   sim::SubCell sub_cell) override;
  void SetPosition(Actor* self, WPos pos) override;
  void SetCenterPosition(Actor* self, WPos pos) override;
  void SetTerrainRampOrientation(WRot orientation);
  bool IsLeavingCell(CPos location,
                     sim::SubCell sub_cell) override;
  sim::SubCell GetAvailableSubCell(
      CPos a, sim::SubCell preferred_sub_cell, Actor* ignore_actor,
      sim::BlockedByActor check) override;
  bool CanExistInCell(CPos cell) override;
  bool CanEnterCell(CPos cell, Actor* ignore_actor,
                    sim::BlockedByActor check) override;
  bool CanStayInCell(CPos cell);

  void SetLocation(CPos from, sim::SubCell from_sub, CPos to,
                   sim::SubCell to_sub);
  void FinishedMoving(Actor* self);
  void AddInfluence();
  void RemoveInfluence();

  sim::Activity* MoveTo(
      CPos cell, int near_enough, Actor* ignore_actor,
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
  sim::Activity* MoveFollow(
      Actor* self, const sim::Target& target, WDist min_range,
      WDist max_range, std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) override;
  sim::Activity* ReturnToCell(Actor* self) override;
  sim::Activity* MoveToTarget(
      Actor* self, const sim::Target& target,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) override;
  sim::Activity* MoveIntoTarget(
      Actor* self, const sim::Target& target) override;
  sim::Activity* MoveOntoTarget(
      Actor* self, const sim::Target& target, const WVec& offset,
      std::optional<WAngle> facing,
      std::optional<core::Color> target_line_color) override;
  sim::Activity* LocalMove(Actor* self, WPos from_pos,
                           WPos to_pos) override;
  int EstimatedMoveDuration(Actor* self, WPos from_pos,
                            WPos to_pos) override;
  CPos NearestMoveableCell(CPos target) override;
  bool CanEnterTargetNow(
      Actor* self, const sim::Target& target) override;
  int MovementSpeedForCell(CPos cell);
  CPos NearestMoveableCell(CPos target, int min_range,
                           int max_range);
  CPos NearestCell(CPos target, const std::function<bool(CPos)>& check,
                   int min_range, int max_range);
  void EnteringCell(Actor* self);
  sim::Activity* MoveTo(
      const std::function<std::pair<bool, std::vector<CPos>>(
          sim::BlockedByActor)>& path_func);

  void Tick(Actor& self) override { UpdateMovement(); }
  void UpdateMovement();
  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;
  void Created(Actor& self) override;
  void OnBecomingIdle(Actor& self) override;
  void OnNotifyBlockingMove(Actor& self, Actor& blocking) override;

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target,
      bool queued) override;
  void ResolveOrder(Actor& self,
                    const net::Order& order) override;
  std::string VoicePhraseForOrder(
      Actor& self, const net::Order& order) override;

  void ModifyActorPreviewInit(
      Actor& self, sim::TypeDictionary& inits) override;
  void ModifyDeathActorInit(
      Actor& self, sim::TypeDictionary& init) override;

  sim::Activity* GetCreationActivity() override;

  bool IsImmovable = false;
  bool TurnToMove = false;
  bool IsBlocking = false;
  bool IsMovingBetweenCells() const {
    return FromCell() != ToCell();
  }
  sim::MoveResult MoveResult = sim::MoveResult::InProgress;
  sim::SubCell FromSubCell = sim::SubCell::FullCell;
  sim::SubCell ToSubCell = sim::SubCell::FullCell;

  class ReturnToCellActivity;

  class LeaveProductionActivity;

  class MoveOrderTargeter;

 private:
  friend class sim::ConditionalTraitCore<Mobile>;
  friend class activities::Move;

  void UpdateOccupiedCellsForCondition();

  sim::Activity* WrapMove(sim::Activity* inner);
  sim::Activity* LocalMoveImpl(Actor* self, WPos from_pos, WPos to_pos,
                               CPos cell);
  std::optional<CPos> ClosestGroundCell();

  void CrushAction(
      Actor* self,
      void (sim::INotifyCrushed::*method)(Actor&, Actor&,
                                          const core::BitSet<sim::CrushClass>&));

  MobileInfoData info_;
  Actor* p_self_ = nullptr;
  std::vector<int> vec_speed_modifiers_;
  bool b_speed_modifiers_resolved_ = false;

  bool b_return_to_cell_on_creation = false;
  bool b_return_to_cell_on_creation_recalculate_sub_cell = true;
  int int4_creation_activity_delay = 0;
  std::vector<CPos> vec_creation_rallypoint{};

  sim::MovementType movement_types_ = sim::MovementType::None;
  WRot rot_terrain_ramp_ = WRot::None();
  WAngle angle_old_facing_;
  WRot orientation_;
  WPos pos_old_;
  CPos cell_from_;
  CPos cell_to_;
  WPos pos_center_;

  std::vector<sim::INotifyCustomLayerChanged*> vec_notify_custom_layer_changed_;
  std::vector<sim::INotifyCenterPositionChanged*>
      vec_notify_center_position_changed_;
  std::vector<sim::INotifyMoving*> vec_notify_moving_;
  std::vector<sim::INotifyFinishedMoving*> vec_notify_finished_moving_;
  std::vector<sim::IWrapMove*> vec_move_wrappers_;
  bool b_require_force_move = false;

  pathfinding::Locomotor* p_locomotor_ = nullptr;
  pathfinding::IPathFinder* p_path_finder_ = nullptr;
};

class Mobile::ReturnToCellActivity final : public sim::Activity {
 public:
  ReturnToCellActivity(sim::Actor& self, int delay = 0,
                       bool recalculate_sub_cell = false);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  Mobile* mobile_ = nullptr;
  bool b_recalculate_sub_cell_;
  CPos cell_;
  sim::SubCell sub_cell_;
  WPos pos_;
  int int4_delay_;
};

class Mobile::LeaveProductionActivity final : public sim::Activity {
 public:
  LeaveProductionActivity(sim::Actor& self, int delay,
                          std::vector<CPos> rally_point,
                          Mobile::ReturnToCellActivity* return_to_cell);

  void OnFirstRun(sim::Actor& self) override;

 private:
  Mobile* mobile_ = nullptr;
  int int4_delay_;
  std::vector<CPos> vec_rally_point_;
  Mobile::ReturnToCellActivity* p_return_to_cell_;
};

class Mobile::MoveOrderTargeter final : public sim::IOrderTargeter {
 public:
  MoveOrderTargeter(sim::Actor& self, Mobile* unit);

  std::string OrderID() const override { return "Move"; }
  int OrderPriority() const override { return 4; }
  bool IsQueued() const override { return b_is_queued_; }
  bool TargetOverridesSelection(
      sim::Actor& self, const sim::Target& target,
      std::span<Actor* const> actors_at, CPos xy,
      sim::TargetModifiers modifiers) override;
  bool CanTarget(sim::Actor& self, const sim::Target& target,
                 sim::TargetModifiers& modifiers,
                 std::string& cursor) override;

 private:
  Mobile* mobile_ = nullptr;
  const pathfinding::LocomotorInfo* locomotor_info_ = nullptr;
  bool b_reject_move_ = false;
  bool b_is_queued_ = false;
};

const pathfinding::LocomotorInfo* ResolveMobileLocomotorInfo(
    sim::World& world, const std::string& str_name);

}
