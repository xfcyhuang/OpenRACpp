// UPSTREAM: OpenRA.Mods.Common/Activities/Wait.cs @b6fc03f
//          全族逐语义重写:Activities/Wait.cs L17-30 | Activities/Turn.cs
//          L17-41 | Move/Drag.cs L17-71 | Move/Nudge.cs L17-56 |
//          Move/AttackMoveActivity.cs L17-106 | Move/MoveCooldownHelper.cs
//          L15-106 | Move/MoveAdjacentTo.cs L17-157 |
//          Move/MoveWithinRange.cs L17-76 | Move/MoveOnto.cs L17-55 |
//          Move/MoveOntoAndTurn.cs L17-42 | Move/Follow.cs L17-86 |
//          Move/LocalMoveIntoTarget.cs L17-84 | Move/Move.cs L22-655
//          (锚文件 = Wait.cs;族内文件见各行内标注)
//          Verbatim-semantics rewrites of the whole Move activity family
//          (the anchor file = Wait.cs; the family members carry their own
//          inline tags).
//
// 机制对照 / Mechanism mapping:
//  - 活动对象分配:C# new(GC)→ WorldArena::Create<T>(每局世界区 + 析构登记,
//    D26/D27 所有权面);活动相互构造(QueueChild)统一经 NewActivity 助手
//    Activity allocation: C#'s new (GC) → WorldArena::Create<T> (the
//    per-world arena + destructor records, the D26/D27 ownership face);
//    activities constructing one another (QueueChild) uniformly go through
//    the NewActivity helper.
//  - GetTargets/TargetLineNodes:Activity 基类未引入渲染域面(见
//    sim/activity.hpp 头注)—— 本族同形省略(Phase 4/6 随 Target 渲染面)
//    GetTargets/TargetLineNodes: the Activity base pulls in no render
//    face (see sim/activity.hpp's header) — omitted here the same way
//    (with the Target render surface in Phase 4/6).
//  - Nudge 的 Aircraft 分支 / AttackMoveActivity 的 AutoTarget·AttackMove
//    trait 面:实现类未移植 → 查询空集的上游等价分支(不可达;COVERAGE 登记)
//    Nudge's Aircraft branch / AttackMoveActivity's AutoTarget·AttackMove
//    trait faces: the implementing classes are unported → the
//    upstream-equivalent empty-query branches (unreachable; registered in
//    COVERAGE).
//  - Move.getPath 的闭包(Func<BlockedByActor,(bool,List)>;三构造变体)
//    → std::function 成员(构造期绑定;求值体零分配 —— TakeWhile 物化进
//    成员缓冲)
//    Move.getPath's closure (Func<BlockedByActor,(bool,List)>; the three
//    constructor variants) → a std::function member (bound at
//    construction; the evaluation body materializes TakeWhile into a
//    member buffer, zero allocation).
#pragma once
import std;

#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "mods/mobile.hpp"
#include "mods/util.hpp"
#include "sim/activity.hpp"
#include "sim/target.hpp"

namespace ora::mods::activities {

/// WorldArena 内构造活动的助手(new 的 C++ 等价;上游 GC)
/// The helper constructing an activity inside the WorldArena (the C++
/// equivalent of new; upstream's GC).
template <class T, class... As>
T* NewActivity(sim::Actor& self, As&&... as) {
  return self.world().Arena().Create<T>(self, std::forward<As>(as)...);
}

/// Wait.cs L17-30
class Wait : public sim::Activity {
 public:
  Wait(int period, bool interruptible = true)
      : int4_remaining_ticks_(period) {
    b_is_interruptible_ = interruptible;
  }

  bool Tick(sim::Actor& self) override;

 private:
  int int4_remaining_ticks_;
};

/// Turn.cs L17-41
class Turn : public sim::Activity {
 public:
  Turn(sim::Actor& self, WAngle desired_facing);

  bool Tick(sim::Actor& self) override;

 private:
  Mobile* mobile_ = nullptr;
  sim::IFacing* facing_ = nullptr;
  WAngle angle_desired_facing_;
};

/// Drag.cs L17-71
class Drag : public sim::Activity {
 public:
  Drag(sim::Actor& self, WPos start, WPos end, int length,
       std::optional<WAngle> facing = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  sim::IPositionable* positionable_ = nullptr;
  TraitBase* disableable_ = nullptr;  // IDisabledTrait 面
  WPos pos_start_;
  WPos pos_end_;
  int int4_length_;
  int int4_ticks_ = 0;
  std::optional<WAngle> opt_desired_facing_;
};

/// Nudge.cs L17-56
class Nudge : public sim::Activity {
 public:
  explicit Nudge(sim::Actor* nudger)
      : p_nudger_(nudger) {}

  void OnFirstRun(sim::Actor& self) override;

 private:
  sim::Actor* p_nudger_;
};

/// AttackMoveActivity.cs L17-106(AutoTarget/AttackMove trait 面未移植:
/// autoTarget/attackMove 恒 null → QueueChild(getMove()) 的上游等价分支)
/// AttackMoveActivity.cs L17-106 (the AutoTarget/AttackMove trait faces
/// unported: autoTarget/attackMove stay null → upstream's
/// QueueChild(getMove()) equivalent branch).
class AttackMoveActivity : public sim::Activity {
 public:
  AttackMoveActivity(sim::Actor& self,
                     std::function<sim::Activity*()> fn_get_move,
                     bool assault_moving = false);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;

 private:
  std::function<sim::Activity*()> fn_get_move_;
  bool b_is_assault_move_ = false;
  bool b_running_move_activity_ = false;
  int int4_token_ = sim::Actor::InvalidConditionToken;
  sim::Target target_ = sim::Target::Invalid();
};

/// MoveCooldownHelper.cs L15-106
class MoveCooldownHelper {
 public:
  MoveCooldownHelper(sim::World& world, Mobile* mobile)
      : world_(world), p_mobile_(mobile) {}

  bool RetryIfDestinationBlocked() const { return b_retry_if_destination_blocked_; }
  void SetRetryIfDestinationBlocked(bool b) {
    b_retry_if_destination_blocked_ = b;
  }
  int MinTicksInclusive() const { return int4_cooldown_min_; }
  int MaxTicksExclusive() const { return int4_cooldown_max_; }
  void SetCooldown(int min_ticks_inclusive, int max_ticks_exclusive) {
    int4_cooldown_min_ = min_ticks_inclusive;
    int4_cooldown_max_ = max_ticks_exclusive;
  }

  void NotifyMoveQueued() { b_was_moving_ = true; }

  /// Tick:L110-141(true/false = 立即完成/等待;nullopt = 继续常规逻辑)
  /// Tick: L110-141 (true/false = complete at once / keep waiting;
  /// nullopt = continue with the usual logic).
  std::optional<bool> Tick(bool b_target_is_hidden_actor);

 private:
  sim::World& world_;
  Mobile* p_mobile_;
  bool b_retry_if_destination_blocked_ = false;
  int int4_cooldown_min_ = 20;
  int int4_cooldown_max_ = 31;
  bool b_was_moving_ = false;
  bool b_has_run_cooldown_ = false;
  int int4_cooldown_ticks_ = 0;
};

/// MoveAdjacentTo.cs L17-157
class MoveAdjacentTo : public sim::Activity {
 public:
  MoveAdjacentTo(sim::Actor& self, const sim::Target& target,
                 std::optional<WPos> initial_target_position = std::nullopt,
                 std::optional<core::Color> target_line_color = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 protected:
  /// CalculatePathToTarget 的求值结果(上游 (bool, List<CPos>) 元组)
  /// The evaluation result of CalculatePathToTarget (upstream's
  /// (bool, List<CPos>) tuple).
  struct PathEval {
    bool b_already_at_destination = false;
    std::vector<CPos> vec_path;
  };

  /// 目标取值(useLastVisibleTarget ? lastVisible : target)
  /// The target value (useLastVisibleTarget ? lastVisible : target).
  sim::Target CurrentTarget() const;

  virtual bool ShouldStop(sim::Actor& self);
  virtual bool ShouldRepath(sim::Actor& self, CPos target_location);
  virtual void SetVisibleTargetLocation(sim::Actor& self,
                                        const sim::Target& target);
  virtual PathEval CalculatePathToTarget(sim::Actor& self,
                                         sim::BlockedByActor check);

  Mobile* mobile_ = nullptr;
  std::optional<core::Color> opt_target_line_color_;
  sim::Target target_;
  sim::Target last_visible_target_;
  CPos last_visible_target_location_;
  bool b_use_last_visible_target_ = false;

  std::vector<CPos> vec_search_cells_;  // SearchCells(L128)
  int int4_search_cells_tick_ = -1;     // searchCellsTick(L126)
};

/// MoveWithinRange.cs L17-76
class MoveWithinRange : public MoveAdjacentTo {
 public:
  MoveWithinRange(sim::Actor& self, const sim::Target& target,
                  WDist min_range, WDist max_range,
                  std::optional<WPos> initial_target_position = std::nullopt,
                  std::optional<core::Color> target_line_color = std::nullopt);

 protected:
  bool ShouldStop(sim::Actor& self) override;
  bool ShouldRepath(sim::Actor& self, CPos target_location) override;
  PathEval CalculatePathToTarget(sim::Actor& self,
                                 sim::BlockedByActor check) override;

 private:
  bool AtCorrectRange(const WPos& origin);

  WDist dist_max_range_;
  WDist dist_min_range_;
  int int4_max_cells_;
  int int4_min_cells_;
};

/// MoveOnto.cs L17-55
class MoveOnto : public MoveAdjacentTo {
 public:
  MoveOnto(sim::Actor& self, const sim::Target& target,
           std::optional<WVec> offset = std::nullopt,
           std::optional<WPos> initial_target_position = std::nullopt,
           std::optional<core::Color> target_line_color = std::nullopt);

 protected:
  void SetVisibleTargetLocation(sim::Actor& self,
                                const sim::Target& target) override;
  bool ShouldStop(sim::Actor& self) override;
  PathEval CalculatePathToTarget(sim::Actor& self,
                                 sim::BlockedByActor check) override;

 private:
  WVec vec_offset_;
};

/// MoveOntoAndTurn.cs L17-42
class MoveOntoAndTurn : public MoveOnto {
 public:
  MoveOntoAndTurn(sim::Actor& self, const sim::Target& target,
                  const WVec& offset, std::optional<WAngle> desired_facing,
                  std::optional<core::Color> target_line_color = std::nullopt);

  bool Tick(sim::Actor& self) override;

 private:
  std::optional<WAngle> opt_desired_facing_;
};

/// Follow.cs L17-86
class Follow : public sim::Activity {
 public:
  Follow(sim::Actor& self, const sim::Target& target, WDist min_range,
         WDist max_range, std::optional<WPos> initial_target_position,
         std::optional<core::Color> target_line_color = std::nullopt);

  bool Tick(sim::Actor& self) override;

 private:
  WDist dist_min_range_;
  WDist dist_max_range_;
  sim::IMove* move_ = nullptr;
  std::optional<core::Color> opt_target_line_color_;
  MoveCooldownHelper move_cooldown_helper_;
  sim::Target target_;
  sim::Target last_visible_target_;
  bool b_use_last_visible_target_ = false;
};

/// LocalMoveIntoTarget.cs L17-84
class LocalMoveIntoTarget : public sim::Activity {
 public:
  LocalMoveIntoTarget(sim::Actor& self, const sim::Target& target,
                      WDist target_movement_threshold,
                      std::optional<core::Color> target_line_color = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  Mobile* mobile_ = nullptr;
  sim::Target target_;
  std::optional<core::Color> opt_target_line_color_;
  WDist dist_target_movement_threshold_;
  WPos pos_target_start_;
};

/// Move.cs L22-655(MovePart/MoveFirstHalf/MoveSecondHalf 内嵌同上游)
/// Move.cs L22-655 (MovePart/MoveFirstHalf/MoveSecondHalf nested as
/// upstream).
class Move : public sim::Activity {
 public:
  /// 脚本移动序(L57-74;无 lane bias)
  /// The scriptable move order (L57-74; no lane bias).
  Move(sim::Actor& self, CPos destination,
       std::optional<core::Color> target_line_color = std::nullopt);

  /// 常规移动序(L76-101)
  /// The regular move order (L76-101).
  Move(sim::Actor& self, CPos destination, WDist near_enough,
       Actor* ignore_actor, bool evaluate_nearest_movable_cell,
       std::optional<core::Color> target_line_color = std::nullopt);

  /// 路径函数序(L103-113)
  /// The path-function order (L103-113).
  Move(sim::Actor& self,
       const std::function<std::pair<bool, std::vector<CPos>>(
           sim::BlockedByActor)>& fn_get_path,
       std::optional<core::Color> target_line_color = std::nullopt);

  WAngle ActorFacingModifier() const { return angle_actor_facing_modifier_; }

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;
  void Cancel(sim::Actor& self, bool keep_queue = false) override;
  void Cancel(sim::Actor& self, bool keep_queue, bool force_clear_path);

  /// PopPath 的求值结果(上游 ((CPos,SubCell)?, bool) 元组)
  /// PopPath's evaluation result (upstream's ((CPos,SubCell)?, bool)
  /// tuple).
  struct NextCellResult {
    std::optional<std::pair<CPos, sim::SubCell>> opt_next;
    bool b_should_try_again = false;
  };

  NextCellResult PopPath(sim::Actor& self);

 private:
  friend class MovePart;
  friend class MoveFirstHalf;
  friend class MoveSecondHalf;

  /// EvalPath(L115-120):TakeWhile(≠ mobile.ToCell)物化
  /// EvalPath (L115-120): the TakeWhile(!= mobile.ToCell)
  /// materialization.
  std::pair<bool, std::vector<CPos>> EvalPath(sim::BlockedByActor check);

  std::optional<std::pair<CPos, sim::SubCell>> UnblockDestination(
      sim::Actor& self);
  static bool CellIsEvacuating(sim::Actor& self, CPos cell);

  /// MovePart(L409-545)
  class MovePart;
  class MoveFirstHalf;
  class MoveSecondHalf;

  WAngle angle_actor_facing_modifier_;  // L24
  Mobile* mobile_ = nullptr;
  WDist dist_near_enough_;
  std::function<std::pair<bool, std::vector<CPos>>(sim::BlockedByActor)>
      fn_get_path_;
  Actor* p_ignore_actor_ = nullptr;
  std::optional<core::Color> opt_target_line_color_;

  int int4_carryover_progress_ = 0;   // L39
  int int4_last_move_part_completed_tick_ = 0;  // L40

  bool b_already_at_destination_ = false;  // L42
  std::vector<CPos> vec_path_;             // L43(null 区分:上游 OnLastRun
                                           // 置 null → C++ 清空 + has_path)
  bool b_has_path_ = true;
  std::optional<CPos> opt_destination_;    // L44
  int int4_start_ticks_ = 0;               // L45
  bool b_had_no_path_ = false;             // L46

  bool b_has_waited_ = false;              // L49
  int int4_wait_ticks_remaining_ = 0;      // L50

  bool b_evaluate_nearest_movable_cell_ = false;  // L53
};

}  // namespace ora::mods::activities
