#pragma once
import std;

#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "mods/mobile.hpp"
#include "mods/util.hpp"
#include "sim/activity.hpp"
#include "sim/target.hpp"

namespace ora::mods::activities {

template <class T, class... As>
T* NewActivity(sim::Actor& self, As&&... as) {
  return self.world().Arena().Create<T>(self, std::forward<As>(as)...);
}

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

class WaitFor final : public sim::Activity {
 public:
  explicit WaitFor(std::function<bool()> fn_predicate,
                   bool interruptible = true)
      : fn_{std::move(fn_predicate)} {
    b_is_interruptible_ = interruptible;
  }

  bool Tick(sim::Actor& self) override;

 private:
  std::function<bool()> fn_;
};

class Turn : public sim::Activity {
 public:
  Turn(sim::Actor& self, WAngle desired_facing);

  bool Tick(sim::Actor& self) override;

 private:
  Mobile* mobile_ = nullptr;
  sim::IFacing* facing_ = nullptr;
  WAngle angle_desired_facing_;
};

class Drag : public sim::Activity {
 public:
  Drag(sim::Actor& self, WPos start, WPos end, int length,
       std::optional<WAngle> facing = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  sim::IPositionable* positionable_ = nullptr;
  TraitBase* disableable_ = nullptr;
  WPos pos_start_;
  WPos pos_end_;
  int int4_length_;
  int int4_ticks_ = 0;
  std::optional<WAngle> opt_desired_facing_;
};

class Nudge : public sim::Activity {
 public:
  explicit Nudge(sim::Actor* nudger)
      : p_nudger_(nudger) {}

  void OnFirstRun(sim::Actor& self) override;

 private:
  sim::Actor* p_nudger_;
};

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

  void SetMobile(Mobile* p_mobile) { p_mobile_ = p_mobile; }

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

class MoveAdjacentTo : public sim::Activity {
 public:
  MoveAdjacentTo(sim::Actor& self, const sim::Target& target,
                 std::optional<WPos> initial_target_position = std::nullopt,
                 std::optional<core::Color> target_line_color = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 protected:

  struct PathEval {
    bool b_already_at_destination = false;
    std::vector<CPos> vec_path;
  };

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

  std::vector<CPos> vec_search_cells_;
  int int4_search_cells_tick_ = -1;
};

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

class MoveOntoAndTurn : public MoveOnto {
 public:
  MoveOntoAndTurn(sim::Actor& self, const sim::Target& target,
                  const WVec& offset, std::optional<WAngle> desired_facing,
                  std::optional<core::Color> target_line_color = std::nullopt);

  bool Tick(sim::Actor& self) override;

 private:
  std::optional<WAngle> opt_desired_facing_;
};

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

class Move : public sim::Activity {
 public:

  Move(sim::Actor& self, CPos destination,
       std::optional<core::Color> target_line_color = std::nullopt);

  Move(sim::Actor& self, CPos destination, WDist near_enough,
       Actor* ignore_actor, bool evaluate_nearest_movable_cell,
       std::optional<core::Color> target_line_color = std::nullopt);

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

  struct NextCellResult {
    std::optional<std::pair<CPos, sim::SubCell>> opt_next;
    bool b_should_try_again = false;
  };

  NextCellResult PopPath(sim::Actor& self);

 private:
  friend class MovePart;
  friend class MoveFirstHalf;
  friend class MoveSecondHalf;

  std::pair<bool, std::vector<CPos>> EvalPath(sim::BlockedByActor check);

  std::optional<std::pair<CPos, sim::SubCell>> UnblockDestination(
      sim::Actor& self);
  static bool CellIsEvacuating(sim::Actor& self, CPos cell);

  class MovePart;
  class MoveFirstHalf;
  class MoveSecondHalf;

  WAngle angle_actor_facing_modifier_;
  Mobile* mobile_ = nullptr;
  WDist dist_near_enough_;
  std::function<std::pair<bool, std::vector<CPos>>(sim::BlockedByActor)>
      fn_get_path_;
  Actor* p_ignore_actor_ = nullptr;
  std::optional<core::Color> opt_target_line_color_;

  int int4_carryover_progress_ = 0;
  int int4_last_move_part_completed_tick_ = 0;

  bool b_already_at_destination_ = false;
  std::vector<CPos> vec_path_;

  bool b_has_path_ = true;
  std::optional<CPos> opt_destination_;
  int int4_start_ticks_ = 0;
  bool b_had_no_path_ = false;

  bool b_has_waited_ = false;
  int int4_wait_ticks_remaining_ = 0;

  bool b_evaluate_nearest_movable_cell_ = false;
};

}
