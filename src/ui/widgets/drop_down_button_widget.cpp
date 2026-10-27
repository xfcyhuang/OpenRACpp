import std;

#include "ui/widgets/drop_down_button_widget.hpp"

#include "core/vector_n.hpp"
#include "meta/field_loader.hpp"
#include "ui/widget_loader.hpp"

namespace ora::ui {

DropDownButtonWidget::DropDownButtonWidget(const DropDownButtonWidget& widget_other)
    : ButtonWidget(widget_other),
      str_decorations(widget_other.str_decorations),
      str_decoration_marker(widget_other.str_decoration_marker),
      str_separators(widget_other.str_separators),
      str_separator_image(widget_other.str_separator_image),
      str_panel_root(widget_other.str_panel_root) {}

bool DropDownButtonWidget::HandleKeyPress(const KeyInput& e_input) {
  if (HasKeyboardFocus() && e_input.Event == KeyInputEvent::Down &&
      e_input.Key == Keycode::ESCAPE) {
    RemovePanel();
    return true;
  }

  return ButtonWidget::HandleKeyPress(e_input);
}

bool DropDownButtonWidget::YieldKeyboardFocus() {
  RemovePanel();
  return ButtonWidget::YieldKeyboardFocus();
}

void DropDownButtonWidget::Draw() {
  ButtonWidget::Draw();
  const int2 int2_state_offset =
      b_depressed ? int2{int4_visual_height, int4_visual_height} : int2{0, 0};

  const Rectangle rect_rb = RenderBounds();
  const bool b_is_disabled = fn_is_disabled();
  bool b_is_hover = Ui::Instance().MouseOverWidget() == this;
  if (!b_is_hover)
    for (Widget* ptr_child : Children())
      if (ptr_child == Ui::Instance().MouseOverWidget()) {
        b_is_hover = true;
        break;
      }

  if (!cache_marker_image_)
    cache_marker_image_ = GetCachedStatefulImage(str_decorations, str_decoration_marker);

  const gfx::Sprite sprite_arrow = cache_marker_image_->Update(
      {b_is_disabled, b_depressed, b_is_hover, false, fn_is_highlighted()});
  const float fp4_offset_x = static_cast<float>(int2_state_offset.X);
  const float fp4_offset_y = static_cast<float>(int2_state_offset.Y);
  DrawSprite(sprite_arrow,
             core::Vector2{
                 fp4_offset_x +
                     static_cast<float>(rect_rb.Right() -
                                        static_cast<int>((rect_rb.Height +
                                                          sprite_arrow.vec_size.X) /
                                                         2)),
                 fp4_offset_y +
                     static_cast<float>(
                         rect_rb.Top() +
                         static_cast<int>((rect_rb.Height - sprite_arrow.vec_size.Y) / 2))});

  if (!cache_separator_image_)
    cache_separator_image_ = GetCachedStatefulImage(str_separators, str_separator_image);

  const gfx::Sprite sprite_separator = cache_separator_image_->Update(
      {b_is_disabled, b_depressed, b_is_hover, false, fn_is_highlighted()});
  if (sprite_separator.ptr_sheet != nullptr)
    DrawSprite(sprite_separator,
               core::Vector2{
                   fp4_offset_x - 3.0f +
                       static_cast<float>(rect_rb.Right() - rect_rb.Height + 4),
                   fp4_offset_y +
                       static_cast<float>(
                           rect_rb.Top() + static_cast<int>((rect_rb.Height -
                                                             sprite_separator.vec_size.Y) /
                                                            2))});
}

void DropDownButtonWidget::Hidden() {
  ButtonWidget::Hidden();
  RemovePanel();
}

void DropDownButtonWidget::Removed() {
  ButtonWidget::Removed();
  RemovePanel();
}

void DropDownButtonWidget::RemovePanel() {
  if (ptr_panel_ == nullptr)
    return;

  ptr_panel_root_->RemoveChild(*ptr_fullscreen_mask_);
  ptr_panel_root_->RemoveChild(*ptr_panel_);
  ptr_panel_ = ptr_fullscreen_mask_ = nullptr;

  YieldKeyboardFocus();
  Ui::Instance().ResetTooltips();
}

void DropDownButtonWidget::AttachPanel(std::unique_ptr<Widget> up_panel) {
  AttachPanel(std::move(up_panel), nullptr);
}

void DropDownButtonWidget::AttachPanel(std::unique_ptr<Widget> up_panel,
                                       std::function<void()> fn_on_cancel) {
  if (ptr_panel_ != nullptr)
    throw std::runtime_error("Attempted to attach a panel to an open dropdown");
  ptr_panel_ = up_panel.get();
  TakeKeyboardFocus();

  auto up_mask = std::make_unique<MaskWidget>();
  const int2 int2_resolution = Ui::Instance().Resolution();
  up_mask->bounds_widget = WidgetBounds{0, 0, int2_resolution.X, int2_resolution.Y};

  up_mask->vec_on_mouse_down.push_back([this](const MouseInput&) {
    Ui::Instance().PlayNotification(mod_rules, str_click_sound);
    RemovePanel();
  });
  if (fn_on_cancel)
    up_mask->vec_on_mouse_down.push_back(
        [fn_on_cancel = std::move(fn_on_cancel)](const MouseInput&) { fn_on_cancel(); });
  ptr_fullscreen_mask_ = up_mask.get();

  ptr_panel_root_ = str_panel_root.empty()
                        ? Ui::Instance().Root()
                        : Ui::Instance().Root()->Get(str_panel_root);

  ptr_panel_root_->AddChild(std::move(up_mask));

  const WidgetBounds rect_old_bounds = up_panel->bounds_widget;
  int int4_panel_x = RenderOrigin().X - ptr_panel_root_->RenderOrigin().X;
  if (kind_panel_align == TextAlign::Right)
    int4_panel_x += bounds_widget.int4_width - rect_old_bounds.int4_width;
  else if (kind_panel_align == TextAlign::Center)
    int4_panel_x += (bounds_widget.int4_width - rect_old_bounds.int4_width) / 2;

  int int4_panel_y =
      RenderOrigin().Y + bounds_widget.int4_height - ptr_panel_root_->RenderOrigin().Y;
  if (int4_panel_y + rect_old_bounds.int4_height > int2_resolution.Y)
    int4_panel_y -= bounds_widget.int4_height + rect_old_bounds.int4_height;

  const int int4_button_right_edge =
      RenderOrigin().X + bounds_widget.int4_width - ptr_panel_root_->RenderOrigin().X;
  if (int4_panel_x + rect_old_bounds.int4_width > int2_resolution.X)
    int4_panel_x = int4_button_right_edge - rect_old_bounds.int4_width;

  up_panel->bounds_widget =
      WidgetBounds{int4_panel_x, int4_panel_y, rect_old_bounds.int4_width,
                   rect_old_bounds.int4_height};
  ptr_panel_root_->AddChild(std::move(up_panel));

  if (auto* ptr_scroll_panel = dynamic_cast<ScrollPanelWidget*>(ptr_panel_))
    ptr_scroll_panel->ScrollToSelectedItem();
}

bool DropDownButtonWidget::LoadFieldOrProperty(std::string_view str_key,
                                               std::string_view str_value) {
  if (str_key == "Decorations") {
    str_decorations = std::string{str_value};
    return true;
  }
  if (str_key == "DecorationMarker") {
    str_decoration_marker = std::string{str_value};
    return true;
  }
  if (str_key == "Separators") {
    str_separators = std::string{str_value};
    return true;
  }
  if (str_key == "SeparatorImage") {
    str_separator_image = std::string{str_value};
    return true;
  }
  if (str_key == "PanelAlign") {
    const std::string str_value_owned{str_value};
    if (str_value_owned == "Left")
      kind_panel_align = TextAlign::Left;
    else if (str_value_owned == "Center")
      kind_panel_align = TextAlign::Center;
    else if (str_value_owned == "Right")
      kind_panel_align = TextAlign::Right;
    else
      throw std::runtime_error(
          "FieldLoader: Cannot parse `" + str_value_owned +
          "` into field `PanelAlign` of type `OpenRA.Mods.Common.Widgets.TextAlign`");
    return true;
  }
  if (str_key == "PanelRoot") {
    str_panel_root = std::string{str_value};
    return true;
  }

  return ButtonWidget::LoadFieldOrProperty(str_key, str_value);
}

bool MaskWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (mi_input.Event == MouseInputEvent::Move)
    return false;

  if (mi_input.Event == MouseInputEvent::Down) {

    std::vector<std::function<void(const MouseInput&)>> vec_handlers = vec_on_mouse_down;
    for (const auto& fn_handler : vec_handlers)
      fn_handler(mi_input);
  }

  return true;
}

}
