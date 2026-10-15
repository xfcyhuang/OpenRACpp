import std;

#include "ui/widgets/scrollable_line_graph_widget.hpp"

#include "core/vector_n.hpp"
#include "game/mod_data.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/sprite_renderer.hpp"
#include "meta/field_loader.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

ScrollableLineGraphWidget::ScrollableLineGraphWidget() {
  ChromeMetrics& metrics = RequireChromeMetrics();
  color_background_dark = metrics.GetColor("TextContrastColorDark");
  color_background_light = metrics.GetColor("TextContrastColorLight");
  str_click_sound = metrics.Get("ClickSound");
  str_click_disabled_sound = metrics.Get("ClickDisabledSound");

  fn_get_value_format = [this] { return str_value_format; };
  fn_get_x_axis_value_format = [this] { return str_x_axis_value_format; };
  fn_get_y_axis_value_format = [this] { return str_y_axis_value_format; };
  fn_get_x_axis_size = [this] { return int4_x_axis_size; };
  fn_get_y_axis_size = [this] { return int4_y_axis_size; };
  fn_get_x_axis_label = [this] { return str_x_axis_label; };
  fn_get_y_axis_label = [this] { return str_y_axis_label; };
  fn_get_display_first_y_axis_value = [this] {
    return b_display_first_y_axis_value;
  };
  fn_get_label_font = [this] { return str_label_font; };
  fn_get_axis_font = [this] { return str_axis_font; };

  cache_left_arrow_ =
      GetCachedStatefulImage(str_scrollbar_decorations, str_decoration_scroll_left);
  cache_right_arrow_ =
      GetCachedStatefulImage(str_scrollbar_decorations, str_decoration_scroll_right);
}

ScrollableLineGraphWidget::ScrollableLineGraphWidget(WidgetArgs& args_args)
    : ScrollableLineGraphWidget() {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("modData"))
    if (const auto* ptr_mod_data = std::get_if<game::ModData*>(ptr_value))
      ptr_mod_rules_ = &(*ptr_mod_data)->DefaultRules();
}

ScrollableLineGraphWidget::ScrollableLineGraphWidget(
    const ScrollableLineGraphWidget& widget_other)
    : Widget(widget_other),
      fn_get_series(widget_other.fn_get_series),
      fn_get_value_format(widget_other.fn_get_value_format),
      fn_get_x_axis_value_format(widget_other.fn_get_x_axis_value_format),
      fn_get_y_axis_value_format(widget_other.fn_get_y_axis_value_format),
      fn_get_x_axis_size(widget_other.fn_get_x_axis_size),
      fn_get_y_axis_size(widget_other.fn_get_y_axis_size),
      fn_get_x_axis_label(widget_other.fn_get_x_axis_label),
      fn_get_y_axis_label(widget_other.fn_get_y_axis_label),
      fn_get_display_first_y_axis_value(
          widget_other.fn_get_display_first_y_axis_value),
      fn_get_label_font(widget_other.fn_get_label_font),
      fn_get_axis_font(widget_other.fn_get_axis_font),
      str_value_format(widget_other.str_value_format),
      str_x_axis_value_format(widget_other.str_x_axis_value_format),
      str_y_axis_value_format(widget_other.str_y_axis_value_format),
      int4_x_axis_size(widget_other.int4_x_axis_size),
      int4_y_axis_size(widget_other.int4_y_axis_size),
      int4_x_axis_ticks_per_label(widget_other.int4_x_axis_ticks_per_label),
      str_x_axis_label(widget_other.str_x_axis_label),
      str_y_axis_label(widget_other.str_y_axis_label),
      b_display_first_y_axis_value(widget_other.b_display_first_y_axis_value),
      str_label_font(widget_other.str_label_font),
      str_axis_font(widget_other.str_axis_font),
      color_background_dark(widget_other.color_background_dark),
      color_background_light(widget_other.color_background_light),
      str_click_sound(widget_other.str_click_sound),
      str_click_disabled_sound(widget_other.str_click_disabled_sound),
      int4_padding(widget_other.int4_padding),
      int4_scrollbar_height(widget_other.int4_scrollbar_height),
      str_scrollbar_background(widget_other.str_scrollbar_background),
      str_scrollbar_button(widget_other.str_scrollbar_button),
      ptr_mod_rules_(widget_other.ptr_mod_rules_) {
  cache_left_arrow_ =
      GetCachedStatefulImage(str_scrollbar_decorations, str_decoration_scroll_left);
  cache_right_arrow_ =
      GetCachedStatefulImage(str_scrollbar_decorations, str_decoration_scroll_right);
}

void ScrollableLineGraphWidget::Initialize(WidgetArgs& args_args) {
  Widget::Initialize(args_args);
}

bool ScrollableLineGraphWidget::LoadFieldOrProperty(std::string_view str_key,
                                                    std::string_view str_value) {
  if (str_key == "ValueFormat")
    str_value_format = str_value;
  else if (str_key == "XAxisValueFormat")
    str_x_axis_value_format = str_value;
  else if (str_key == "YAxisValueFormat")
    str_y_axis_value_format = str_value;
  else if (str_key == "XAxisSize")
    int4_x_axis_size = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "YAxisSize")
    int4_y_axis_size = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "XAxisTicksPerLabel")
    int4_x_axis_ticks_per_label = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "XAxisLabel")
    str_x_axis_label = str_value;
  else if (str_key == "YAxisLabel")
    str_y_axis_label = str_value;
  else if (str_key == "DisplayFirstYAxisValue")
    b_display_first_y_axis_value = str_value == "true";
  else if (str_key == "LabelFont")
    str_label_font = str_value;
  else if (str_key == "AxisFont")
    str_axis_font = str_value;
  else if (str_key == "BackgroundColorDark")
    color_background_dark = meta::GetColorValue(str_key, str_value);
  else if (str_key == "BackgroundColorLight")
    color_background_light = meta::GetColorValue(str_key, str_value);
  else if (str_key == "Padding")
    int4_padding = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "ScrollbarHeight")
    int4_scrollbar_height = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "ScrollbarBackground")
    str_scrollbar_background = str_value;
  else if (str_key == "ScrollbarButton")
    str_scrollbar_button = str_value;
  else if (str_key == "ScrollbarDecorations")
    str_scrollbar_decorations = str_value;
  else if (str_key == "MinimumThumbWidth")
    int4_minimum_thumb_width = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "SmoothScrollSpeed")
    fp4_smooth_scroll_speed = meta::GetFloatValue(str_key, str_value);
  else
    return Widget::LoadFieldOrProperty(str_key, str_value);
  return true;
}

void ScrollableLineGraphWidget::SetHorizontalOffset(float fp4_value, bool b_smooth) {
  fp4_target_horizontal_offset = fp4_value;
  if (!b_smooth) {
    fp4_horizontal_offset = fp4_value;
    Ui::Instance().ResetTooltips();
  }
}

void ScrollableLineGraphWidget::UpdateSmoothScrolling() {
  if (int8_last_smooth_scroll_time == 0) {
    int8_last_smooth_scroll_time = Ui::Instance().RunTime();
    return;
  }

  const std::int64_t int8_dt = Ui::Instance().RunTime() - int8_last_smooth_scroll_time;
  int8_last_smooth_scroll_time = Ui::Instance().RunTime();

  const float fp4_offset_diff = fp4_target_horizontal_offset - fp4_horizontal_offset;
  const float fp4_abs_offset_diff = std::abs(fp4_offset_diff);
  if (fp4_abs_offset_diff > 1.0f) {
    const float fp4_speed =
        std::max(0.01f, std::min(1.0f, fp4_smooth_scroll_speed * int8_dt / 40.0f));
    fp4_horizontal_offset += fp4_offset_diff * fp4_speed;
  } else
    fp4_horizontal_offset = fp4_target_horizontal_offset;
}

void ScrollableLineGraphWidget::ScrollBy(float fp4_amount, bool b_smooth) {
  const float fp4_new_target =
      fp4_target_horizontal_offset + fp4_amount * Ui::Instance().UiScrollSpeed();
  SetHorizontalOffset(fp4_new_target, b_smooth);
  b_auto_scroll_enabled = false;
  b_manually_scrolled = true;
}

void ScrollableLineGraphWidget::PlayClickSound() {
  if ((b_thumb_pressed && (!b_right_disabled || !b_left_disabled)) ||
      (b_right_pressed && !b_right_disabled) || (b_left_pressed && !b_left_disabled))
    Ui::Instance().PlayNotification(ptr_mod_rules_, str_click_sound);
  else if ((b_right_pressed && b_right_disabled) || (b_left_pressed && b_left_disabled))
    Ui::Instance().PlayNotification(ptr_mod_rules_, str_click_disabled_sound);
}

void ScrollableLineGraphWidget::Draw() {
  if (!fn_get_series || !fn_get_label_font)
    return;

  std::vector<ScrollableLineGraphSeries> vec_series = fn_get_series();
  if (vec_series.empty())
    return;

  const std::string str_font = fn_get_label_font();
  if (str_font.empty())
    return;

  UpdateSmoothScrolling();

  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  if (ptr_renderer == nullptr)
    return;
  gfx::RgbaColorRenderer& cr = ptr_renderer->UIRgbaColorRenderer();
  const Rectangle rect = RenderBounds();

  gfx::SpriteFont& font_label = RequireWidgetFont(str_font);
  gfx::SpriteFont& font_axis = RequireWidgetFont(fn_get_axis_font());

  const int int4_x_axis_size = fn_get_x_axis_size();
  const int int4_y_axis_size = fn_get_y_axis_size();

  const std::string str_x_axis_label = fn_get_x_axis_label();
  const int2 int2_x_axis_label_size = font_axis.Measure(str_x_axis_label);

  const int int4_x_axis_point_label_height = font_label.Measure("0").Y;

  const int int4_graph_bottom_offset = int4_padding * 2 +
                                       int2_x_axis_label_size.Y +
                                       int4_x_axis_point_label_height +
                                       int4_scrollbar_height;
  const int int4_height = rect.Height - (int4_graph_bottom_offset + int4_padding * 4);

  float fp4_max_value = 0.0f;
  for (const auto& series : vec_series)
    for (const float fp4_point : series.vec_points)
      fp4_max_value = std::max(fp4_max_value, fp4_point);

  std::string_view sv_longest_name;
  for (const auto& series : vec_series)
    if (series.str_key.size() > sv_longest_name.size())
      sv_longest_name = series.str_key;
  const std::string str_longest_name{sv_longest_name};

  const float fp4_scaled_max_value =
      std::max(std::ceil(fp4_max_value / 1000.0f) * 1000.0f, 5000.0f);
  const float fp4_scale = int4_height / fp4_scaled_max_value;

  const int int4_width_max_value = font_label
                                       .Measure(FormatCurrent(
                                           fn_get_y_axis_value_format(),
                                           fp4_scaled_max_value))
                                       .X;
  const int int4_width_longest_name = font_label.Measure(str_longest_name).X;

  const std::string str_y_axis_label = fn_get_y_axis_label();
  const int2 int2_y_axis_label_size = font_axis.Measure(str_y_axis_label);

  const int int4_width =
      rect.Width - (int4_padding * 10 + int4_width_max_value +
                    int4_width_longest_name + int2_y_axis_label_size.Y);

  std::size_t sz_point_count = 0;
  for (const auto& series : vec_series)
    sz_point_count = std::max(sz_point_count, series.vec_points.size());
  const int int4_point_count = static_cast<int>(sz_point_count);
  const int int4_total_data_width = int4_point_count * (int4_width / int4_x_axis_size);
  const float fp4_max_horizontal_offset =
      std::max(0.0f, static_cast<float>(int4_total_data_width - int4_width));

  fp4_target_horizontal_offset =
      std::max(-fp4_max_horizontal_offset,
               std::min(0.0f, fp4_target_horizontal_offset));
  fp4_horizontal_offset =
      std::max(-fp4_max_horizontal_offset,
               std::min(0.0f, fp4_horizontal_offset));

  const int int4_x_step = int4_width / int4_x_axis_size;
  const int int4_y_step = int4_height / int4_y_axis_size;

  const int int4_visible_start =
      std::max(0, static_cast<int>(std::floor(-fp4_horizontal_offset / int4_x_step)));
  const int int4_visible_end =
      std::min(int4_point_count, int4_visible_start + int4_x_axis_size + 1);

  const core::Vector2 vec_graph_origin{
      static_cast<float>(rect.Left() + int4_padding * 3 + int4_width_max_value +
                         int2_y_axis_label_size.Y),
      static_cast<float>(rect.Bottom() - int4_graph_bottom_offset)};
  const core::Vector2 vec_origin{static_cast<float>(rect.Left()),
                                 static_cast<float>(rect.Bottom())};

  int int4_key_offset = 0;

  std::stable_sort(vec_series.begin(), vec_series.end(),
                   [](const ScrollableLineGraphSeries& a,
                      const ScrollableLineGraphSeries& b) {
                     const float fp4_a = a.vec_points.empty() ? 0.0f : a.vec_points.back();
                     const float fp4_b = b.vec_points.empty() ? 0.0f : b.vec_points.back();
                     return fp4_a > fp4_b;
                   });

  const core::Color color_white = core::Color::FromArgb(255, 255, 255, 255);
  const Rectangle rect_graph_clip{
      static_cast<int>(vec_graph_origin.X),
      static_cast<int>(vec_graph_origin.Y - int4_height), int4_width, int4_height};
  ptr_renderer->EnableScissor(rect_graph_clip);

  for (const auto& series : vec_series) {
    if (series.vec_points.empty())
      continue;
    std::vector<core::Vector3> vec_visible_points;
    for (int int4_i = int4_visible_start;
         int4_i < std::min(int4_visible_end,
                           static_cast<int>(series.vec_points.size()));
         ++int4_i) {
      const float fp4_screen_x = int4_i * int4_x_step + fp4_horizontal_offset;
      const float fp4_screen_y = -series.vec_points[int4_i] * fp4_scale;
      vec_visible_points.push_back(core::Vector3{
          vec_graph_origin.X + fp4_screen_x, vec_graph_origin.Y + fp4_screen_y, 0.0f});
    }

    if (vec_visible_points.size() > 1)
      cr.DrawLine(vec_visible_points, 1.0f, series.color_c);
  }

  ptr_renderer->DisableScissor();

  for (const auto& series : vec_series) {
    const core::Color color_c = series.color_c;
    if (!series.vec_points.empty()) {
      std::vector<core::Vector3> vec_visible_points;
      for (int int4_i = int4_visible_start;
           int4_i < std::min(int4_visible_end,
                             static_cast<int>(series.vec_points.size()));
           ++int4_i) {
        const float fp4_screen_x = int4_i * int4_x_step + fp4_horizontal_offset;
        const float fp4_screen_y = -series.vec_points[int4_i] * fp4_scale;

        if (fp4_screen_x >= -int4_x_step && fp4_screen_x <= int4_width + int4_x_step)
          vec_visible_points.push_back(core::Vector3{
              vec_graph_origin.X + fp4_screen_x, vec_graph_origin.Y + fp4_screen_y, 0.0f});
      }

      if (!vec_visible_points.empty()) {
        const core::Vector3 vec_last_point = vec_visible_points.back();

        if (vec_last_point.X >= vec_graph_origin.X &&
            vec_last_point.X <= vec_graph_origin.X + int4_width) {
          const int int4_last_index =
              int4_visible_start + static_cast<int>(vec_visible_points.size()) - 1;
          const float fp4_last_value =
              series.vec_points[std::min(
                  int4_last_index, static_cast<int>(series.vec_points.size()) - 1)];
          if (fp4_last_value != 0.0f)
            font_label.DrawTextWithShadow(
                FormatCurrent(fn_get_value_format(), fp4_last_value),
                core::Vector2{vec_last_point.X, vec_last_point.Y - 2.0f}, color_c,
                color_background_dark, color_background_light, 1);
        }
      }
    }

    font_label.DrawTextWithShadow(
        series.str_key,
        core::Vector2{
            static_cast<float>(rect.Right() - (int4_width_longest_name + int4_padding)),
            static_cast<float>(rect.Top() + 10 * int4_key_offset + 3)},
        color_c, color_background_dark, color_background_light, 1);
    int4_key_offset++;
  }

  const int int4_scrollbar_y = rect.Bottom() - int4_scrollbar_height;
  const int int4_scrollbar_width = int4_width;
  const int int4_scrollbar_x = static_cast<int>(vec_graph_origin.X);

  rect_scrollbar_ = Rectangle{int4_scrollbar_x, int4_scrollbar_y, int4_scrollbar_width,
                              int4_scrollbar_height};
  rect_left_button_ = Rectangle{int4_scrollbar_x, int4_scrollbar_y, int4_scrollbar_height,
                                int4_scrollbar_height};
  rect_right_button_ =
      Rectangle{int4_scrollbar_x + int4_scrollbar_width - int4_scrollbar_height,
                int4_scrollbar_y, int4_scrollbar_height, int4_scrollbar_height};

  DrawPanel(str_scrollbar_background, rect_scrollbar_);

  const int int4_available_thumb_space = int4_scrollbar_width - 2 * int4_scrollbar_height;
  const int int4_thumb_width =
      fp4_max_horizontal_offset > 0
          ? std::max(static_cast<float>(int4_minimum_thumb_width),
                     std::min(static_cast<float>(int4_available_thumb_space -
                                                 int4_minimum_thumb_width * 2),
                              static_cast<float>(int4_available_thumb_space) *
                                  static_cast<float>(int4_width) /
                                  static_cast<float>(int4_total_data_width)))
          : static_cast<float>(int4_available_thumb_space);
  const int int4_actual_thumb_width =
      std::min(int4_thumb_width, int4_available_thumb_space);

  const float fp4_thumb_calculation_offset =
      b_auto_scroll_enabled && fp4_max_horizontal_offset > 0
          ? -fp4_max_horizontal_offset
          : fp4_horizontal_offset;
  const int int4_thumb_position =
      fp4_max_horizontal_offset > 0
          ? int4_scrollbar_height +
                static_cast<int>((int4_available_thumb_space - int4_actual_thumb_width) *
                                 (-fp4_thumb_calculation_offset / fp4_max_horizontal_offset))
          : int4_scrollbar_height;

  rect_thumb_ = Rectangle{int4_scrollbar_x + int4_thumb_position, int4_scrollbar_y,
                          int4_actual_thumb_width, int4_scrollbar_height};

  const bool b_mouse_button_down = b_left_pressed || b_thumb_pressed ||
                                   (b_right_pressed && !b_right_disabled);
  if (b_auto_scroll_enabled && fp4_max_horizontal_offset > 0 && !b_mouse_button_down)
    SetHorizontalOffset(-fp4_max_horizontal_offset, false);

  if (!b_auto_scroll_enabled && fp4_max_horizontal_offset > 0 &&
      !b_mouse_button_down && b_right_disabled && b_manually_scrolled) {
    if (b_right_pressed ||
        std::abs(fp4_target_horizontal_offset + fp4_max_horizontal_offset) < 1) {
      b_auto_scroll_enabled = true;
      b_manually_scrolled = false;
    }
  }

  const float fp4_effective_horizontal_offset =
      b_auto_scroll_enabled && fp4_max_horizontal_offset > 0
          ? -fp4_max_horizontal_offset
          : fp4_horizontal_offset;

  b_left_disabled = fp4_effective_horizontal_offset >= 0;
  b_right_disabled = fp4_effective_horizontal_offset <= -fp4_max_horizontal_offset;

  const bool b_left_hover = Ui::Instance().MouseOverWidget() == this &&
                            rect_left_button_.Contains(gfx::Viewport::int2_last_mouse_pos);
  const bool b_right_hover = Ui::Instance().MouseOverWidget() == this &&
                             rect_right_button_.Contains(gfx::Viewport::int2_last_mouse_pos);
  const bool b_thumb_hover = Ui::Instance().MouseOverWidget() == this &&
                             rect_thumb_.Contains(gfx::Viewport::int2_last_mouse_pos);

  ButtonWidget::DrawBackground(str_scrollbar_button, rect_left_button_,
                               b_left_disabled, b_left_pressed, b_left_hover, false);
  ButtonWidget::DrawBackground(str_scrollbar_button, rect_right_button_,
                               b_right_disabled, b_right_pressed, b_right_hover, false);

  if (fp4_max_horizontal_offset > 0)
    ButtonWidget::DrawBackground(str_scrollbar_button, rect_thumb_, false,
                                 b_thumb_pressed, b_thumb_hover, false);

  const int int4_left_offset = !b_left_pressed || b_left_disabled ? 0 : 1;
  const int int4_right_offset = !b_right_pressed || b_right_disabled ? 0 : 1;

  const gfx::Sprite sprite_left_arrow = cache_left_arrow_.Update(
      StatefulImageKey{b_left_disabled, b_left_pressed, b_left_hover, false, false});
  DrawSprite(sprite_left_arrow,
             core::Vector2{static_cast<float>(rect_left_button_.Left() + int4_left_offset),
                           static_cast<float>(rect_left_button_.Top() + int4_left_offset)});

  const gfx::Sprite sprite_right_arrow = cache_right_arrow_.Update(
      StatefulImageKey{b_right_disabled, b_right_pressed, b_right_hover, false, false});
  DrawSprite(sprite_right_arrow,
             core::Vector2{
                 static_cast<float>(rect_right_button_.Left() + int4_right_offset),
                 static_cast<float>(rect_right_button_.Top() + int4_right_offset)});

  font_axis.DrawTextWithShadow(
      str_x_axis_label,
      core::Vector2{
          vec_graph_origin.X + int4_width / 2 - int2_x_axis_label_size.X / 2,
          vec_origin.Y - (int2_x_axis_label_size.Y + int4_padding + int4_scrollbar_height)},
      color_white, color_background_dark, color_background_light, 1);

  const Rectangle rect_x_axis_clip{
      static_cast<int>(vec_graph_origin.X) - 100,
      static_cast<int>(vec_graph_origin.Y - int4_height), int4_width + 100,
      int4_height + int4_x_axis_point_label_height + 10};
  ptr_renderer->EnableScissor(rect_x_axis_clip);

  const int int4_max_data_points = int4_point_count;
  const int int4_labels_to_show =
      std::max(int4_x_axis_size + 2, int4_max_data_points);

  for (int int4_i = 0; int4_i < int4_labels_to_show; ++int4_i) {
    const float fp4_screen_x = int4_i * int4_x_step + fp4_horizontal_offset;

    if (fp4_screen_x >= 0) {
      cr.DrawLine(core::Vector2{vec_graph_origin.X + fp4_screen_x, vec_graph_origin.Y},
                  core::Vector2{vec_graph_origin.X + fp4_screen_x,
                                vec_graph_origin.Y - 5},
                  1.0f, color_white);
      if (int4_i % int4_x_axis_ticks_per_label == 0) {
        const std::string str_x_axis_text = FormatCurrent(
            fn_get_x_axis_value_format(),
            int4_i / int4_x_axis_ticks_per_label);
        const int int4_x_axis_tick_text_width =
            font_label.Measure(str_x_axis_text).X;
        const float fp4_x_location =
            fp4_screen_x - int4_x_axis_tick_text_width / 2.0f;
        font_label.DrawTextWithShadow(
            str_x_axis_text,
            core::Vector2{vec_graph_origin.X + fp4_x_location, vec_graph_origin.Y + 2},
            color_white, color_background_dark, color_background_light, 1);
      }
    }
  }

  ptr_renderer->DisableScissor();

  font_axis.DrawTextWithShadow(
      str_y_axis_label,
      core::Vector2{
          vec_origin.X + 5 - font_axis.TopOffset(),
          vec_graph_origin.Y - (int4_height / 2 - int2_y_axis_label_size.X / 2)},
      color_white, color_background_dark, color_background_light, 1,
      static_cast<float>(std::numbers::pi) / 2.0f);

  for (int int4_y = fn_get_display_first_y_axis_value() ? 0 : int4_y_step;
       int4_y <= int4_height; int4_y += int4_y_step) {
    const float fp4_y_value = int4_y / fp4_scale;
    cr.DrawLine(core::Vector2{vec_graph_origin.X, vec_graph_origin.Y - int4_y},
                core::Vector2{vec_graph_origin.X + 5, vec_graph_origin.Y - int4_y},
                1.0f, color_white);
    const std::string str_text =
        FormatCurrent(fn_get_y_axis_value_format(), fp4_y_value);

    const int2 int2_text_width = font_label.Measure(str_text);

    const int int4_y_location =
        int4_y + (int2_text_width.Y + font_label.TopOffset()) / 2;

    font_label.DrawTextWithShadow(
        str_text,
        core::Vector2{vec_graph_origin.X - (int2_text_width.X + 3),
                      vec_graph_origin.Y - int4_y_location},
        color_white, color_background_dark, color_background_light, 1);
  }

  cr.DrawLine(vec_graph_origin,
              core::Vector2{vec_graph_origin.X + int4_width, vec_graph_origin.Y},
              1.0f, color_white);
  cr.DrawLine(vec_graph_origin,
              core::Vector2{vec_graph_origin.X, vec_graph_origin.Y - int4_height},
              1.0f, color_white);
}

bool ScrollableLineGraphWidget::YieldMouseFocus(const MouseInput& mi_input) {
  b_left_pressed = b_right_pressed = b_thumb_pressed = false;
  return Widget::YieldMouseFocus(mi_input);
}

bool ScrollableLineGraphWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (mi_input.Event == MouseInputEvent::Scroll &&
      EventBoundsContains(mi_input.Location)) {
    ScrollBy(mi_input.Delta.Y, true);
    return true;
  }

  if (mi_input.Button != MouseButton::Left)
    return false;

  if (mi_input.Event == MouseInputEvent::Down && !TakeMouseFocus(mi_input))
    return false;

  if (!HasMouseFocus())
    return false;

  if (mi_input.Event == MouseInputEvent::Up) {
    b_left_pressed = b_right_pressed = b_thumb_pressed = false;
    YieldMouseFocus(mi_input);
    return true;
  }

  if (mi_input.Event == MouseInputEvent::Move && b_thumb_pressed) {
    const int int4_delta_x = mi_input.Location.X - int2_last_mouse_pos.X;
    std::vector<ScrollableLineGraphSeries> vec_series = fn_get_series();
    if (!vec_series.empty()) {
      std::size_t sz_point_count = 0;
      for (const auto& series : vec_series)
        sz_point_count = std::max(sz_point_count, series.vec_points.size());
      const int int4_point_count = static_cast<int>(sz_point_count);
      const Rectangle rect = RenderBounds();

      const std::string str_font = fn_get_label_font();
      if (str_font.empty())
        return false;

      gfx::SpriteFont& font_label = RequireWidgetFont(str_font);
      gfx::SpriteFont& font_axis = RequireWidgetFont(fn_get_axis_font());

      float fp4_max_value = 0.0f;
      for (const auto& series : vec_series)
        for (const float fp4_point : series.vec_points)
          fp4_max_value = std::max(fp4_max_value, fp4_point);

      std::string_view sv_longest_name;
      for (const auto& series : vec_series)
        if (series.str_key.size() > sv_longest_name.size())
          sv_longest_name = series.str_key;
      const std::string str_longest_name{sv_longest_name};
      const float fp4_scaled_max_value =
          std::max(std::ceil(fp4_max_value / 1000.0f) * 1000.0f, 5000.0f);

      const int int4_width_max_value = font_label
                                           .Measure(FormatCurrent(
                                               fn_get_y_axis_value_format(),
                                               fp4_scaled_max_value))
                                           .X;
      const int int4_width_longest_name = font_label.Measure(str_longest_name).X;
      const std::string str_y_axis_label = fn_get_y_axis_label();
      const int2 int2_y_axis_label_size = font_axis.Measure(str_y_axis_label);

      const int int4_width =
          rect.Width - (int4_padding * 10 + int4_width_max_value +
                        int4_width_longest_name + int2_y_axis_label_size.Y);

      const int int4_total_data_width =
          int4_point_count * (int4_width / fn_get_x_axis_size());
      const float fp4_max_horizontal_offset =
          std::max(0.0f, static_cast<float>(int4_total_data_width - int4_width));

      if (fp4_max_horizontal_offset > 0) {
        const int int4_available_thumb_space =
            rect_scrollbar_.Width - 2 * int4_scrollbar_height;
        const float fp4_proportional_thumb_size =
            static_cast<float>(int4_available_thumb_space) *
            static_cast<float>(int4_width) / static_cast<float>(int4_total_data_width);
        const float fp4_actual_thumb_size =
            std::max(static_cast<float>(int4_minimum_thumb_width),
                     std::min(fp4_proportional_thumb_size,
                              static_cast<float>(int4_available_thumb_space -
                                                 int4_minimum_thumb_width * 2)));
        const float fp4_thumb_range = int4_available_thumb_space - fp4_actual_thumb_size;

        if (fp4_thumb_range > 0) {
          const float fp4_scroll_amount =
              int4_delta_x / fp4_thumb_range * fp4_max_horizontal_offset;
          SetHorizontalOffset(fp4_target_horizontal_offset - fp4_scroll_amount, false);
          b_auto_scroll_enabled = false;
          b_manually_scrolled = true;
        }
      }
    }

    int2_last_mouse_pos = mi_input.Location;
    return true;
  }

  if (mi_input.Event == MouseInputEvent::Down) {
    if (rect_left_button_.Contains(mi_input.Location) && !b_left_disabled) {
      b_left_pressed = true;
      ScrollBy(1, true);
      PlayClickSound();
      return true;
    } else if (rect_right_button_.Contains(mi_input.Location) && !b_right_disabled) {
      b_right_pressed = true;
      ScrollBy(-1, true);
      PlayClickSound();
      return true;
    } else if (rect_thumb_.Contains(mi_input.Location)) {
      b_thumb_pressed = true;
      int2_last_mouse_pos = mi_input.Location;
      b_auto_scroll_enabled = false;
      b_manually_scrolled = true;
      PlayClickSound();
      return true;
    } else if (rect_scrollbar_.Contains(mi_input.Location)) {
      const int int4_click_x = mi_input.Location.X;
      const int int4_thumb_center_x =
          rect_thumb_.Left() + rect_thumb_.Width / 2;

      if (int4_click_x < int4_thumb_center_x)
        ScrollBy(2, true);
      else
        ScrollBy(-2, true);

      return true;
    }
  }

  return false;
}

void ScrollableLineGraphWidget::Tick() {
  if (b_left_pressed && !b_left_disabled)
    ScrollBy(1, true);

  if (b_right_pressed && !b_right_disabled)
    ScrollBy(-1, true);
}

}  // namespace ora::ui
