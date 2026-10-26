// UPSTREAM: OpenRA.Mods.Common/Widgets/LogicTickerWidget.cs @1f594e320 L19-22(全文)。
#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::ui {

class LogicTickerWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "LogicTickerWidget";

  LogicTickerWidget() = default;
  explicit LogicTickerWidget(WidgetArgs&) {}
  explicit LogicTickerWidget(const LogicTickerWidget& widget_other)
      : Widget(widget_other), fn_on_tick(widget_other.fn_on_tick) {}
  ~LogicTickerWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new LogicTickerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::function<void()> fn_on_tick = [] {};

  void Tick() override { fn_on_tick(); }
};

}  // namespace ora::ui
