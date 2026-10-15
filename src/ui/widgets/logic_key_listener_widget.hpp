// UPSTREAM: OpenRA.Mods.Common/Widgets/LogicKeyListenerWidget.cs @e180e3225 L19-36(全文)。
#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::ui {

class LogicKeyListenerWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "LogicKeyListenerWidget";

  LogicKeyListenerWidget() = default;
  explicit LogicKeyListenerWidget(WidgetArgs&) {}
  explicit LogicKeyListenerWidget(const LogicKeyListenerWidget& widget_other)
      : Widget(widget_other) {}
  ~LogicKeyListenerWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new LogicKeyListenerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void AddHandler(std::function<bool(const KeyInput&)> fn_handler);

  bool HandleKeyPress(const KeyInput& e_input) override;

 private:
  std::vector<std::function<bool(const KeyInput&)>> vec_handlers_;
};

}  // namespace ora::ui
