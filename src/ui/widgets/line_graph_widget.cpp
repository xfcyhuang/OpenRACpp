import std;

#include "ui/widgets/line_graph_widget.hpp"

#include "core/vector_n.hpp"
#include "meta/field_loader.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/sprite_renderer.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

LineGraphWidget::LineGraphWidget() {
  ChromeMetrics& metrics = RequireChromeMetrics();
  color_background_dark = metrics.GetColor("TextContrastColorDark");
  color_background_light = metrics.GetColor("TextContrastColorLight");
  BindDefaultDelegates();
}

LineGraphWidget::LineGraphWidget(const LineGraphWidget& widget_other)
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
      int4_padding(widget_other.int4_padding) {}

void LineGraphWidget::BindDefaultDelegates() {
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
}

bool LineGraphWidget::LoadFieldOrProperty(std::string_view str_key,
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
  else
    return Widget::LoadFieldOrProperty(str_key, str_value);
  return true;
}

void LineGraphWidget::Draw() {
  if (!fn_get_series || !fn_get_label_font)
    return;

  std::vector<LineGraphSeries> vec_series = fn_get_series();
  if (vec_series.empty())
    return;

  const std::string str_font = fn_get_label_font();
  if (str_font.empty())
    return;

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

  const int int4_graph_bottom_offset =
      int4_padding * 2 + int2_x_axis_label_size.Y + int4_x_axis_point_label_height;
  const int int4_height = rect.Height - (int4_graph_bottom_offset + int4_padding);

  float fp4_max_value = 0.0f;
  for (const auto& series : vec_series)
    for (const float fp4_point : series.vec_points)
      fp4_max_value = std::max(fp4_max_value, fp4_point);

  std::string_view sv_longest_name;
  for (const auto& series : vec_series)
    if (series.str_key.size() > sv_longest_name.size())
      sv_longest_name = series.str_key;
  const std::string str_longest_name{sv_longest_name};

  const float fp4_scale =
      200.0f / std::max(5000.0f,
                        std::ceil(fp4_max_value / 1000.0f) * 1000.0f);

  const int int4_width_max_value = font_label
                                       .Measure(FormatCurrent(
                                           fn_get_y_axis_value_format(),
                                           int4_height / fp4_scale))
                                       .X;
  const int int4_width_longest_name = font_label.Measure(str_longest_name).X;

  const std::string str_y_axis_label = fn_get_y_axis_label();
  const int2 int2_y_axis_label_size = font_axis.Measure(str_y_axis_label);

  const int int4_width =
      rect.Width - (int4_padding * 4 + int4_width_max_value +
                    int4_width_longest_name + int2_y_axis_label_size.Y);

  const int int4_x_step = int4_width / int4_x_axis_size;
  const int int4_y_step = int4_height / int4_y_axis_size;

  std::size_t sz_point_count = 0;
  for (const auto& series : vec_series)
    sz_point_count = std::max(sz_point_count, series.vec_points.size());
  const int int4_point_count = static_cast<int>(sz_point_count);
  const int int4_point_start = std::max(0, int4_point_count - int4_x_axis_size);
  const int int4_point_end = std::max(int4_point_count, int4_x_axis_size);

  const core::Vector2 vec_graph_origin{
      static_cast<float>(rect.Left() + int4_padding * 2 + int4_width_max_value +
                         int2_y_axis_label_size.Y),
      static_cast<float>(rect.Bottom() - int4_graph_bottom_offset)};
  const core::Vector2 vec_origin{static_cast<float>(rect.Left()),
                                 static_cast<float>(rect.Bottom())};

  int int4_key_offset = 0;

  std::stable_sort(vec_series.begin(), vec_series.end(),
                   [](const LineGraphSeries& a, const LineGraphSeries& b) {
                     const float fp4_a = a.vec_points.empty()
                                             ? 0.0f
                                             : a.vec_points.back();
                     const float fp4_b = b.vec_points.empty()
                                             ? 0.0f
                                             : b.vec_points.back();
                     return fp4_a > fp4_b;
                   });

  const core::Color color_white = core::Color::FromArgb(255, 255, 255, 255);
  for (const auto& series : vec_series) {
    const core::Color color_c = series.color_c;
    if (!series.vec_points.empty()) {
      const std::size_t sz_take_begin =
          series.vec_points.size() > static_cast<std::size_t>(int4_x_axis_size)
              ? series.vec_points.size() - static_cast<std::size_t>(int4_x_axis_size)
              : 0;
      std::vector<core::Vector3> vec_line;
      int int4_last_x = 0;
      float fp4_last_point = 0.0f;
      for (std::size_t sz_i = sz_take_begin; sz_i < series.vec_points.size(); ++sz_i) {
        const float fp4_point = series.vec_points[sz_i];
        int4_last_x = static_cast<int>(sz_i - sz_take_begin);
        fp4_last_point = fp4_point;
        vec_line.push_back(core::Vector3{
            vec_graph_origin.X + int4_last_x * int4_x_step,
            vec_graph_origin.Y - fp4_point * fp4_scale, 0.0f});
      }
      cr.DrawLine(vec_line, 1.0f, color_c);

      if (fp4_last_point != 0.0f)
        font_label.DrawTextWithShadow(
            FormatCurrent(fn_get_value_format(), fp4_last_point),
            core::Vector2{vec_graph_origin.X + int4_last_x * int4_x_step,
                          vec_graph_origin.Y - fp4_last_point * fp4_scale - 2.0f},
            color_c, color_background_dark, color_background_light, 1);
    }

    font_label.DrawTextWithShadow(
        series.str_key,
        core::Vector2{
            static_cast<float>(rect.Right() - (int4_width_longest_name + int4_padding)),
            static_cast<float>(rect.Top() + 10 * int4_key_offset + 3)},
        color_c, color_background_dark, color_background_light, 1);
    int4_key_offset++;
  }

  font_axis.DrawTextWithShadow(
      str_x_axis_label,
      core::Vector2{
          vec_graph_origin.X + int4_width / 2 - int2_x_axis_label_size.X / 2,
          vec_origin.Y - (int2_x_axis_label_size.Y + int4_padding)},
      color_white, color_background_dark, color_background_light, 1);

  for (int int4_n = int4_point_start, int4_x = 0; int4_n <= int4_point_end;
       int4_n++, int4_x += int4_x_step) {
    cr.DrawLine(core::Vector2{vec_graph_origin.X + int4_x, vec_graph_origin.Y},
                core::Vector2{vec_graph_origin.X + int4_x, vec_graph_origin.Y - 5},
                1.0f, color_white);
    if (int4_n % int4_x_axis_ticks_per_label != 0)
      continue;

    const std::string str_x_axis_text = FormatCurrent(
        fn_get_x_axis_value_format(), int4_n / int4_x_axis_ticks_per_label);
    const int int4_x_axis_tick_text_width = font_label.Measure(str_x_axis_text).X;
    const int int4_x_location = int4_x - int4_x_axis_tick_text_width / 2;
    font_label.DrawTextWithShadow(
        str_x_axis_text,
        core::Vector2{vec_graph_origin.X + int4_x_location, vec_graph_origin.Y + 2},
        color_white, color_background_dark, color_background_light, 1);
  }

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

}
