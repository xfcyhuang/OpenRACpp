import std;

#include "ui/widgets/scroll_item_widget.hpp"

#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

ScrollItemWidget::ScrollItemWidget(WidgetArgs& args_args)
    : ButtonWidget(args_args),

      cache_panel_(GetCachedStatefulPanelImages(str_background)) {
  fn_is_visible = [] { return false; };
  int4_visual_height = 0;
}

ScrollItemWidget::ScrollItemWidget(const ScrollItemWidget& widget_other)
    : ButtonWidget(widget_other),
      str_background(widget_other.str_background),
      b_enable_child_mouse_over(widget_other.b_enable_child_mouse_over),
      str_item_key(widget_other.str_item_key),
      cache_panel_(GetCachedStatefulPanelImages(str_background)) {
  key_hotkey = widget_other.key_hotkey;
  fn_is_visible = [] { return false; };
  int4_visual_height = 0;
}

void ScrollItemWidget::Initialize(WidgetArgs& args_args) {
  ButtonWidget::Initialize(args_args);

  b_ignore_child_mouse_over = !b_enable_child_mouse_over;
}

void ScrollItemWidget::Draw() {
  if (str_background.empty())
    return;

  bool b_hover = Ui::Instance().MouseOverWidget() == this;
  if (!b_ignore_child_mouse_over && !b_hover)
    b_hover = [this]() {
      for (Widget* ptr_child : Children())
        if (ptr_child == Ui::Instance().MouseOverWidget())
          return true;
      return false;
    }();

  DrawPanel(RenderBounds(),
            cache_panel_.Update(StatefulImageKey{
                fn_is_disabled(), b_depressed, b_hover, false,
                fn_is_selected() || fn_is_highlighted()}));
}

bool ScrollItemWidget::LoadFieldOrProperty(std::string_view str_key,
                                           std::string_view str_value) {
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }
  if (str_key == "EnableChildMouseOver") {
    b_enable_child_mouse_over = meta::GetBoolValue("EnableChildMouseOver", str_value);
    return true;
  }

  return ButtonWidget::LoadFieldOrProperty(str_key, str_value);
}

std::unique_ptr<ScrollItemWidget> ScrollItemWidget::Setup(
    ScrollItemWidget& widget_template, std::function<bool()> fn_is_selected,
    std::function<void()> fn_on_click) {
  auto up_widget = std::unique_ptr<ScrollItemWidget>(
      static_cast<ScrollItemWidget*>(widget_template.Clone().release()));
  up_widget->fn_is_visible = [] { return true; };
  up_widget->fn_is_selected = std::move(fn_is_selected);
  up_widget->fn_on_click = std::move(fn_on_click);
  return up_widget;
}

std::unique_ptr<ScrollItemWidget> ScrollItemWidget::Setup(
    ScrollItemWidget& widget_template, std::function<bool()> fn_is_selected,
    std::function<void()> fn_on_click, std::function<void()> fn_on_double_click) {
  auto up_widget = Setup(widget_template, std::move(fn_is_selected), std::move(fn_on_click));
  up_widget->fn_on_double_click = std::move(fn_on_double_click);
  return up_widget;
}

std::unique_ptr<ScrollItemWidget> ScrollItemWidget::Setup(
    const std::string& str_key, ScrollItemWidget& widget_template,
    std::function<bool()> fn_is_selected, std::function<void()> fn_on_click,
    std::function<void()> fn_on_double_click) {
  auto up_widget = Setup(widget_template, std::move(fn_is_selected), std::move(fn_on_click));
  up_widget->fn_on_double_click = std::move(fn_on_double_click);
  up_widget->str_item_key = str_key;
  return up_widget;
}

}
