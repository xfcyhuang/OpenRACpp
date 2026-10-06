// UPSTREAM: OpenRA.Game/Primitives/ActionQueue.cs @b6fc03f(线程安全延迟动作
// 队列逐语义:时间升序稳定插入、锁内切片/锁外执行;C# BinarySearch+推进循环
// 的等值段末插入 = upper_bound)
// Verbatim-semantics rewrite of the thread-safe delayed-action queue
// (ascending-time stable insertion; the C# BinarySearch-then-advance
// insertion lands on upper_bound).
#pragma once
import std;

namespace ora::core {

/// DelayedAction(L69-94)
/// DelayedAction (L69-94).
struct DelayedAction {
  std::int64_t int8_time = 0;
  std::function<void()> fn_action;

  std::string ToString() const {
    std::ostringstream oss;
    oss << "Time: " << int8_time << " Action: " << (fn_action ? "<action>" : "<null>");
    return oss.str();
  }
};

/// ActionQueue(L21-67)
/// ActionQueue (L21-67).
class ActionQueue {
 public:
  /// Add(L26-36):同值时间稳定插到段末(上游 Index 的 while 推进语义)。
  /// Add (L26-36): equal times insert stably at the run's end (upstream
  /// Index's advance loop).
  void Add(std::function<void()> fn_a, std::int64_t int8_desired_time) {
    // ArgumentNullException.ThrowIfNull(a) 等价 | the ThrowIfNull equivalent
    if (!fn_a)
      throw std::invalid_argument("Value cannot be null. (Parameter 'a')");

    std::lock_guard lock(mutex_sync_);
    const std::size_t int4_index = Index(int8_desired_time);
    vec_actions_.insert(vec_actions_.begin() + static_cast<std::ptrdiff_t>(int4_index),
                        DelayedAction{int8_desired_time, std::move(fn_a)});
  }

  /// PerformActions(L38-55):锁内切片出队,锁外执行(执行期可重入 Add 不死锁)。
  /// PerformActions (L38-55): slices out of the queue under the lock, runs
  /// with the lock released (a reentrant Add during execution cannot
  /// deadlock).
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

  /// 测试面:当前排队数 | Test face: the queued count.
  std::size_t size() const {
    std::lock_guard lock(mutex_sync_);
    return vec_actions_.size();
  }

 private:
  /// Index(L57-66):首个严格更大时间的位置 | the first strictly-greater
  /// position (L57-66).
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

}  // namespace ora::core
