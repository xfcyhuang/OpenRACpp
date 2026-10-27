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

}
