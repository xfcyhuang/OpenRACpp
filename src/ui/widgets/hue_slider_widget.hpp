// UPSTREAM: OpenRA.Mods.Common/Widgets/HueSliderWidget.cs @1f594e320 L18-75(全文)。
#pragma once
import std;

#include "gfx/sheet.hpp"
#include "ui/widgets/slider_widget.hpp"

namespace ora::ui {

class HueSliderWidget : public SliderWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "HueSliderWidget";

  HueSliderWidget() : SliderWidget() {}
  explicit HueSliderWidget(WidgetArgs& args_args) : SliderWidget(args_args) {}
  explicit HueSliderWidget(const HueSliderWidget& widget_other)
      : SliderWidget(widget_other) {}
  ~HueSliderWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new HueSliderWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Initialize(WidgetArgs& args_args) override;
  void Draw() override;
  void Removed() override;

 private:
  std::unique_ptr<gfx::Sheet> up_hue_sheet_;
  gfx::Sprite sprite_hue_;
  gfx::Sprite sprite_picker_;
};

}  // namespace ora::ui
