#pragma once
import std;

namespace ora::core {

struct DelayedAction {
  std::int64_t int8_time = 0;
  std::function<void()> fn_action;

  std::string ToString() const {
    std::ostringstream oss;
    oss << "Time: " << int8_time << " Action: " << (fn_action ? "<action>" : "<null>");
    return oss.str();
  }
};

class ActionQueue {
 public:

  void Add(std::function<void()> fn_a, std::int64_t int8_desired_time) {

    if (!fn_a)
      throw std::invalid_argument("Value cannot be null. (Parameter 'a')");

    std::lock_guard lock(mutex_sync_);
    const std::size_t int4_index = Index(int8_desired_time);
    vec_actions_.insert(vec_actions_.begin() + static_cast<std::ptrdiff_t>(int4_index),
                        DelayedAction{int8_desired_time, std::move(fn_a)});
  }

  void PerformActions(std::int64_t int8_current_time) {
    std::vector<DelayedAction> vec_pending;
    {
      std::lock_guard lock(mutex_sync_);
      const std::size_t int4_index = Index(int8_current_time);
      if (int4_index == 0)
        return;

      vec_pending.assign(vec_actions_.begin(),
                         vec_actions_.begin() + static_cast<std::ptrdiff_t>(int4_index));
      vec_actions_.erase(vec_actions_.begin(),
                         vec_actions_.begin() + static_cast<std::ptrdiff_t>(int4_index));
    }

    for (DelayedAction& action_delayed : vec_pending)
      action_delayed.fn_action();
  }

  std::size_t size() const {
    std::lock_guard lock(mutex_sync_);
    return vec_actions_.size();
  }

  void Clear() {
    std::lock_guard lock(mutex_sync_);
    vec_actions_.clear();
  }

 private:

  std::size_t Index(std::int64_t int8_time) const {
    return static_cast<std::size_t>(
        std::upper_bound(vec_actions_.begin(), vec_actions_.end(), int8_time,
                         [](std::int64_t int8_lhs, const DelayedAction& action_rhs) {
                           return int8_lhs < action_rhs.int8_time;
                         }) -
        vec_actions_.begin());
  }

  mutable std::mutex mutex_sync_;
  std::vector<DelayedAction> vec_actions_;
};

}
