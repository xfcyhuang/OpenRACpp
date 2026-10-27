#include "sim/activity.hpp"

#include "sim/actor.hpp"

namespace ora::sim {

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

  if (b_child_has_priority_) {
    b_last_run_ = TickChild(self) && (b_finishing_ || Tick(self));
    b_finishing_ = b_finishing_ || b_last_run_;
  } else {

    b_last_run_ = Tick(self);
  }

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

bool Activity::TickChild(Actor& self) {
  child_activity_ = RunActivity(self, child_activity_);
  return child_activity_ == nullptr;
}

void Activity::OnActorDisposeOuter(Actor& self) {
  if (child_activity_ != nullptr)
    child_activity_->OnActorDisposeOuter(self);

  OnActorDispose(self);
}

void Activity::Cancel(Actor& self, bool keep_queue) {
  if (!keep_queue)
    next_activity_ = nullptr;

  if (!b_is_interruptible_)
    return;

  if (child_activity_ != nullptr)
    child_activity_->Cancel(self);

  state_ = state_ == ActivityState::Queued ? ActivityState::Done
                                           : ActivityState::Canceling;
}

void Activity::Queue(Activity* activity) {
  auto* it = this;
  while (it->next_activity_ != nullptr)
    it = it->next_activity_;
  it->next_activity_ = activity;
}

void Activity::QueueChild(Activity* activity) {
  if (child_activity_ != nullptr)
    child_activity_->Queue(activity);
  else
    child_activity_ = activity;
}

}
