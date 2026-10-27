import std;

#include "ui/widgets/hotkey_entry_widget.hpp"

#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

HotkeyEntryWidget::HotkeyEntryWidget() {
  ChromeMetrics& metrics_chrome = RequireChromeMetrics();
  str_font = metrics_chrome.Get("HotkeyFont");
  color_text = metrics_chrome.GetColor("HotkeyColor");
  color_text_disabled = metrics_chrome.GetColor("HotkeyColorDisabled");
  color_text_invalid = metrics_chrome.GetColor("HotkeyColorInvalid");
}

HotkeyEntryWidget::HotkeyEntryWidget(const HotkeyEntryWidget& widget_other)
    : InputWidget(widget_other),
      int4_visual_height(widget_other.int4_visual_height),
      int4_left_margin(widget_other.int4_left_margin),
      int4_right_margin(widget_other.int4_right_margin),
      fn_on_esc_key(widget_other.fn_on_esc_key),
      fn_on_lose_focus(widget_other.fn_on_lose_focus),
      fn_is_valid(widget_other.fn_is_valid),
      str_font(widget_other.str_font),
      color_text(widget_other.color_text),
      color_text_disabled(widget_other.color_text_disabled),
      color_text_invalid(widget_other.color_text_invalid) {}

bool HotkeyEntryWidget::YieldKeyboardFocus() {
  fn_on_lose_focus();
  if (!fn_is_valid())
    return false;

  return InputWidget::YieldKeyboardFocus();
}

bool HotkeyEntryWidget::ForceYieldKeyboardFocus() {
  fn_on_lose_focus();
  return InputWidget::YieldKeyboardFocus();
}

bool HotkeyEntryWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (fn_is_disabled())
    return false;

  if (mi_input.Event != MouseInputEvent::Down)
    return false;

  if (!RenderBounds().Contains(mi_input.Location) || !TakeKeyboardFocus())
    return false;

  int4_blink_cycle_ = 15;

  return true;
}

bool HotkeyEntryWidget::HandleKeyPress(const KeyInput& e_input) {
  if (fn_is_disabled() || e_input.Event == KeyInputEvent::Up)
    return false;

  static constexpr Keycode kIgnoreKeys[] = {
      Keycode::RSHIFT, Keycode::LSHIFT, Keycode::RCTRL, Keycode::LCTRL,
      Keycode::RALT,   Keycode::LALT,   Keycode::RGUI,  Keycode::LGUI,
  };

  if (!HasKeyboardFocus())
    return false;
  for (const Keycode key_ignore : kIgnoreKeys)
    if (e_input.Key == key_ignore)
      return false;

  switch (e_input.Key) {
    case Keycode::ESCAPE:
      fn_on_esc_key(e_input);
      break;

    default:
      key_hotkey = Hotkey::FromKeyInput(e_input);
      break;
  }

  YieldKeyboardFocus();

  return true;
}

void HotkeyEntryWidget::Tick() {
  if (HasKeyboardFocus() && --int4_blink_cycle_ <= 0) {
    int4_blink_cycle_ = 15;
    b_show_entry_ ^= true;
  }
}

void HotkeyEntryWidget::Draw() {
  const std::string str_apparent = key_hotkey.DisplayString();

  gfx::SpriteFont& font_entry = RequireWidgetFont(str_font);
  const int2 int2_pos = RenderOrigin();

  const int2 int2_text_size = font_entry.Measure(str_apparent);

  const bool b_disabled = fn_is_disabled();
  const bool b_valid = fn_is_valid();
  const std::string str_state = GetStatefulImageName(
      "textfield", b_disabled, false, Ui::Instance().MouseOverWidget() == this,
      HasKeyboardFocus());

  DrawPanel(str_state, RenderBounds());

  if (HasKeyboardFocus() && !b_show_entry_)
    return;

  const int2 int2_text_pos =
      int2_pos + int2{int4_left_margin,
                      (bounds_widget.int4_height - int2_text_size.Y) / 2 -
                          int4_visual_height};

  const bool b_overflowing =
      int2_text_size.X > bounds_widget.int4_width - int4_left_margin - int4_right_margin;
  if (b_overflowing) {
    RequireRenderer().EnableScissor(
        Rectangle{int2_pos.X + int4_left_margin, int2_pos.Y,
                  bounds_widget.int4_width - int4_left_margin - int4_right_margin,
                  bounds_widget.Bottom()});
  }

  const core::Color color_draw = b_disabled  ? color_text_disabled
                                 : !b_valid  ? color_text_invalid
                                             : color_text;
  font_entry.DrawText(str_apparent, int2_text_pos.ToVector2(), color_draw);

  if (b_overflowing)
    RequireRenderer().DisableScissor();
}

bool HotkeyEntryWidget::LoadFieldOrProperty(std::string_view str_key,
                                            std::string_view str_value) {
  if (str_key == "VisualHeight") {
    int4_visual_height = meta::GetInt32Value(str_key, str_value);
    return true;
  }
  if (str_key == "LeftMargin") {
    int4_left_margin = meta::GetInt32Value(str_key, str_value);
    return true;
  }
  if (str_key == "RightMargin") {
    int4_right_margin = meta::GetInt32Value(str_key, str_value);
    return true;
  }
  if (str_key == "Font") {
    str_font = std::string{str_value};
    return true;
  }
  if (str_key == "TextColor") {
    color_text = meta::GetColorValue(str_key, str_value);
    return true;
  }
  if (str_key == "TextColorDisabled") {
    color_text_disabled = meta::GetColorValue(str_key, str_value);
    return true;
  }
  if (str_key == "TextColorInvalid") {
    color_text_invalid = meta::GetColorValue(str_key, str_value);
    return true;
  }

  return InputWidget::LoadFieldOrProperty(str_key, str_value);
}

}
