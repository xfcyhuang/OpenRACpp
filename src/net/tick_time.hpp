// UPSTREAM: OpenRA.Game/Network/TickTime.cs @b6fc03f L16-60(逐语义重写;
//          Game.TimestepJankThreshold 常量随值内联 = 250,Game.RunTime 时钟
//          经回调注入 —— Game 静态面 Phase 5)
//          Verbatim-semantics rewrite; the Game.TimestepJankThreshold
//          constant inlined as 250 and the Game.RunTime clock injected via
//          callback — the Game statics land in Phase 5.
#pragma once
import std;

namespace ora::net {

class TickTime {
 public:
  static constexpr std::int64_t kTimestepJankThreshold = 250;  // Game.cs L37

  TickTime(std::function<int()> fn_timestep, std::int64_t last_tick_time)
      : fn_timestep_(std::move(fn_timestep)), value_(last_tick_time) {}

  std::int64_t Value() const { return value_; }
  void SetValue(std::int64_t v) { value_ = v; }

  bool ShouldAdvance(std::int64_t tick) {
    const auto i = fn_timestep_();

    if (i == 0)
      return false;

    const auto tick_delta = tick - value_;
    return tick_delta >= i;
  }

  void AdvanceTickTime(std::int64_t tick) {
    const auto tick_delta = tick - value_;

    const auto current_timestep = fn_timestep_();

    const auto integral_tick_timestep =
        tick_delta / current_timestep * current_timestep;
    value_ += integral_tick_timestep >= kTimestepJankThreshold
                  ? integral_tick_timestep
                  : current_timestep;
  }

 private:
  std::function<int()> fn_timestep_;
  std::int64_t value_ = 0;
};

}  // namespace ora::net
