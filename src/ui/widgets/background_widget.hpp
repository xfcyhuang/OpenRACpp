// UPSTREAM: OpenRA.Mods.Common/Widgets/BackgroundWidget.cs @e180e3225 L18-43(全文)。
// The whole of BackgroundWidget.cs L18-43.
#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::ui {

class BackgroundWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "BackgroundWidget";

  BackgroundWidget() = default;
  explicit BackgroundWidget(const BackgroundWidget& widget_other);
  ~BackgroundWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new BackgroundWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool b_click_through = false;
  std::string str_background = "dialog";

  void Draw() override;
  bool HandleMouseInput(const MouseInput& mi_input) override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
};

}  // namespace ora::ui
