#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class Actor;

enum class ActivityState { Queued, Active, Canceling, Done };

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

  static Activity* SkipDoneActivities(Activity* first) {

    while (first != nullptr && first->state_ == ActivityState::Done)
      first = first->next_activity_;
    return first;
  }

  bool IsInterruptible() const { return b_is_interruptible_; }
  bool ChildHasPriority() const { return b_child_has_priority_; }
  bool IsCanceling() const { return state_ == ActivityState::Canceling; }

  Activity* TickOuter(Actor& self);

  bool TickChild(Actor& self);

  virtual bool Tick(Actor& ) { return true; }

  virtual void OnFirstRun(Actor& ) {}

  virtual void OnLastRun(Actor& ) {}

  virtual void OnActorDispose(Actor& ) {}

  virtual void GetTargets(Actor& , std::vector<Target>& vec_out) {
    (void)vec_out;
  }

  void OnActorDisposeOuter(Actor& self);

  virtual void Cancel(Actor& self, bool keep_queue = false);

  void Queue(Activity* activity);

  void QueueChild(Activity* activity);

  template <class T>
  std::vector<T*> ActivitiesImplementing(bool include_children = true) {
    std::vector<T*> out;
    CollectImplementing<T>(out, include_children);
    return out;
  }

 protected:
  Activity()
      : b_is_interruptible_(true),
        b_child_has_priority_(true) {}

  void SetChildActivity(Activity* a) { child_activity_ = a; }
  void SetNextActivity(Activity* a) { next_activity_ = a; }

  bool b_is_interruptible_;
  bool b_child_has_priority_;

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
  bool b_finishing_ = false;
  bool b_first_run_completed_ = false;
  bool b_last_run_ = false;
};

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

}
