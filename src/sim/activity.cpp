// UPSTREAM: OpenRA.Game/Activities/Activity.cs @b6fc03f L95-226(实现部分)
//          The implementation half of Activity.cs.
#include "sim/activity.hpp"

#include "sim/actor.hpp"

namespace ora::sim {

// Activity.cs L95-140
Activity* Activity::TickOuter(Actor& self) {
  if (state_ == ActivityState::Done)
    throw std::runtime_error(
        "Actor " + self.DebugName() +
        " attempted to tick activity after it had already completed.");

  if (state_ == ActivityState::Queued) {
    OnFirstRun(self);
    b_first_run_completed_ = true;
    state_ = ActivityState::Active;
  }

  if (!b_first_run_completed_)
    throw std::runtime_error(
        "Actor " + self.DebugName() +
        " attempted to tick activity before running its OnFirstRun method.");

  // Only run the parent tick when the child is done.
  // We must always let the child finish on its own before continuing.
  if (b_child_has_priority_) {
    b_last_run_ = TickChild(self) && (b_finishing_ || Tick(self));
    b_finishing_ = b_finishing_ || b_last_run_;
  } else {
    // The parent determines whether the child gets a chance at ticking.
    b_last_run_ = Tick(self);
  }

  // Avoid a single tick delay if the childactivity was just queued.
  auto* ca = ChildActivity();
  if (ca != nullptr && ca->state_ == ActivityState::Queued) {
    if (b_child_has_priority_)
      b_last_run_ = TickChild(self) && b_finishing_;
    else
      TickChild(self);
  }

  if (b_last_run_) {
    state_ = ActivityState::Done;
    OnLastRun(self);
    return NextActivity();
  }

  return this;
}

// Activity.cs L142-146
bool Activity::TickChild(Actor& self) {
  child_activity_ = RunActivity(self, child_activity_);
  return child_activity_ == nullptr;
}

// Activity.cs L191-196
void Activity::OnActorDisposeOuter(Actor& self) {
  if (child_activity_ != nullptr)
    child_activity_->OnActorDisposeOuter(self);

  OnActorDispose(self);
}

// Activity.cs L198-210
void Activity::Cancel(Actor& self, bool keep_queue) {
  if (!keep_queue)
    next_activity_ = nullptr;

  if (!b_is_interruptible_)
    return;

  if (child_activity_ != nullptr)
    child_activity_->Cancel(self);

  // Directly mark activities that are queued and therefore didn't run yet as done
  state_ = state_ == ActivityState::Queued ? ActivityState::Done
                                           : ActivityState::Canceling;
}

// Activity.cs L212-218
void Activity::Queue(Activity* activity) {
  auto* it = this;
  while (it->next_activity_ != nullptr)
    it = it->next_activity_;
  it->next_activity_ = activity;
}

// Activity.cs L220-226
void Activity::QueueChild(Activity* activity) {
  if (child_activity_ != nullptr)
    child_activity_->Queue(activity);
  else
    child_activity_ = activity;
}

}  // namespace ora::sim
