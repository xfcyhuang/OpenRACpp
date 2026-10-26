// UPSTREAM: OpenRA.Mods.Common/Widgets/HotkeyEntryWidget.cs @1f594e320 L19-162(全文;
// 键捕获 + 15-tick blink + 三色态)。
#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "platform/keycode.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class HotkeyEntryWidget : public InputWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "HotkeyEntryWidget";

  HotkeyEntryWidget();
  explicit HotkeyEntryWidget(const HotkeyEntryWidget& widget_other);
  ~HotkeyEntryWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new HotkeyEntryWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  Hotkey key_hotkey{};

  int int4_visual_height = 1;
  int int4_left_margin = 5;
  int int4_right_margin = 5;

  std::function<void(const KeyInput&)> fn_on_esc_key = [](const KeyInput&) {};
  std::function<void()> fn_on_lose_focus = [] {};

  std::function<bool()> fn_is_valid = [] { return false; };
  std::string str_font;
  core::Color color_text{};
  core::Color color_text_disabled{};
  core::Color color_text_invalid{};

  bool YieldKeyboardFocus() override;
  bool ForceYieldKeyboardFocus();

  bool HandleMouseInput(const MouseInput& mi_input) override;
  bool HandleKeyPress(const KeyInput& e_input) override;
  void Tick() override;
  void Draw() override;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

  bool ShowEntry() const { return b_show_entry_; }

 protected:
  int int4_blink_cycle_ = 15;
  bool b_show_entry_ = true;
};

}  // namespace ora::ui
