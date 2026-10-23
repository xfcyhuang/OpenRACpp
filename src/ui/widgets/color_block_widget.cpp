// UPSTREAM: OpenRA.Mods.Common/Widgets/ColorBlockWidget.cs + Gradient-
// ColorBlockWidget.cs @e180e3225(color_block_widget.hpp 的实现)
// The implementation of color_block_widget.hpp.
import std;

#include "ui/widgets/color_block_widget.hpp"

#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

ColorBlockWidget::ColorBlockWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("modData"))
    if (const auto* ptr_mod = std::get_if<game::ModData*>(ptr_value))
      mod_rules = &(*ptr_mod)->DefaultRules();

  fn_get_color = [this] { return color_value; };
}

ColorBlockWidget::ColorBlockWidget(const ColorBlockWidget& widget_other)
    : Widget(widget_other),
      color_value(widget_other.color_value),
      fn_get_color(widget_other.fn_get_color),
      fn_on_mouse_down(widget_other.fn_on_mouse_down),
      fn_on_mouse_up(widget_other.fn_on_mouse_up),
      str_click_sound(widget_other.str_click_sound),
      mod_rules(widget_other.mod_rules) {}

void ColorBlockWidget::Draw() {
  FillRectWithColor(RenderBounds(), fn_get_color());
}

bool ColorBlockWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (mi_input.Button != MouseButton::Left)
    return false;

  if (mi_input.Event == MouseInputEvent::Down && !TakeMouseFocus(mi_input))
    return false;

  if (HasMouseFocus() && mi_input.Event == MouseInputEvent::Up) {
    // Only fire the onMouseUp event if we successfully lost focus, and were
    // pressed. The focus yields first and the callback fires off a stack
    // copy — a self-destroying OnClick takes the widget with it.
    const bool b_yielded = YieldMouseFocus(mi_input);
    auto fn_on_mouse_up_copy = fn_on_mouse_up;
    fn_on_mouse_up_copy(mi_input);

    return b_yielded;
  }

  if (mi_input.Event == MouseInputEvent::Down) {
    // OnMouseDown returns false if the button shouldn't be pressed
    fn_on_mouse_down(mi_input);

    Ui::Instance().PlayNotification(mod_rules, str_click_sound);
  }

  return false;
}

bool ColorBlockWidget::LoadFieldOrProperty(std::string_view str_key,
                                           std::string_view str_value) {
  if (str_key == "Color") {
    color_value = meta::GetColorValue("Color", str_value);
    return true;
  }
  if (str_key == "ClickSound") {
    str_click_sound = std::string{str_value};
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

GradientColorBlockWidget::GradientColorBlockWidget() {
  fn_get_top_left_color = [this] { return color_top_left; };
  fn_get_top_right_color = [this] { return color_top_right; };
  fn_get_bottom_right_color = [this] { return color_bottom_right; };
  fn_get_bottom_left_color = [this] { return color_bottom_left; };
}

GradientColorBlockWidget::GradientColorBlockWidget(
    const GradientColorBlockWidget& widget_other)
    : Widget(widget_other),
      color_top_left(widget_other.color_top_left),
      color_top_right(widget_other.color_top_right),
      color_bottom_right(widget_other.color_bottom_right),
      color_bottom_left(widget_other.color_bottom_left),
      fn_get_top_left_color(widget_other.fn_get_top_left_color),
      fn_get_top_right_color(widget_other.fn_get_top_right_color),
      fn_get_bottom_right_color(widget_other.fn_get_bottom_right_color),
      fn_get_bottom_left_color(widget_other.fn_get_bottom_left_color) {}

void GradientColorBlockWidget::Draw() {
  FillRectWithColor(RenderBounds(), fn_get_top_left_color(), fn_get_top_right_color(),
                    fn_get_bottom_right_color(), fn_get_bottom_left_color());
}

bool GradientColorBlockWidget::LoadFieldOrProperty(std::string_view str_key,
                                                   std::string_view str_value) {
  if (str_key == "TopLeftColor") {
    color_top_left = meta::GetColorValue("TopLeftColor", str_value);
    return true;
  }
  if (str_key == "TopRightColor") {
    color_top_right = meta::GetColorValue("TopRightColor", str_value);
    return true;
  }
  if (str_key == "BottomRightColor") {
    color_bottom_right = meta::GetColorValue("BottomRightColor", str_value);
    return true;
  }
  if (str_key == "BottomLeftColor") {
    color_bottom_left = meta::GetColorValue("BottomLeftColor", str_value);
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

const bool kWidgetRegistered_GradientColorBlockWidget = [] {
  WidgetTypeRegistry::Instance().Register(
      "GradientColorBlockWidget",
      [](WidgetArgs&) -> std::unique_ptr<Widget> {
        return std::make_unique<GradientColorBlockWidget>();
      });
  return true;
}();

}  // namespace ora::ui
