// UPSTREAM: OpenRA.Mods.Common/Widgets/ButtonWidget.cs @1f594e320 L20-298(全文)。
// 输入三态(Depressed/双击/键重复门)/键激活(HotkeyReference)/Tooltip 容器
// 懒查/GetTextPosition 三对齐/DrawBackground 静态九宫格。Game.Sound/
// Game.Renderer.Fonts/Ui.MouseOverWidget → Ui::Instance() 注入承载;
// ModData.DefaultRules → WidgetArgs "modData"。
// The whole of ButtonWidget.cs L20-298. The input trio (Depressed /
// double-click / the key-repeat gate) / key activation (HotkeyReference) /
// the tooltip container's lazy lookup / GetTextPosition's three alignments /
// the static nine-slice DrawBackground. Game.Sound and
// Game.Renderer.Fonts and Ui.MouseOverWidget go through the Ui::Instance()
// injected carrier; ModData.DefaultRules through WidgetArgs "modData".
#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::game {
class Ruleset;
}

namespace ora::ui {

class TooltipContainerWidget;

class ButtonWidget : public InputWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ButtonWidget";

  explicit ButtonWidget(WidgetArgs& args_args);
  explicit ButtonWidget(const ButtonWidget& widget_other);
  ~ButtonWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ButtonWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_tooltip_container;  // 空 = C# null | null
  std::string str_tooltip_template = "BUTTON_TOOLTIP";

  HotkeyReference key_hotkey;
  bool b_disable_key_repeat = false;
  bool b_disable_key_sound = false;

  std::string str_text = "";
  TextAlign kind_align = TextAlign::Center;
  int int4_left_margin = 5;
  int int4_right_margin = 5;
  std::string str_background = "button";
  bool b_depressed = false;
  int int4_visual_height = 0;
  std::string str_font;
  core::Color color_text{};
  core::Color color_text_disabled{};
  bool b_contrast = false;
  bool b_shadow = false;
  core::Color color_contrast_dark{};
  core::Color color_contrast_light{};
  int int4_contrast_radius = 0;
  std::string str_click_sound;  // 空 = C# null | null
  std::string str_click_disabled_sound;
  bool b_highlighted = false;
  std::function<std::string()> fn_get_text;
  std::function<core::Color()> fn_get_color;
  std::function<core::Color()> fn_get_color_disabled;
  std::function<core::Color()> fn_get_contrast_color_dark;
  std::function<core::Color()> fn_get_contrast_color_light;
  std::function<bool()> fn_is_highlighted;
  std::function<void(const MouseInput&)> fn_on_mouse_down = [](const MouseInput&) {};
  std::function<void(const MouseInput&)> fn_on_mouse_up = [](const MouseInput&) {};

  std::optional<std::string> opt_tooltip_text;
  std::function<std::string()> fn_get_tooltip_text;

  std::optional<std::string> opt_tooltip_desc;
  std::function<std::string()> fn_get_tooltip_desc;

  std::function<void()> fn_on_click = [] {};
  std::function<void()> fn_on_double_click;  // 空 = C# null | null
  std::function<void(const KeyInput&)> fn_on_key_press = [](const KeyInput&) {};

  std::string str_cursor;

  const game::Ruleset* mod_rules = nullptr;  // 非拥有 | non-owning

  bool YieldMouseFocus(const MouseInput& mi_input) override;
  bool HandleKeyPress(const KeyInput& e_input) override;
  bool HandleMouseInput(const MouseInput& mi_input) override;
  void MouseEntered() override;
  void MouseExited() override;

  const std::string* GetCursor(int2) const override { return &str_cursor; }

  int2 ChildOrigin() const override;

  void Draw() override;

  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

  virtual int UsableWidth() const { return bounds_widget.int4_width; }

  virtual void DrawBackground(Rectangle rect_bounds, bool b_disabled, bool b_pressed,
                              bool b_hover, bool b_highlighted);

  /// DrawBackground 静态双形态(L282-291)。
  /// DrawBackground's static pair (L282-291).
  static void DrawBackground(std::string_view str_base_name, Rectangle rect_bounds,
                             bool b_disabled, bool b_pressed, bool b_hover,
                             bool b_highlighted);

 protected:
  TooltipContainerWidget* TooltipContainerOrNull();

 private:
  int2 GetTextPosition(const std::string& str_text, gfx::SpriteFont& font_text,
                       Rectangle rect_bounds);
  TooltipContainerWidget* ResolveTooltipContainer();
  void BindDefaults();

  /// ctor 局部 CachedTransform 的持有化(C# 闭包捕获的等价;clone 重建)
  /// the ctor-local CachedTransforms held as members (the C# closure-capture
  /// equivalent; clones rebuild them).
  std::unique_ptr<core::CachedTransform<std::string, std::string>> cache_text_;
  std::unique_ptr<core::CachedTransform<std::string, std::string>> cache_tooltip_text_;
  std::unique_ptr<core::CachedTransform<std::string, std::string>> cache_tooltip_desc_;

  bool b_tooltip_resolved_ = false;
  TooltipContainerWidget* ptr_tooltip_container_cached_ = nullptr;
};

}  // namespace ora::ui
