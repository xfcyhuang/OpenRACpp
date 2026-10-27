import std;

#include "ui/widgets/exponential_slider_widget.hpp"

#include "meta/field_loader.hpp"

namespace ora::ui {

namespace {

double Clamp01(double fp8_x) { return std::min(1.0, std::max(0.0, fp8_x)); }

}

float ExponentialSliderWidget::ValueFromPx(int int4_x) const {
  const Rectangle rect_rb = RenderBounds();
  const float fp4_t =
      (static_cast<float>(int4_x) - 0.5f * static_cast<float>(rect_rb.Height)) /
      static_cast<float>(rect_rb.Width - rect_rb.Height);
  double fp8_exp = 0.0;
  if (fp4_t > 0.0f)
    fp8_exp = Clamp01(fp8_exp_a * std::exp(fp8_exp_b * static_cast<double>(fp4_t)));
  return fp4_minimum_value +
         (fp4_maximum_value - fp4_minimum_value) * static_cast<float>(fp8_exp);
}

int ExponentialSliderWidget::PxFromValue(float fp4_x) const {
  const Rectangle rect_rb = RenderBounds();
  const float fp4_norm = (fp4_x - fp4_minimum_value) / (fp4_maximum_value - fp4_minimum_value);
  const float fp4_linear =
      static_cast<float>(Clamp01(std::log(static_cast<double>(fp4_norm) / fp8_exp_a) / fp8_exp_b));
  return static_cast<int>(
      0.5f * static_cast<float>(rect_rb.Height) +
      static_cast<float>(rect_rb.Width - rect_rb.Height) * fp4_linear);
}

bool ExponentialSliderWidget::LoadFieldOrProperty(std::string_view str_key,
                                                  std::string_view str_value) {
  if (str_key == "ExpA") {
    fp8_exp_a = static_cast<double>(meta::GetFloatValue("ExpA", str_value));
    return true;
  }
  if (str_key == "ExpB") {
    fp8_exp_b = static_cast<double>(meta::GetFloatValue("ExpB", str_value));
    return true;
  }

  return SliderWidget::LoadFieldOrProperty(str_key, str_value);
}

}
