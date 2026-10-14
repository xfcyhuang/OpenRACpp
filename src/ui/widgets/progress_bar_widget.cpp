// UPSTREAM: OpenRA.Mods.Common/Widgets/ProgressBarWidget.cs @e180e3225
// (progress_bar_widget.hpp 的实现)
// The implementation of progress_bar_widget.hpp.
import std;

#include "ui/widgets/progress_bar_widget.hpp"

#include "gfx/chrome_provider.hpp"
#include "meta/field_loader.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

ProgressBarWidget::ProgressBarWidget() {
  fn_get_percentage = [this] { return int4_percentage; };
  fn_is_indeterminate = [this] { return b_indeterminate; };
}

ProgressBarWidget::ProgressBarWidget(const ProgressBarWidget& widget_other)
    : Widget(widget_other),
      str_background(widget_other.str_background),
      str_bar(widget_other.str_bar),
      int2_bar_margin(widget_other.int2_bar_margin),
      int4_percentage(widget_other.int4_percentage),
      fn_get_percentage(widget_other.fn_get_percentage),
      fn_is_indeterminate(widget_other.fn_is_indeterminate) {}

void ProgressBarWidget::Draw() {
  const Rectangle rect_rb = RenderBounds();
  const int int4_percentage_value = fn_get_percentage();
  DrawPanel(str_background, rect_rb);

  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
  const int int4_min_bar_width =
      ptr_chrome != nullptr ? ptr_chrome->GetMinimumPanelSize(str_bar).X : 0;
  const int int4_max_bar_width = rect_rb.Width - int2_bar_margin.X * 2;
  int int4_bar_width = b_was_indeterminate_
                           ? int4_max_bar_width / 4
                           : int4_percentage_value * int4_max_bar_width / 100;
  int4_bar_width = std::max(int4_bar_width, int4_min_bar_width);

  const int int4_bar_offset =
      b_was_indeterminate_ ? static_cast<int>(0.75 * fp4_offset_ * int4_max_bar_width) : 0;
  const Rectangle rect_bar{rect_rb.X + int2_bar_margin.X + int4_bar_offset,
                           rect_rb.Y + int2_bar_margin.Y, int4_bar_width,
                           rect_rb.Height - 2 * int2_bar_margin.Y};
  DrawPanel(str_bar, rect_bar);
}

void ProgressBarWidget::Tick() {
  const bool b_indeterminate_value = fn_is_indeterminate();
  if (b_indeterminate_value != b_was_indeterminate_)
    fp4_offset_ = 0.0f;

  if (b_indeterminate_value) {
    fp4_offset_ += fp4_tick_step_;
    fp4_offset_ = std::clamp(fp4_offset_, 0.0f, 1.0f);
    if (fp4_offset_ == 0.0f || fp4_offset_ == 1.0f)
      fp4_tick_step_ *= -1.0f;
  }

  b_was_indeterminate_ = b_indeterminate_value;
}

bool ProgressBarWidget::LoadFieldOrProperty(std::string_view str_key,
                                            std::string_view str_value) {
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }
  if (str_key == "Bar") {
    str_bar = std::string{str_value};
    return true;
  }
  if (str_key == "BarMargin") {
    const std::string str_value_owned{str_value};
    const std::size_t int8_comma = str_value_owned.find(',');
    if (int8_comma != std::string::npos) {
      int2_bar_margin = int2{std::stoi(str_value_owned.substr(0, int8_comma)),
                             std::stoi(str_value_owned.substr(int8_comma + 1))};
      return true;
    }
    throw std::runtime_error("FieldLoader: Cannot parse `" + str_value_owned +
                             "` into field `BarMargin` of type `OpenRA.Primitives.Size`");
  }
  if (str_key == "Percentage") {
    int4_percentage = meta::GetInt32Value("Percentage", str_value);
    return true;
  }
  if (str_key == "Indeterminate") {
    b_indeterminate = meta::GetBoolValue("Indeterminate", str_value);
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
