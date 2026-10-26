// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/PerfDebugLogic.cs @1f594e320(实现)。
import std;

#include "ui/logic/perf_debug_logic.hpp"

#include "game/perf_history.hpp"
#include "game/settings.hpp"
#include "gfx/renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {
namespace {

std::int64_t NowMs(std::chrono::steady_clock::time_point time_start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - time_start)
      .count();
}

std::string FormatFps(double fp8_fps) {
  if (std::isnan(fp8_fps))
    return "NaN";
  if (std::isinf(fp8_fps))
    return fp8_fps > 0 ? "\u221E" : "-\u221E";
  return std::format("{}", std::llround(fp8_fps));
}

std::string FormatF1(double fp8_value) {
  const double fp8_rounded =
      std::copysign(std::round(std::abs(fp8_value) * 10.0) / 10.0, fp8_value);
  return std::format("{:.1f}", fp8_rounded);
}

}  // namespace

PerfDebugLogic::PerfDebugLogic(WidgetArgs& args_args) {
  Widget* ptr_widget = std::get<Widget*>(args_args.At("widget"));
  gfx::WorldRenderer* ptr_world_renderer =
      std::get<gfx::WorldRenderer*>(args_args.At("worldRenderer"));

  const auto fn_debug_bool = [](std::string_view sv_field) {
    const game::SettingsModule* ptr_module =
        game::Settings::Instance().Module("Debug");
    if (ptr_module == nullptr)
      return false;
    const std::optional<std::size_t> opt_slot =
        ptr_module->SlotIndexOf(sv_field);
    return opt_slot.has_value() && ptr_module->GetBool(*opt_slot);
  };

  Widget* ptr_perf_graph = ptr_widget->Get("GRAPH_BG");
  ptr_perf_graph->fn_is_visible = [fn_debug_bool] {
    return fn_debug_bool("PerfGraph");
  };

  LabelWidget* ptr_perf_text = ptr_widget->Get<LabelWidget>("PERF_TEXT");
  ptr_perf_text->fn_is_visible = [fn_debug_bool] {
    return fn_debug_bool("PerfText");
  };

  time_fps_start_ = std::chrono::steady_clock::now();
  vec_frame_timings_.reserve(32);
  vec_frame_timings_.emplace_back(
      Ui::Instance().DepsForTest().fn_render_frame(), std::int64_t{0});

  fn_text_ = [this, ptr_world_renderer] {
    const std::int32_t int4_frame_now =
        Ui::Instance().DepsForTest().fn_render_frame();
    vec_frame_timings_.emplace_back(int4_frame_now, NowMs(time_fps_start_));

    const std::int64_t int8_cutoff =
        vec_frame_timings_.back().second - 1000;
    std::size_t sz_first_past_cutoff = 0;
    while (sz_first_past_cutoff < vec_frame_timings_.size() &&
           vec_frame_timings_[sz_first_past_cutoff].second < int8_cutoff)
      sz_first_past_cutoff++;

    if (vec_frame_timings_.size() - sz_first_past_cutoff >= 2)
      vec_frame_timings_.erase(
          vec_frame_timings_.begin(),
          vec_frame_timings_.begin() +
              static_cast<std::ptrdiff_t>(sz_first_past_cutoff));

    const auto [int4_oldest_frame, int8_oldest_time] =
        vec_frame_timings_.front();
    const auto [int4_newest_frame, int8_newest_time] =
        vec_frame_timings_.back();
    const double fp8_seconds =
        static_cast<double>(int8_newest_time - int8_oldest_time) / 1000.0;
    const double fp8_fps =
        static_cast<double>(int4_newest_frame - int4_oldest_frame) /
        fp8_seconds;

    const game::SettingsModule* ptr_module_debug =
        game::Settings::Instance().Module("Debug");
    std::int64_t int4_samples = 25;
    if (ptr_module_debug != nullptr) {
      if (const std::optional<std::size_t> opt_slot =
              ptr_module_debug->SlotIndexOf("Samples");
          opt_slot.has_value())
        int4_samples = ptr_module_debug->GetInt(*opt_slot);
    }

    int2 int2_wfb_size{0, 0};
    std::int32_t int4_downscale = 1;
    if (const gfx::Renderer* ptr_renderer =
            Ui::Instance().RendererDeps()) {
      int2_wfb_size = ptr_renderer->WorldFrameBufferSize();
      int4_downscale = ptr_renderer->WorldDownscaleFactor();
    }

    gfx::IViewportSurface* ptr_viewport = ptr_world_renderer->Viewport();
    const int2 int2_viewport_size =
        ptr_viewport != nullptr ? ptr_viewport->ViewportSize() : int2{0, 0};

    game::PerfHistory& history_perf = game::PerfHistory::Instance();
    return std::format(
        "FPS: {}\n"
        "Tick: {} / {} @ {} ms\n"
        "Render {} @ {} ms\n"
        "Batches: {}\n"
        "Viewport Size: {} x {} / {}\n"
        "WFB Size: {} x {}",
        FormatFps(fp8_fps),
        ptr_world_renderer->World().WorldTick(),
        Ui::Instance().DepsForTest().fn_local_tick(),
        FormatF1(history_perf.Item("tick_time").Average(
            static_cast<int>(int4_samples))),
        int4_frame_now,
        FormatF1(history_perf.Item("render").Average(
            static_cast<int>(int4_samples))),
        history_perf.Item("batches").LastValue(),
        int2_viewport_size.X, int2_viewport_size.Y, int4_downscale,
        int2_wfb_size.X, int2_wfb_size.Y);
  };
  ptr_perf_text->fn_get_text = fn_text_;

  fn_unsubscribe_ = Ui::Instance().DepsForTest().fn_add_after_game_start(
      [this] {
        vec_frame_timings_.clear();
        vec_frame_timings_.emplace_back(
            Ui::Instance().DepsForTest().fn_render_frame(),
            NowMs(time_fps_start_));
      });
}

PerfDebugLogic::~PerfDebugLogic() {
  if (fn_unsubscribe_)
    fn_unsubscribe_();
}

}  // namespace ora::ui
