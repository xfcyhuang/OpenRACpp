// UPSTREAM: OpenRA.Mods.Common/Traits/Mobile.cs @b6fc03f L25-1091 全文
//          (逐语义重写;MoveOrderTargeter/ReturnToCellActivity/
//          LeaveProductionActivity 内嵌同上游)
//          The whole of Mobile.cs L25-1091 (verbatim-semantics rewrite;
//          MoveOrderTargeter/ReturnToCellActivity/LeaveProductionActivity
//          nested as upstream).
//
// 机制对照 / Mechanism mapping:
//  - PausableConditionalTrait<MobileInfo> → sim::ConditionalTraitCore<Mobile>
//    组合(四钩子静态派发;见 sim/conditional_trait.hpp 头注)
//    PausableConditionalTrait<MobileInfo> → the
//    sim::ConditionalTraitCore<Mobile> composition (the four hooks
//    statically dispatched; see sim/conditional_trait.hpp's header).
//  - MobileInfo.RulesetLoaded 的 LocomotorInfo 解析(世界 actor 的 Info 表
//    扫描)→ 工厂解析面(时点差异同 Health 的 D 系;异常文本逐字);
//    Info.locomotor 运行时缓存随每实例解析物化(上游跨 world 重置的
//    "reset between worlds" 语义由此免掉 —— 每次工厂解析即新实例)
//    MobileInfo.RulesetLoaded's LocomotorInfo resolution (the world
//    actor's Info-table scan) → the factory-parse face (the timing
//    difference like Health's D series; exception texts verbatim); the
//    Info.locomotor runtime cache materializes with each instance's parse
//    (upstream's "reset between worlds" reset becomes unnecessary — every
//    factory parse is a fresh instance).
//  - Shroud.IsExplored(L948/L1077/L1081):Shroud 未移植 —— MoveIntoShroud
//    默认 true 时短路不触;false 时按已探索论(COVERAGE 登记,Shroud 批回接)
//    Shroud.IsExplored (L948/L1077/L1081): Shroud unported — short-
//    circuited untouched when MoveIntoShroud keeps its default true;
//    treated-as-explored when false (registered in COVERAGE; rewired with
//    the Shroud batch).
//  - Parachutable/RejectsOrders/Turreted/Hovers 未移植:TraitOrDefault 查询
//    = 空集的上游等价面(不可达分支;登记于 COVERAGE)
//    Parachutable/RejectsOrders/Turreted/Hovers unported: the
//    TraitOrDefault queries are the upstream-equivalent empty set (the
//    unreachable branches; registered in COVERAGE).
//  - ShowTargetLines(L952):Settings 面 Phase 6(上游 UI 命令,仿真不可观测)
//    ShowTargetLines (L952): the Settings face is Phase 6 (an upstream UI
//    command, unobservable in the sim).
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
}  // namespace ora::mods::pathfinding

namespace ora::mods {

namespace activities {
class Move;  // 移动活动族(move_activities.hpp;Mobile::MoveTo 构造)
             // The move-activity family (move_activities.hpp; constructed
             // by Mobile::MoveTo).
}  // namespace activities

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// MobileInfo 的解析面(L26-171)
/// The parsed face of MobileInfo (L26-171).
struct MobileInfoData {
  std::string str_locomotor;                 // L32([FieldLoader.Require])
  WAngle angle_initial_facing{0};            // L34
  WAngle angle_turn_speed{512};              // L37
  int int4_speed = 1;                        // L39
  bool b_always_turn_in_place = false;       // L42
  bool b_turns_while_moving = false;         // L45
  std::string str_cursor{"move"};            // L49
  std::vector<std::pair<std::string, std::string>> vec_terrain_cursors;  // L54
  std::string str_blocked_cursor{"move-blocked"};  // L58
  std::string str_voice{"Action"};           // L61
  core::Color color_target_line =
      core::Color::FromArgb(0, 128, 0);      // L64 System.Drawing Color.Green
  WAngle angle_preview_facing{384};          // L67
  int int4_editor_facing_display_order = 3;  // L70
  bool b_can_move_backward = false;          // L73
  int int4_backward_duration = 40;           // L77
  int int4_max_backward_cells = 15;          // L81
  std::optional<expr::BooleanExpression> expr_require_force_move;  // L85
  std::optional<expr::BooleanExpression> expr_immovable;          // L89
  WDist dist_terrain_orientation_adjustment_margin{-1};  // L93 WDist(-1)

  // RulesetLoaded 解析面(L106-121)—— 工厂时点
  // The RulesetLoaded resolution face (L106-121) — at factory time.
  const pathfinding::LocomotorInfo* locomotor_info = nullptr;
  sim::ConditionalTraitData conditional;

  static MobileInfoData Parse(const meta::RecordObject& rec_info);

  /// L131-140:CanEnterCell(Info 面;locomotor 运行时解析缓存于 world)
  /// L131-140: CanEnterCell (the Info face; the locomotor runtime
  /// resolution cached on the world).
  bool CanEnterCell(sim::World& world, Actor* self, CPos cell,
                    sim::SubCell sub_cell, Actor* ignore_actor,
                    sim::BlockedByActor check) const;

  /// L142-152:CanStayInCell(Tunnel 层恒否)
  /// L142-152: CanStayInCell (the Tunnel layer always no).
  bool CanStayInCell(sim::World& world, CPos cell) const;
};

/// Mobile(L173-1090)
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
  Mobile(ActorInitializer& init, const MobileInfoData& info);  // L269-306

  ORA_TRAIT_INTERFACES(
      Mobile, OpenRA_Mods_Common_Traits_Mobile, sim::IObservesVariables,
      sim::INotifyCreated, sim::IIssueOrder, sim::IResolveOrder,
      sim::IOrderVoice, sim::IPositionable, sim::IOccupySpace, sim::IMove,
      sim::ITick,
      sim::ICreationActivity, sim::IFacing, sim::IDeathActorInitModifier,
      sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld,
      sim::INotifyBlockingMove, sim::IActorPreviewInitModifier,
      sim::INotifyBecomingIdle, sim::ISync)

  // ———— 条件面(TraitBase 覆写 → 核转发)————
  // ———— The conditional faces (TraitBase overrides → core forwarding) ————

  /// Exts.IsTraitEnabled(ConditionalTrait = IDisabledTrait)
  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<Mobile>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<Mobile>::IsTraitDisabled();
  }

  /// ConditionalTrait.GetVariableObservers(L894-904):base(requires/pause)
  /// + RequireForceMove/Immovable 两观察者(yield 序保真)
  /// ConditionalTrait.GetVariableObservers (L894-904): the base
  /// (requires/pause) + the RequireForceMove/Immovable observers (the
  /// yield order kept).
  std::vector<sim::VariableObserver> GetVariableObservers() override;

  /// 条件核四钩子的公开落点(ConditionalTraitCore 静态派发;
  /// TraitEnabled/Disabled/Resumed/Paused → UpdateOccupiedCells,L355-373)
  /// The condition core's four public landing points
  /// (ConditionalTraitCore's static dispatch; TraitEnabled/Disabled/
  /// Resumed/Paused → UpdateOccupiedCells, L355-373).
  void TraitEnabledHook(Actor& /*self*/) { UpdateOccupiedCellsForCondition(); }
  void TraitDisabledHook(Actor& /*self*/) { UpdateOccupiedCellsForCondition(); }
  void TraitResumedHook(Actor& /*self*/) { UpdateOccupiedCellsForCondition(); }
  void TraitPausedHook(Actor& /*self*/) { UpdateOccupiedCellsForCondition(); }

  // ———— IMove CurrentMovementTypes(L184-202)————
  sim::MovementType CurrentMovementTypes() const override {
    return movement_types_;
  }
  void SetCurrentMovementTypes(sim::MovementType value) override;

  // ———— IFacing(L224-237)————
  WAngle Facing() const override { return orientation_.Yaw; }
  void SetFacing(const WAngle& facing) override {
    orientation_ = orientation_.WithYaw(facing);
  }
  WRot Orientation() const override {         // L233
    return orientation_.Rotate(rot_terrain_ramp_);
  }
  WAngle TurnSpeed() const override {         // L235
    return info_.angle_turn_speed;
  }

  // ———— 位置面(L239-266)————
  CPos FromCell() const { return cell_from_; }          // L240 [VerifySync]
  CPos ToCell() const { return cell_to_; }              // L242 [VerifySync]
  WPos CenterPosition() const override {                // L252 [VerifySync]
    return pos_center_;
  }
  CPos TopLeft() const override { return cell_to_; }    // L254

  /// L256-266:OccupiedCells(同格单条/SharesCell HACK/双条)
  /// L256-266: OccupiedCells (the same-cell single entry / the SharesCell
  /// HACK / the two entries).
  std::vector<std::pair<CPos, sim::SubCell>> OccupiedCells()
      const override;

  const MobileInfoData& Info() const { return info_; }
  pathfinding::Locomotor* Locomotor() const { return p_locomotor_; }
  pathfinding::IPathFinder* PathFinder() const { return p_path_finder_; }

  // ———— 局部杂项(L375-453)————
  std::optional<CPos> GetAdjacentEnterableCell(
      CPos next_cell,
      const std::function<bool(CPos)>& prefer_to_avoid = nullptr);  // L377-392
  std::optional<CPos> GetAdjacentCell(
      CPos next_cell,
      const std::function<bool(CPos)>& prefer_to_avoid = nullptr);  // L394-416
  bool IsLeaving();                                                  // L430-439
  bool CanInteractWithGroundLayer(Actor* self);                      // L441-451

  // ———— IPositionable(L455-553)————
  sim::SubCell GetValidSubCell(sim::SubCell preferred) override;     // L458-477
  void SetPosition(Actor* self, CPos cell,
                   sim::SubCell sub_cell) override;                  // L480-493
  void SetPosition(Actor* self, WPos pos) override;                  // L496-502
  void SetCenterPosition(Actor* self, WPos pos) override;            // L505-519
  void SetTerrainRampOrientation(WRot orientation);                  // L521-525
  bool IsLeavingCell(CPos location,
                     sim::SubCell sub_cell) override;                // L527-531
  sim::SubCell GetAvailableSubCell(
      CPos a, sim::SubCell preferred_sub_cell, Actor* ignore_actor,
      sim::BlockedByActor check) override;                           // L533-536
  bool CanExistInCell(CPos cell) override;                           // L538-541
  bool CanEnterCell(CPos cell, Actor* ignore_actor,
                    sim::BlockedByActor check) override;             // L543-546
  bool CanStayInCell(CPos cell);                                    // L548-551(非接口面 | not an interface face)

  // ———— 局部 IPositionable 相关(L555-615)————
  void SetLocation(CPos from, sim::SubCell from_sub, CPos to,
                   sim::SubCell to_sub);                              // L558-575
  void FinishedMoving(Actor* self);                                   // L577-589
  void AddInfluence();                                                // L603-607
  void RemoveInfluence();                                             // L609-613

  // ———— IMove(L617-756 + 758-843)————
  sim::Activity* MoveTo(
      CPos cell, int near_enough, Actor* ignore_actor,
      bool evaluate_nearest_movable_cell,
      std::optional<core::Color> target_line_color) override;        // L628-632
  sim::Activity* MoveWithinRange(
      const sim::Target& target, WDist range,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) override;        // L634-638
  sim::Activity* MoveWithinRange(
      const sim::Target& target, WDist min_range, WDist max_range,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) override;        // L640-644
  sim::Activity* MoveFollow(
      Actor* self, const sim::Target& target, WDist min_range,
      WDist max_range, std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) override;        // L646-650
  sim::Activity* ReturnToCell(Actor* self) override;                 // L652-655
  sim::Activity* MoveToTarget(
      Actor* self, const sim::Target& target,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color);                 // L707-714
  sim::Activity* MoveIntoTarget(
      Actor* self, const sim::Target& target) override;              // L716-724
  sim::Activity* MoveOntoTarget(
      Actor* self, const sim::Target& target, const WVec& offset,
      std::optional<WAngle> facing,
      std::optional<core::Color> target_line_color) override;        // L726-729
  sim::Activity* LocalMove(Actor* self, WPos from_pos,
                           WPos to_pos) override;                    // L731-734
  int EstimatedMoveDuration(Actor* self, WPos from_pos,
                            WPos to_pos) override;                   // L736-740
  CPos NearestMoveableCell(CPos target) override;                    // L742-746
  bool CanEnterTargetNow(
      Actor* self, const sim::Target& target) override;              // L748-754
  int MovementSpeedForCell(CPos cell);                               // L760-766
  CPos NearestMoveableCell(CPos target, int min_range,
                           int max_range);                           // L768-790
  CPos NearestCell(CPos target, const std::function<bool(CPos)>& check,
                   int min_range, int max_range);                    // L792-803
  void EnteringCell(Actor* self);                                    // L805-812
  sim::Activity* MoveTo(
      const std::function<std::pair<bool, std::vector<CPos>>(
          sim::BlockedByActor)>& path_func);                         // L814

  // ———— 通知/生命周期面 ————
  void Tick(Actor& self) override { UpdateMovement(); }              // L322-325
  void UpdateMovement();                                             // L327-343
  void AddedToWorld(Actor& self) override;                           // L345-348
  void RemovedFromWorld(Actor& self) override;                       // L350-353
  void Created(Actor& self) override;                                // L308-320
  void OnBecomingIdle(Actor& self) override;                         // L860-878
  void OnNotifyBlockingMove(Actor& self, Actor& blocking) override;  // L880-892

  // ———— order 面(L919-987)————
  std::vector<sim::IOrderTargeter*> Orders() override;
  net::Order* IssueOrder(Actor& self, sim::IOrderTargeter* order,
                         const sim::Target& target,
                         bool queued) override;                      // L929-935
  void ResolveOrder(Actor& self,
                    const net::Order& order) override;               // L937-963
  std::string VoicePhraseForOrder(
      Actor& self, const net::Order& order) override;                // L965-987

  // ———— init 修改面 ————
  void ModifyActorPreviewInit(
      Actor& self, sim::TypeDictionary& inits) override;             // L845-849
  void ModifyDeathActorInit(
      Actor& self, sim::TypeDictionary& init) override;              // L851-858

  // ———— ICreationActivity(L1035-1042)————
  sim::Activity* GetCreationActivity() override;

  // ———— 公开字段(上游同名 public)————
  bool IsImmovable = false;        // L217(属性;条件观察器写)
  bool TurnToMove = false;         // L218
  bool IsBlocking = false;         // L219(属性面 → 公开字段)
  bool IsMovingBetweenCells() const {   // L221
    return FromCell() != ToCell();
  }
  sim::MoveResult MoveResult = sim::MoveResult::InProgress;  // L222
  sim::SubCell FromSubCell = sim::SubCell::FullCell;         // L208
  sim::SubCell ToSubCell = sim::SubCell::FullCell;

  /// ReturnToCellActivity(L657-705;内嵌类同上游)
  /// ReturnToCellActivity (L657-705; nested as upstream).
  class ReturnToCellActivity;

  /// LeaveProductionActivity(L989-1033)
  class LeaveProductionActivity;

  /// MoveOrderTargeter(L1044-1089)
  class MoveOrderTargeter;

 private:
  friend class sim::ConditionalTraitCore<Mobile>;
  friend class activities::Move;

  void UpdateOccupiedCellsForCondition();  // L355-373 共体
                                           // (the shared body)
  sim::Activity* WrapMove(sim::Activity* inner);  // L619-626
  sim::Activity* LocalMoveImpl(Actor* self, WPos from_pos, WPos to_pos,
                               CPos cell);        // L816-825
  std::optional<CPos> ClosestGroundCell();        // L827-841
  /// CrushAction(L591-601):上游 Func<INotifyCrushed, Action<…>> 的方法组
  /// 实参 → 成员指针
  /// CrushAction (L591-601): upstream's Func<INotifyCrushed, Action<…>>
  /// method-group argument → a member pointer.
  void CrushAction(
      Actor* self,
      void (sim::INotifyCrushed::*method)(Actor&, Actor&,
                                          const core::BitSet<sim::CrushClass>&));

  MobileInfoData info_;                       // L173 基类的 Info 面
  Actor* p_self_ = nullptr;                   // L176 self
  std::vector<int> vec_speed_modifiers_;      // L177 Lazy(首次物化标记)
  bool b_speed_modifiers_resolved_ = false;

  bool b_return_to_cell_on_creation = false;         // L179
  bool b_return_to_cell_on_creation_recalculate_sub_cell = true;  // L180
  int int4_creation_activity_delay = 0;              // L181
  std::vector<CPos> vec_creation_rallypoint{};       // L182(空 = null)

  sim::MovementType movement_types_ = sim::MovementType::None;  // L185
  WRot rot_terrain_ramp_ = WRot::None();           // L204
  WAngle angle_old_facing_;                        // L205
  WRot orientation_;                               // L206
  WPos pos_old_;                                   // L207
  CPos cell_from_;                                 // L240
  CPos cell_to_;                                   // L242
  WPos pos_center_;                                // L252

  std::vector<sim::INotifyCustomLayerChanged*> vec_notify_custom_layer_changed_;
  std::vector<sim::INotifyCenterPositionChanged*>
      vec_notify_center_position_changed_;
  std::vector<sim::INotifyMoving*> vec_notify_moving_;
  std::vector<sim::INotifyFinishedMoving*> vec_notify_finished_moving_;
  std::vector<sim::IWrapMove*> vec_move_wrappers_;
  bool b_require_force_move = false;               // L215

  pathfinding::Locomotor* p_locomotor_ = nullptr;  // L245
  pathfinding::IPathFinder* p_path_finder_ = nullptr;  // L247
};

/// Mobile::ReturnToCellActivity(L657-705;生产出口的回格活动)
/// Mobile::ReturnToCellActivity (L657-705; the production-exit
/// return-to-cell activity).
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

/// Mobile::LeaveProductionActivity(L989-1033;出厂活动)
/// Mobile::LeaveProductionActivity (L989-1033; the leave-production
/// activity).
class Mobile::LeaveProductionActivity final : public sim::Activity {
 public:
  LeaveProductionActivity(sim::Actor& self, int delay,
                          std::vector<CPos> rally_point,
                          Mobile::ReturnToCellActivity* return_to_cell);

  void OnFirstRun(sim::Actor& self) override;

 private:
  Mobile* mobile_ = nullptr;
  int int4_delay_;
  std::vector<CPos> vec_rally_point_;  // 空 = null
  Mobile::ReturnToCellActivity* p_return_to_cell_;
};

/// Mobile::MoveOrderTargeter(L1044-1089)
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

/// RulesetLoaded(L106-121)的 LocomotorInfo 解析(mobile.cpp;工厂面调用)
/// The RulesetLoaded (L106-121) LocomotorInfo resolution (mobile.cpp;
/// called by the factory face).
const pathfinding::LocomotorInfo* ResolveMobileLocomotorInfo(
    sim::World& world, const std::string& str_name);

}  // namespace ora::mods
