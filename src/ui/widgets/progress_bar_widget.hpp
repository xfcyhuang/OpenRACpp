// UPSTREAM: OpenRA.Mods.Common/Widgets/ProgressBarWidget.cs @1f594e320 L19-85(全文)。
// 定态/不定态双面(Tick 的 offset 往返 + tickStep 翻转;wasIndeterminate 的
// 复位语义);ChromeProvider.GetMinimumPanelSize 的最小条宽。
// The whole of ProgressBarWidget.cs L19-85. The determinate/indeterminate
// pair (Tick's offset ping-pong + the tickStep flip; wasIndeterminate's
// reset semantics); ChromeProvider.GetMinimumPanelSize's minimum bar width.
#pragma once
import std;

#include "core/int2.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class ProgressBarWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ProgressBarWidget";

  ProgressBarWidget();
  explicit ProgressBarWidget(const ProgressBarWidget& widget_other);
  ~ProgressBarWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ProgressBarWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_background = "progressbar-bg";
  std::string str_bar = "progressbar-thumb";
  int2 int2_bar_margin{2, 2};

  int int4_percentage = 0;
  bool b_indeterminate = false;

  std::function<int()> fn_get_percentage;
  std::function<bool()> fn_is_indeterminate;

  void Draw() override;
  void Tick() override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

 private:
  float fp4_offset_ = 0.0f;
  float fp4_tick_step_ = 0.04f;
  bool b_was_indeterminate_ = false;
};

}  // namespace ora::ui
