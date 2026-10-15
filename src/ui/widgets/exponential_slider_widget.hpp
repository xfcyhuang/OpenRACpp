// UPSTREAM: OpenRA.Mods.Common/Widgets/ExponentialSliderWidget.cs @e180e3225 L17-50(全文)。
#pragma once
import std;

#include "ui/widgets/slider_widget.hpp"

namespace ora::ui {

class ExponentialSliderWidget : public SliderWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ExponentialSliderWidget";

  ExponentialSliderWidget() : SliderWidget() {}
  explicit ExponentialSliderWidget(WidgetArgs& args_args) : SliderWidget(args_args) {}
  explicit ExponentialSliderWidget(const ExponentialSliderWidget& widget_other)
      : SliderWidget(widget_other),
        fp8_exp_a(widget_other.fp8_exp_a),
        fp8_exp_b(widget_other.fp8_exp_b) {}
  ~ExponentialSliderWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ExponentialSliderWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  double fp8_exp_a = 1.0e-3;
  double fp8_exp_b = 6.908;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 protected:
  float ValueFromPx(int int4_x) const override;
  int PxFromValue(float fp4_x) const override;
};

}  // namespace ora::ui
