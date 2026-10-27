#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "ui/widget.hpp"
#include "ui/widget_utils.hpp"

namespace ora::game {
class Ruleset;
}

namespace ora::ui {

struct ScrollableLineGraphSeries {
  std::string str_key;
  core::Color color_c{};
  std::vector<float> vec_points;
};

class ScrollableLineGraphWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ScrollableLineGraphWidget";

  ScrollableLineGraphWidget();
  explicit ScrollableLineGraphWidget(WidgetArgs& args_args);
  explicit ScrollableLineGraphWidget(const ScrollableLineGraphWidget& widget_other);
  ~ScrollableLineGraphWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ScrollableLineGraphWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Initialize(WidgetArgs& args_args) override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
  void Draw() override;
  void Tick() override;
  bool YieldMouseFocus(const MouseInput& mi_input) override;
  bool HandleMouseInput(const MouseInput& mi_input) override;

  std::function<std::vector<ScrollableLineGraphSeries>()> fn_get_series;
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
  std::string str_click_sound;
  std::string str_click_disabled_sound;
  int int4_padding = 5;

  int int4_scrollbar_height = 16;
  std::string str_scrollbar_background = "scrollpanel-bg";
  std::string str_scrollbar_button = "scrollpanel-button";
  std::string str_scrollbar_decorations = "scrollpanel-decorations";
  const std::string str_decoration_scroll_left = "left";
  const std::string str_decoration_scroll_right = "right";
  int int4_minimum_thumb_width = 20;
  float fp4_smooth_scroll_speed = 0.333f;

 private:
  void SetHorizontalOffset(float fp4_value, bool b_smooth);
  void UpdateSmoothScrolling();
  void ScrollBy(float fp4_amount, bool b_smooth = true);
  void PlayClickSound();

  game::Ruleset* ptr_mod_rules_ = nullptr;
  StatefulImageCache cache_left_arrow_;
  StatefulImageCache cache_right_arrow_;

  float fp4_horizontal_offset = 0.0f;
  float fp4_target_horizontal_offset = 0.0f;
  bool b_left_pressed = false;
  bool b_right_pressed = false;
  bool b_thumb_pressed = false;
  bool b_left_disabled = false;
  bool b_right_disabled = false;
  bool b_auto_scroll_enabled = true;
  bool b_manually_scrolled = true;
  int2 int2_last_mouse_pos{};
  Rectangle rect_left_button_{};
  Rectangle rect_right_button_{};
  Rectangle rect_scrollbar_{};
  Rectangle rect_thumb_{};
  std::int64_t int8_last_smooth_scroll_time = 0;
};

}
