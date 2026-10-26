// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/PerfDebugLogic.cs @1f594e320 L21-81(全文)。
// 形态适配:Game.AfterGameStart 的静态事件 → Ui::Deps 注入的订阅面;
// Game.RenderFrame → fn_render_frame;析构退订 = 上游 Dispose 的 C++ 域。
#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::ui {

class PerfDebugLogic : public ChromeLogic {
 public:
  explicit PerfDebugLogic(WidgetArgs& args_args);
  ~PerfDebugLogic() override;

  const std::function<std::string()>& TextDelegateForTest() const {
    return fn_text_;
  }

 private:
  std::function<std::string()> fn_text_;
  std::function<void()> fn_unsubscribe_;
  std::vector<std::pair<std::int32_t, std::int64_t>> vec_frame_timings_;
  std::chrono::steady_clock::time_point time_fps_start_;
};

}  // namespace ora::ui
