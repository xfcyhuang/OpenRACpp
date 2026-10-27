#pragma once
import std;

#include "core/color.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

struct LineGraphSeries {
  std::string str_key;
  core::Color color_c{};
  std::vector<float> vec_points;
};

class LineGraphWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "LineGraphWidget";

  LineGraphWidget();
  explicit LineGraphWidget(WidgetArgs& args_args) : LineGraphWidget() {
    (void)args_args;
  }
  explicit LineGraphWidget(const LineGraphWidget& widget_other);
  ~LineGraphWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new LineGraphWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::function<std::vector<LineGraphSeries>()> fn_get_series;
  std::function<std::string()> fn_get_value_format;
  std::function<std::string()> fn_get_x_axis_value_format;
  std::function<std::string()> fn_get_y_axis_value_format;
  std::function<int()> fn_get_x_axis_size;
  std::function<int()> fn_get_y_axis_size;
  std::function<std::string()> fn_get_x_axis_label;
  std::function<std::string()> fn_get_y_axis_label;
  std::function<bool()> fn_get_display_first_y_axis_value;
  std::function<std::string()> fn_get_label_font;
  std::function<std::string()> fn_get_axis_font;

  std::string str_value_format = "{0}";
  std::string str_x_axis_value_format = "{0}";
  std::string str_y_axis_value_format = "{0}";
  int int4_x_axis_size = 10;
  int int4_y_axis_size = 10;
  int int4_x_axis_ticks_per_label = 1;
  std::string str_x_axis_label;
  std::string str_y_axis_label;
  bool b_display_first_y_axis_value = false;
  std::string str_label_font;
  std::string str_axis_font;
  core::Color color_background_dark{};
  core::Color color_background_light{};
  int int4_padding = 5;

  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
  void Draw() override;

 private:
  void BindDefaultDelegates();
};

}
