// UPSTREAM: OpenRA.Mods.Common/Widgets/BackgroundWidget.cs @e180e3225
// (background_widget.hpp 的实现)
// The implementation of background_widget.hpp.
import std;

#include "ui/widgets/background_widget.hpp"

#include "meta/field_loader.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

BackgroundWidget::BackgroundWidget(const BackgroundWidget& widget_other)
    : Widget(widget_other),
      b_click_through(widget_other.b_click_through),
      str_background(widget_other.str_background) {}

void BackgroundWidget::Draw() {
  DrawPanel(str_background, RenderBounds());
}

bool BackgroundWidget::HandleMouseInput(const MouseInput& mi_input) {
  return !b_click_through && EventBounds().Contains(mi_input.Location);
}

bool BackgroundWidget::LoadFieldOrProperty(std::string_view str_key,
                                           std::string_view str_value) {
  if (str_key == "ClickThrough") {
    b_click_through = meta::GetBoolValue("ClickThrough", str_value);
    return true;
  }
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
