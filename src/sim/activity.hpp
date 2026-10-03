// UPSTREAM: OpenRA.Game/Activities/Activity.cs @7d57605 L21-295(逐行重写)
//          + OpenRA.Game/Traits/ActivityUtils.cs L17-38
//          Line-by-line rewrite of Activity.cs + ActivityUtils.cs.
//
// 机制对照 / Mechanism mapping:
//  - ActivityState/State/ChildActivity/NextActivity/SkipDoneActivities/
//    TickOuter/TickChild/Cancel/Queue/QueueChild:控制流逐语句对应(含
//    lastRun/finishing 的"child 刚入队免延迟 tick"分支)
//    ActivityState/State/ChildActivity/NextActivity/SkipDoneActivities/
//    TickOuter/TickChild/Cancel/Queue/QueueChild: statement-level control
//    flow correspondence (including the lastRun/finishing "freshly queued
//    child skips a tick delay" branch).
//  - TargetLineNode/GetTargets/TargetLineNodes(PrintActivityTree):渲染域
//    (Sprite/Color/Target 线),Phase 4/6 随 Target 渲染面落地 —— 本文件不引入
//    TargetLineNode/GetTargets/TargetLineNodes (PrintActivityTree): render
//    domain (Sprite/Color/Target lines), landing with the Target render
//    surface in Phase 4/6 — not pulled into this file.
//  - ActivitiesImplementing<T>:IActivityInterface 标记 + 动态转型的模板
//    等价物(dynamic_cast;上游 is T + cast)
//    ActivitiesImplementing<T>: the template equivalent of the
//    IActivityInterface marker + runtime cast (dynamic_cast; upstream is
//    `is T` + cast).
#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class Actor;

/// Activity.cs L21
enum class ActivityState { Queued, Active, Canceling, Done };

/// Activity.cs L49-295
class Activity : public IActivityInterface {
 public:
  ActivityState State() const { return state_; }

  Activity* ChildActivity() { return SkipDoneActivities(child_activity_); }
  const Activity* ChildActivity() const {
    return SkipDoneActivities(child_activity_);
  }

  Activity* NextActivity() { return SkipDoneActivities(next_activity_); }
  const Activity* NextActivity() const {
    return SkipDoneActivities(next_activity_);
  }

  /// Activity.cs L67-80(internal static;子/next 链上跳过 Done 节点)
  /// Activity.cs L67-80 (internal static; skips Done nodes along the
  /// child/next chains).
  static Activity* SkipDoneActivities(Activity* first) {
    // If first.Cancel() was called while it was queued (i.e. before it first
    // ticked), its state will be Done rather than Queued (the activity
    // system guarantees that it cannot be Active or Canceling).
    // An unknown number of ticks may have elapsed between the Cancel() call
    // and now, so we cannot make any assumptions on the value of
    // first.NextActivity.
    // We must not return first (ticking it would be bogus), but returning
    // null would potentially drop valid activities queued after it. Walk the
    // queue until we find a valid activity or (more likely) run out of
    // activities.
    while (first != nullptr && first->state_ == ActivityState::Done)
      first = first->next_activity_;
    return first;
  }

  bool IsInterruptible() const { return b_is_interruptible_; }
  bool ChildHasPriority() const { return b_child_has_priority_; }
  bool IsCanceling() const { return state_ == ActivityState::Canceling; }

  /// Activity.cs L95-140
  Activity* TickOuter(Actor& self);

  /// Activity.cs L142-146(protected)
  bool TickChild(Actor& self);

  /// Activity.cs L166-169:每 tick 运行;true = 完成
  /// Activity.cs L166-169: runs every tick; true = complete.
  virtual bool Tick(Actor& /*self*/) { return true; }

  /// Activity.cs L174:首次 Tick 前执行一次
  /// Activity.cs L174: runs once immediately before the first Tick().
  virtual void OnFirstRun(Actor& /*self*/) {}

  /// Activity.cs L179:最后一次 Tick 后执行一次
  /// Activity.cs L179: runs once immediately after the last Tick().
  virtual void OnLastRun(Actor& /*self*/) {}

  /// Activity.cs L185:Actor.Dispose() 清理钩子(可强制触发 OnLastRun)
  /// Activity.cs L185: the Actor.Dispose() cleanup hook (may force-trigger
  /// OnLastRun).
  virtual void OnActorDispose(Actor& /*self*/) {}

  /// Activity.cs L191-196(internal;保证 ChildActivity 链同游)
  /// Activity.cs L191-196 (internal; walks the ChildActivity chain too).
  void OnActorDisposeOuter(Actor& self);

  /// Activity.cs L198-210
  virtual void Cancel(Actor& self, bool keep_queue = false);

  /// Activity.cs L212-218
  void Queue(Activity* activity);

  /// Activity.cs L220-226
  void QueueChild(Activity* activity);

  /// Activity.cs L276-294(跳过 Done 的 child/next 链上收集 T 实例)
  /// Activity.cs L276-294 (collects T instances along the child/next
  /// chains, skipping Done nodes).
  template <class T>
  std::vector<T*> ActivitiesImplementing(bool include_children = true) {
    std::vector<T*> out;
    CollectImplementing<T>(out, include_children);
    return out;
  }

 protected:
  Activity()
      : b_is_interruptible_(true),
        b_child_has_priority_(true) {}  // Activity.cs L89-93

  void SetChildActivity(Activity* a) { child_activity_ = a; }
  void SetNextActivity(Activity* a) { next_activity_ = a; }

  bool b_is_interruptible_;      // IsInterruptible(protected set)
  bool b_child_has_priority_;    // ChildHasPriority(protected set)

 private:
  template <class T>
  void CollectImplementing(std::vector<T*>& out, bool include_children) {
    if (include_children) {
      auto* ca = ChildActivity();
      if (ca != nullptr)
        ca->CollectImplementing<T>(out, include_children);
    }

    if (auto* t = dynamic_cast<T*>(this))
      out.push_back(t);

    auto* na = NextActivity();
    if (na != nullptr)
      na->CollectImplementing<T>(out, include_children);
  }

  ActivityState state_ = ActivityState::Queued;
  Activity* child_activity_ = nullptr;
  Activity* next_activity_ = nullptr;
  bool b_finishing_ = false;        // finishing
  bool b_first_run_completed_ = false;  // firstRunCompleted
  bool b_last_run_ = false;         // lastRun
};

/// ActivityUtils.cs L19-37:热路径 —— 同一活动返回自身则停(等下 tick)
/// ActivityUtils.cs L19-37: hot path — a self-returning activity stops the
/// loop (it waits for the next tick).
inline Activity* RunActivity(Actor& self, Activity* act) {
  if (act == nullptr)
    return act;

  do {
    auto* prev = act;
    act = act->TickOuter(self);
    if (act == prev)
      break;
  } while (act != nullptr);

  return act;
}

}  // namespace ora::sim
