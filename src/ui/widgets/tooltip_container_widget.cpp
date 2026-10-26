// UPSTREAM: OpenRA.Mods.Common/Widgets/TooltipContainerWidget.cs @1f594e320
// (tooltip_container_widget.hpp 的实现)
// The implementation of tooltip_container_widget.hpp.
import std;

#include "ui/widgets/tooltip_container_widget.hpp"

#include "gfx/viewport.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"

namespace ora::ui {
namespace {

bool CursorDoubleOrDefault() {
  return Ui::HasInstance() && Ui::Instance().CursorDouble();
}

}  // namespace

TooltipContainerWidget::TooltipContainerWidget() {
  fn_is_visible = [this] {
    const std::int64_t int8_run_time = Ui::Instance().RunTime();
    const bool b_visible =
        int8_run_time > gfx::Viewport::int8_last_move_run_time +
                            static_cast<std::int64_t>(int4_tooltip_delay_milliseconds);
    if (b_visible)
      LoadWidgetInner();

    return b_visible;
  };
}

TooltipContainerWidget::TooltipContainerWidget(const TooltipContainerWidget& widget_other)
    : Widget(widget_other),
      int2_cursor_offset(widget_other.int2_cursor_offset),
      int4_bottom_edge_y_offset(widget_other.int4_bottom_edge_y_offset),
      int4_tooltip_delay_milliseconds(widget_other.int4_tooltip_delay_milliseconds) {}

int TooltipContainerWidget::SetTooltip(const std::string& str_id, WidgetArgs args_widget) {
  RemoveTooltip();
  int4_current_token_ = int4_next_token_++;
  ptr_tooltip_ = nullptr;
  str_id_ = str_id;
  opt_widget_args_ = std::move(args_widget);
  return int4_current_token_;
}

void TooltipContainerWidget::RemoveTooltip(int int4_token) {
  if (int4_current_token_ != int4_token)
    return;

  ptr_tooltip_ = nullptr;
  str_id_.clear();
  opt_widget_args_.reset();

  RemoveChildren();
  fn_before_render = [] {};
}

void TooltipContainerWidget::RemoveTooltip() {
  RemoveTooltip(int4_current_token_);
}

void TooltipContainerWidget::Draw() {
  fn_before_render();
}

int2 TooltipContainerWidget::ChildOrigin() const {
  const int int4_scale = CursorDoubleOrDefault() ? 2 : 1;
  int2 int2_pos = gfx::Viewport::int2_last_mouse_pos +
                  int2{int4_scale * int2_cursor_offset.X, int4_scale * int2_cursor_offset.Y};
  if (ptr_tooltip_ != nullptr && Ui::HasInstance()) {
    const int2 int2_resolution = Ui::Instance().Resolution();

    // If the tooltip overlaps the right edge of the screen, move it left
    // until it fits
    if (int2_pos.X + ptr_tooltip_->bounds_widget.Right() > int2_resolution.X)
      int2_pos = int2_pos.WithX(int2_resolution.X - ptr_tooltip_->bounds_widget.Right());

    // If the tooltip overlaps the bottom edge of the screen, switch tooltip
    // above cursor
    if (int2_pos.Y + ptr_tooltip_->bounds_widget.Bottom() > int2_resolution.Y)
      int2_pos = int2_pos.WithY(gfx::Viewport::int2_last_mouse_pos.Y +
                                int4_scale * int4_bottom_edge_y_offset -
                                ptr_tooltip_->bounds_widget.int4_height);
  }

  return int2_pos;
}

bool TooltipContainerWidget::LoadFieldOrProperty(std::string_view str_key,
                                                 std::string_view str_value) {
  if (str_key == "CursorOffset") {
    const std::string str_value_owned{str_value};
    const std::size_t int8_comma = str_value_owned.find(',');
    if (int8_comma != std::string::npos) {
      int2_cursor_offset = int2{std::stoi(str_value_owned.substr(0, int8_comma)),
                                std::stoi(str_value_owned.substr(int8_comma + 1))};
      return true;
    }
    throw std::runtime_error("FieldLoader: Cannot parse `" + str_value_owned +
                             "` into field `CursorOffset` of type `OpenRA.Primitives.int2`");
  }
  if (str_key == "BottomEdgeYOffset") {
    int4_bottom_edge_y_offset = std::stoi(std::string{str_value});
    return true;
  }
  if (str_key == "TooltipDelayMilliseconds") {
    int4_tooltip_delay_milliseconds = std::stoi(std::string{str_value});
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

void TooltipContainerWidget::LoadWidgetInner() {
  // PERF: Only load widget once visible(上游注释)| PERF: only load the
  // widget once visible (the upstream comment)
  if (str_id_.empty() || ptr_tooltip_ != nullptr)
    return;

  WidgetArgs args_widget = opt_widget_args_.value_or(WidgetArgs{});
  args_widget.Set("tooltipContainer", static_cast<Widget*>(this));
  ptr_tooltip_ = Ui::Instance().LoadWidget(str_id_, this, std::move(args_widget));
}

}  // namespace ora::ui
