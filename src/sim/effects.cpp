// UPSTREAM: OpenRA.Game/Effects/DelayedAction.cs @7d57605 L18-36(实现)
//          DelayedAction implementation.
#include "sim/effects.hpp"

#include "sim/world.hpp"

namespace ora::sim {

void DelayedAction::Tick(World& world) {
  // L29-33:--delay <= 0 时帧末"移除自身后触发闭包"(单任务,顺序与上游
  // 一致;fn 为拷贝 —— Remove 析构本对象后闭包仍安全,与 C# 委托捕获等价)
  if (--int4_delay_ <= 0) {
    auto fn = fn_;
    world.AddFrameEndTask([this, fn](World& w) {
      w.Remove(this);
      fn();
    });
  }
}

}  // namespace ora::sim
