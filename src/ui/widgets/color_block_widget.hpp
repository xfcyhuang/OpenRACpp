// UPSTREAM: OpenRA.Mods.Common/Widgets/ColorBlockWidget.cs @1f594e320 L18-80(全文)
// + GradientColorBlockWidget.cs L18-57(全文)。ColorBlock 的输入三态
// (Down 持焦/Up 让焦触发)与 ClickSound 通知;Gradient 的四角色 FillRect。
// ModData.DefaultRules → WidgetArgs "modData";Game.Sound → Ui 注入。
// The whole of ColorBlockWidget.cs L18-80 + GradientColorBlockWidget.cs
// L18-57. ColorBlock's input trio (Down taking focus / Up yielding focus to
// fire) with the ClickSound notification; Gradient's four-color FillRect.
// ModData.DefaultRules through WidgetArgs "modData"; Game.Sound through the
// Ui injection.
#pragma once
import std;

#include "core/color.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class Ruleset;
}

namespace ora::ui {

class ColorBlockWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ColorBlockWidget";

  explicit ColorBlockWidget(WidgetArgs& args_args);
  explicit ColorBlockWidget(const ColorBlockWidget& widget_other);
  ~ColorBlockWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ColorBlockWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  core::Color color_value{};
  std::function<core::Color()> fn_get_color;
  std::function<void(const MouseInput&)> fn_on_mouse_down = [](const MouseInput&) {};
  std::function<void(const MouseInput&)> fn_on_mouse_up = [](const MouseInput&) {};
  std::string str_click_sound;  // 空 = C# null | null

  const game::Ruleset* mod_rules = nullptr;

  void Draw() override;
  bool HandleMouseInput(const MouseInput& mi_input) override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
};

class GradientColorBlockWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "GradientColorBlockWidget";

  GradientColorBlockWidget();
  explicit GradientColorBlockWidget(const GradientColorBlockWidget& widget_other);
  ~GradientColorBlockWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new GradientColorBlockWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  core::Color color_top_left{};
  core::Color color_top_right{};
  core::Color color_bottom_right{};
  core::Color color_bottom_left{};

  std::function<core::Color()> fn_get_top_left_color;
  std::function<core::Color()> fn_get_top_right_color;
  std::function<core::Color()> fn_get_bottom_right_color;
  std::function<core::Color()> fn_get_bottom_left_color;

  void Draw() override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
};

}  // namespace ora::ui
