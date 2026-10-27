import std;

#include "ui/widgets/scroll_panel_widget.hpp"

#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/scroll_item_widget.hpp"

namespace ora::ui {

void ListLayout::AdjustChild(Widget& widget_child) {
  if (ptr_widget_->OwnedChildren().empty())
    ptr_widget_->int4_content_height =
        2 * ptr_widget_->int4_top_bottom_spacing - ptr_widget_->int4_item_spacing;

  widget_child.bounds_widget.int4_y = ptr_widget_->int4_content_height -
                                      ptr_widget_->int4_top_bottom_spacing +
                                      ptr_widget_->int4_item_spacing;
  if (!ptr_widget_->b_collapse_hidden_children || widget_child.fn_is_visible())
    ptr_widget_->int4_content_height +=
        widget_child.bounds_widget.int4_height + ptr_widget_->int4_item_spacing;
}

void ListLayout::AdjustChildren() {
  ptr_widget_->int4_content_height = ptr_widget_->int4_top_bottom_spacing;
  for (const auto& up_child : ptr_widget_->OwnedChildren()) {
    up_child->bounds_widget.int4_y = ptr_widget_->int4_content_height;
    if (!ptr_widget_->b_collapse_hidden_children || up_child->fn_is_visible())
      ptr_widget_->int4_content_height +=
          up_child->bounds_widget.int4_height + ptr_widget_->int4_item_spacing;
  }

  ptr_widget_->int4_content_height +=
      ptr_widget_->int4_top_bottom_spacing - ptr_widget_->int4_item_spacing;
}

void GridLayout::AdjustChild(Widget& widget_child) {
  if (ptr_widget_->OwnedChildren().empty()) {
    ptr_widget_->int4_content_height = 2 * ptr_widget_->int4_top_bottom_spacing;
    int2_pos = int2{ptr_widget_->int4_item_spacing, ptr_widget_->int4_top_bottom_spacing};
  }

  if (int2_pos.X + widget_child.bounds_widget.int4_width + ptr_widget_->int4_item_spacing >
      ptr_widget_->bounds_widget.int4_width - ptr_widget_->int4_scrollbar_width) {

    int2_pos = int2{ptr_widget_->int4_item_spacing,
                    ptr_widget_->int4_content_height -
                        ptr_widget_->int4_top_bottom_spacing +
                        ptr_widget_->int4_item_spacing};
  }

  widget_child.bounds_widget.int4_x += int2_pos.X;
  widget_child.bounds_widget.int4_y += int2_pos.Y;

  int2_pos = int2_pos.WithX(int2_pos.X + widget_child.bounds_widget.int4_width +
                            ptr_widget_->int4_item_spacing);

  ptr_widget_->int4_content_height =
      std::max(ptr_widget_->int4_content_height,
               int2_pos.Y + widget_child.bounds_widget.int4_height +
                   ptr_widget_->int4_top_bottom_spacing);
}

ScrollPanelWidget::ScrollPanelWidget(WidgetArgs& args_args)
    : cache_up_arrow_(GetCachedStatefulImage(str_decorations, str_decoration_scroll_up)),
      cache_down_arrow_(GetCachedStatefulImage(str_decorations, str_decoration_scroll_down)) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("modData"))
    if (const auto* ptr_mod = std::get_if<game::ModData*>(ptr_value))
      mod_rules = &(*ptr_mod)->DefaultRules();

  ChromeMetrics& metrics = RequireChromeMetrics();
  int4_button_depth = metrics.GetInt32("ButtonDepth");
  str_click_sound = metrics.Get("ClickSound");
  str_click_disabled_sound = metrics.Get("ClickDisabledSound");

  ptr_layout = std::make_unique<ListLayout>(*this);
}

ScrollPanelWidget::ScrollPanelWidget(const ScrollPanelWidget& widget_other)
    : Widget(widget_other),
      int4_scrollbar_width(widget_other.int4_scrollbar_width),
      int4_border_width(widget_other.int4_border_width),
      int4_top_bottom_spacing(widget_other.int4_top_bottom_spacing),
      int4_item_spacing(widget_other.int4_item_spacing),
      int4_button_depth(widget_other.int4_button_depth),
      str_click_sound(widget_other.str_click_sound),
      str_click_disabled_sound(widget_other.str_click_disabled_sound),
      str_background(widget_other.str_background),
      str_scrollbar_background(widget_other.str_scrollbar_background),
      str_button(widget_other.str_button),
      str_decorations(widget_other.str_decorations),
      str_decoration_scroll_up(widget_other.str_decoration_scroll_up),
      str_decoration_scroll_down(widget_other.str_decoration_scroll_down),
      int4_content_height(widget_other.int4_content_height),
      int4_minimum_thumb_size(widget_other.int4_minimum_thumb_size),
      kind_align(widget_other.kind_align),
      kind_scrollbar(widget_other.kind_scrollbar),
      b_collapse_hidden_children(widget_other.b_collapse_hidden_children),
      fp4_smooth_scroll_speed(widget_other.fp4_smooth_scroll_speed),
      mod_rules(widget_other.mod_rules),
      cache_up_arrow_(GetCachedStatefulImage(str_decorations, str_decoration_scroll_up)),
      cache_down_arrow_(GetCachedStatefulImage(str_decorations, str_decoration_scroll_down)) {
  ptr_layout = std::make_unique<ListLayout>(*this);
}

void ScrollPanelWidget::RemoveChildren() {
  int4_content_height = 0;
  Widget::RemoveChildren();
  Scroll(0);
}

void ScrollPanelWidget::AddChild(std::unique_ptr<Widget> ptr_child) {

  ptr_layout->AdjustChild(*ptr_child);
  Widget::AddChild(std::move(ptr_child));
}

std::unique_ptr<Widget> ScrollPanelWidget::RemoveChild(Widget& child_widget) {
  std::unique_ptr<Widget> up_removed = Widget::RemoveChild(child_widget);
  ptr_layout->AdjustChildren();
  Scroll(0);
  return up_removed;
}

void ScrollPanelWidget::ReplaceChild(Widget& old_child, std::unique_ptr<Widget> up_new_child) {
  old_child.Removed();
  up_new_child->ptr_parent = this;
  for (std::unique_ptr<Widget>& up_child : ChildrenOwned()) {
    if (up_child.get() == &old_child) {
      up_child = std::move(up_new_child);
      break;
    }
  }
  ptr_layout->AdjustChildren();
  Scroll(0);
}

void ScrollPanelWidget::SetListOffset(float fp4_value, bool b_smooth) {
  fp4_target_list_offset_ = fp4_value;
  if (!b_smooth) {
    const float fp4_old_list_offset = fp4_current_list_offset_;
    fp4_current_list_offset_ = fp4_value;

    if (fp4_old_list_offset != fp4_current_list_offset_)
      Ui::Instance().ResetTooltips();
  }
}

void ScrollPanelWidget::DrawOuter() {
  if (!fn_is_visible())
    return;

  UpdateSmoothScrolling();

  const Rectangle rect_rb = RenderBounds();
  const int int4_scrollbar_height = rect_rb.Height - 2 * int4_scrollbar_width;

  int int4_thumb_height = 0;
  int int4_thumb_origin = rect_rb.Y + int4_scrollbar_width;
  if (int4_content_height > rect_rb.Height) {
    int4_thumb_height = std::max(int4_minimum_thumb_size,
                                 int4_scrollbar_height * rect_rb.Height / int4_content_height);
    int4_thumb_origin += static_cast<int>(
        (int4_scrollbar_height - int4_thumb_height) * fp4_current_list_offset_ /
        static_cast<float>(rect_rb.Height - int4_content_height));
  }

  switch (kind_scrollbar) {
    case ScrollBar::Left:
      rect_background_ = Rectangle{rect_rb.X + int4_scrollbar_width, rect_rb.Y,
                                   rect_rb.Width + 1, rect_rb.Height};
      rect_up_button_ = Rectangle{rect_rb.X, rect_rb.Y, int4_scrollbar_width,
                                  int4_scrollbar_width};
      rect_down_button_ = Rectangle{rect_rb.X, rect_rb.Bottom() - int4_scrollbar_width,
                                    int4_scrollbar_width, int4_scrollbar_width};
      rect_scrollbar_ = Rectangle{rect_rb.X, rect_rb.Y + int4_scrollbar_width - 1,
                                  int4_scrollbar_width, int4_scrollbar_height + 2};
      rect_thumb_ = Rectangle{rect_rb.X, int4_thumb_origin, int4_scrollbar_width,
                              int4_thumb_height};
      break;
    case ScrollBar::Right:
      rect_background_ = Rectangle{rect_rb.X, rect_rb.Y, rect_rb.Width - int4_scrollbar_width + 1,
                                   rect_rb.Height};
      rect_up_button_ = Rectangle{rect_rb.Right() - int4_scrollbar_width, rect_rb.Y,
                                  int4_scrollbar_width, int4_scrollbar_width};
      rect_down_button_ = Rectangle{rect_rb.Right() - int4_scrollbar_width,
                                    rect_rb.Bottom() - int4_scrollbar_width,
                                    int4_scrollbar_width, int4_scrollbar_width};
      rect_scrollbar_ = Rectangle{rect_rb.Right() - int4_scrollbar_width,
                                  rect_rb.Y + int4_scrollbar_width - 1, int4_scrollbar_width,
                                  int4_scrollbar_height + 2};
      rect_thumb_ = Rectangle{rect_rb.Right() - int4_scrollbar_width, int4_thumb_origin,
                              int4_scrollbar_width, int4_thumb_height};
      break;
    case ScrollBar::Hidden:
      rect_background_ = Rectangle{rect_rb.X, rect_rb.Y, rect_rb.Width + 1, rect_rb.Height};
      break;
  }

  DrawPanel(str_background, rect_background_);

  if (kind_scrollbar != ScrollBar::Hidden) {
    const bool b_up_hover = Ui::Instance().MouseOverWidget() == this &&
                            rect_up_button_.Contains(gfx::Viewport::int2_last_mouse_pos);
    b_up_disabled_ = int4_thumb_height == 0 || fp4_current_list_offset_ >= 0;

    const bool b_down_hover = Ui::Instance().MouseOverWidget() == this &&
                              rect_down_button_.Contains(gfx::Viewport::int2_last_mouse_pos);
    b_down_disabled_ = int4_thumb_height == 0 ||
                       fp4_current_list_offset_ <=
                           static_cast<float>(bounds_widget.int4_height - int4_content_height);

    const bool b_thumb_hover = Ui::Instance().MouseOverWidget() == this &&
                               rect_thumb_.Contains(gfx::Viewport::int2_last_mouse_pos);
    DrawPanel(str_scrollbar_background, rect_scrollbar_);
    ButtonWidget::DrawBackground(str_button, rect_up_button_, b_up_disabled_, b_up_pressed_,
                                 b_up_hover, false);
    ButtonWidget::DrawBackground(str_button, rect_down_button_, b_down_disabled_,
                                 b_down_pressed_, b_down_hover, false);

    if (int4_thumb_height > 0)
      ButtonWidget::DrawBackground(str_button, rect_thumb_, false,
                                   HasMouseFocus() && b_thumb_hover, b_thumb_hover, false);

    const int int4_up_offset = !b_up_pressed_ || b_up_disabled_ ? 4 : 4 + int4_button_depth;
    const int int4_down_offset = !b_down_pressed_ || b_down_disabled_ ? 4 : 4 + int4_button_depth;

    const gfx::Sprite& sprite_up_arrow = cache_up_arrow_.Update(
        StatefulImageKey{b_up_disabled_, b_up_pressed_, b_up_hover, false, false});
    DrawSprite(sprite_up_arrow,
               core::Vector2{static_cast<float>(rect_up_button_.Left() + int4_up_offset),
                             static_cast<float>(rect_up_button_.Top() + int4_up_offset)});

    const gfx::Sprite& sprite_down_arrow = cache_down_arrow_.Update(
        StatefulImageKey{b_down_disabled_, b_down_pressed_, b_down_hover, false, false});
    DrawSprite(sprite_down_arrow,
               core::Vector2{static_cast<float>(rect_down_button_.Left() + int4_down_offset),
                             static_cast<float>(rect_down_button_.Top() + int4_down_offset)});
  }

  Rectangle rect_draw_bounds = InflateBy(rect_background_, -int4_border_width,
                                         -int4_border_width, -int4_border_width,
                                         -int4_border_width);
  RequireRenderer().EnableScissor(rect_draw_bounds);

  const int2 int2_child_origin = ChildOrigin();
  rect_draw_bounds = Rectangle{rect_draw_bounds.X - int2_child_origin.X,
                               rect_draw_bounds.Y - int2_child_origin.Y,
                               rect_draw_bounds.Width, rect_draw_bounds.Height};

  for (Widget* ptr_child : Children())
    if (ptr_child->bounds_widget.ToRectangle().IntersectsWith(rect_draw_bounds))
      ptr_child->DrawOuter();

  RequireRenderer().DisableScissor();
}

int2 ScrollPanelWidget::ChildOrigin() const {
  return RenderOrigin() +
         int2{kind_scrollbar == ScrollBar::Left ? int4_scrollbar_width : 0,
              static_cast<int>(fp4_current_list_offset_)};
}

bool ScrollPanelWidget::EventBoundsContains(int2 location) const {
  return EventBounds().Contains(location);
}

void ScrollPanelWidget::Scroll(int int4_amount, bool b_smooth) {
  float fp4_new_target = fp4_target_list_offset_ +
                         int4_amount * Ui::Instance().UiScrollSpeed();
  fp4_new_target = std::min(
      0.0f, std::max(static_cast<float>(bounds_widget.int4_height - int4_content_height),
                     fp4_new_target));

  SetListOffset(fp4_new_target, b_smooth);
}

void ScrollPanelWidget::ScrollToBottom(bool b_smooth) {
  const float fp4_value =
      kind_align == ScrollPanelAlign::Top
          ? std::min(0.0f,
                     static_cast<float>(bounds_widget.int4_height - int4_content_height))
          : static_cast<float>(bounds_widget.int4_height - int4_content_height);

  SetListOffset(fp4_value, b_smooth);
}

void ScrollPanelWidget::ScrollToTop(bool b_smooth) {
  const float fp4_value =
      kind_align == ScrollPanelAlign::Top
          ? 0.0f
          : std::max(0.0f,
                     static_cast<float>(bounds_widget.int4_height - int4_content_height));

  SetListOffset(fp4_value, b_smooth);
}

bool ScrollPanelWidget::ScrolledToBottom() const {
  return fp4_target_list_offset_ ==
             std::min(0.0f,
                      static_cast<float>(bounds_widget.int4_height - int4_content_height)) ||
         int4_content_height <= bounds_widget.int4_height;
}

void ScrollPanelWidget::ScrollToItemInner(Widget& item_widget, bool b_smooth) {

  std::optional<float> opt_new_offset;
  if (static_cast<float>(item_widget.bounds_widget.Top()) + fp4_current_list_offset_ < 0.0f)
    opt_new_offset =
        static_cast<float>(int4_item_spacing - item_widget.bounds_widget.Top());

  if (static_cast<float>(item_widget.bounds_widget.Bottom()) + fp4_current_list_offset_ >
      static_cast<float>(RenderBounds().Height))
    opt_new_offset = static_cast<float>(RenderBounds().Height -
                                        item_widget.bounds_widget.Bottom() -
                                        int4_item_spacing);

  if (opt_new_offset.has_value())
    SetListOffset(*opt_new_offset, b_smooth);
}

void ScrollPanelWidget::ScrollToItem(const std::string& str_item_key, bool b_smooth) {
  for (Widget* ptr_child : Children())
    if (auto* ptr_item = dynamic_cast<ScrollItemWidget*>(ptr_child);
        ptr_item != nullptr && ptr_item->str_item_key == str_item_key) {
      ScrollToItemInner(*ptr_child, b_smooth);
      return;
    }
}

void ScrollPanelWidget::ScrollToSelectedItem() {
  for (Widget* ptr_child : Children())
    if (auto* ptr_item = dynamic_cast<ScrollItemWidget*>(ptr_child);
        ptr_item != nullptr && ptr_item->fn_is_selected()) {
      ScrollToItemInner(*ptr_child);
      return;
    }
}

void ScrollPanelWidget::UpdateSmoothScrolling() {
  const std::int64_t int8_run_time = Ui::Instance().RunTime();
  if (int8_last_smooth_scroll_time_ == 0) {
    int8_last_smooth_scroll_time_ = int8_run_time;
    return;
  }

  const float fp4_offset_diff = fp4_target_list_offset_ - fp4_current_list_offset_;
  const float fp4_abs_offset_diff = std::abs(fp4_offset_diff);
  if (fp4_abs_offset_diff > 1.0f) {
    const float fp4_dt = static_cast<float>(int8_run_time - int8_last_smooth_scroll_time_);
    fp4_current_list_offset_ +=
        fp4_offset_diff * std::clamp(fp4_smooth_scroll_speed, 0.1f, 1.0f) * fp4_dt / 40.0f;

    Ui::Instance().ResetTooltips();
  } else
    SetListOffset(fp4_target_list_offset_, false);

  int8_last_smooth_scroll_time_ = int8_run_time;
}

void ScrollPanelWidget::Tick() {
  if (b_up_pressed_)
    Scroll(1);

  if (b_down_pressed_)
    Scroll(-1);
}

bool ScrollPanelWidget::YieldMouseFocus(const MouseInput& mi_input) {
  b_up_pressed_ = b_down_pressed_ = b_thumb_pressed_ = false;
  return Widget::YieldMouseFocus(mi_input);
}

bool ScrollPanelWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (mi_input.Event == MouseInputEvent::Scroll) {
    Scroll(mi_input.Delta.Y, true);
    return true;
  }

  if (mi_input.Button != MouseButton::Left)
    return false;

  if (mi_input.Event == MouseInputEvent::Down && !TakeMouseFocus(mi_input))
    return false;

  if (!HasMouseFocus())
    return false;

  if (mi_input.Event == MouseInputEvent::Up)
    return YieldMouseFocus(mi_input);

  if (b_thumb_pressed_ && mi_input.Event == MouseInputEvent::Move) {
    const Rectangle rect_rb = RenderBounds();
    const int int4_scrollbar_height = rect_rb.Height - 2 * int4_scrollbar_width;
    const int int4_thumb_height =
        int4_content_height == 0
            ? 0
            : std::max(
                  int4_minimum_thumb_size,
                  static_cast<int>(int4_scrollbar_height *
                                   std::min(static_cast<float>(rect_rb.Height) /
                                                static_cast<float>(int4_content_height),
                                            1.0f)));
    const float fp4_old_offset = fp4_current_list_offset_;

    int int4_new_offset = static_cast<int>(
        fp4_current_list_offset_ +
        static_cast<float>((int2_last_mouse_location_.Y - mi_input.Location.Y) *
                           (int4_content_height - rect_rb.Height)) /
            static_cast<float>(int4_scrollbar_height - int4_thumb_height));
    int4_new_offset = std::min(
        0, std::max(rect_rb.Height - int4_content_height, int4_new_offset));
    SetListOffset(static_cast<float>(int4_new_offset), false);

    if (fp4_old_offset != static_cast<float>(int4_new_offset))
      int2_last_mouse_location_ = mi_input.Location;
  } else {
    b_up_pressed_ = rect_up_button_.Contains(mi_input.Location);
    b_down_pressed_ = rect_down_button_.Contains(mi_input.Location);
    b_thumb_pressed_ = rect_thumb_.Contains(mi_input.Location);
    if (b_thumb_pressed_)
      int2_last_mouse_location_ = mi_input.Location;

    if (mi_input.Event == MouseInputEvent::Down) {
      if (b_thumb_pressed_ || (b_up_pressed_ && !b_up_disabled_) ||
          (b_down_pressed_ && !b_down_disabled_))
        Ui::Instance().PlayNotification(mod_rules, str_click_sound);
      else if ((b_up_pressed_ && b_up_disabled_) || (b_down_pressed_ && b_down_disabled_))
        Ui::Instance().PlayNotification(mod_rules, str_click_disabled_sound);
    }
  }

  return b_up_pressed_ || b_down_pressed_ || b_thumb_pressed_;
}

bool ScrollPanelWidget::LoadFieldOrProperty(std::string_view str_key,
                                            std::string_view str_value) {
  if (str_key == "ScrollbarWidth") {
    int4_scrollbar_width = meta::GetInt32Value("ScrollbarWidth", str_value);
    return true;
  }
  if (str_key == "BorderWidth") {
    int4_border_width = meta::GetInt32Value("BorderWidth", str_value);
    return true;
  }
  if (str_key == "TopBottomSpacing") {
    int4_top_bottom_spacing = meta::GetInt32Value("TopBottomSpacing", str_value);
    return true;
  }
  if (str_key == "ItemSpacing") {
    int4_item_spacing = meta::GetInt32Value("ItemSpacing", str_value);
    return true;
  }
  if (str_key == "ButtonDepth") {
    int4_button_depth = meta::GetInt32Value("ButtonDepth", str_value);
    return true;
  }
  if (str_key == "ClickSound") {
    str_click_sound = std::string{str_value};
    return true;
  }
  if (str_key == "ClickDisabledSound") {
    str_click_disabled_sound = std::string{str_value};
    return true;
  }
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }
  if (str_key == "ScrollBarBackground") {
    str_scrollbar_background = std::string{str_value};
    return true;
  }
  if (str_key == "Button") {
    str_button = std::string{str_value};
    return true;
  }
  if (str_key == "Decorations") {
    str_decorations = std::string{str_value};
    return true;
  }
  if (str_key == "MinimumThumbSize") {
    int4_minimum_thumb_size = meta::GetInt32Value("MinimumThumbSize", str_value);
    return true;
  }
  if (str_key == "Align") {
    const std::string str_value_owned{str_value};
    if (str_value_owned == "Bottom")
      kind_align = ScrollPanelAlign::Bottom;
    else if (str_value_owned == "Top")
      kind_align = ScrollPanelAlign::Top;
    else
      throw std::runtime_error(
          "FieldLoader: Cannot parse `" + str_value_owned +
          "` into field `Align` of type `OpenRA.Mods.Common.Widgets.ScrollPanelAlign`");
    return true;
  }
  if (str_key == "ScrollBar") {
    const std::string str_value_owned{str_value};
    if (str_value_owned == "Left")
      kind_scrollbar = ScrollBar::Left;
    else if (str_value_owned == "Right")
      kind_scrollbar = ScrollBar::Right;
    else if (str_value_owned == "Hidden")
      kind_scrollbar = ScrollBar::Hidden;
    else
      throw std::runtime_error(
          "FieldLoader: Cannot parse `" + str_value_owned +
          "` into field `ScrollBar` of type `OpenRA.Mods.Common.Widgets.ScrollBar`");
    return true;
  }
  if (str_key == "CollapseHiddenChildren") {
    b_collapse_hidden_children = meta::GetBoolValue("CollapseHiddenChildren", str_value);
    return true;
  }
  if (str_key == "SmoothScrollSpeed") {
    fp4_smooth_scroll_speed = meta::GetFloatValue("SmoothScrollSpeed", str_value);
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

}
