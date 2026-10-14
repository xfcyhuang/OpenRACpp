// UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWidget.cs @e180e3225(label_widget.hpp
// 的实现)
// The implementation of label_widget.hpp.
import std;

#include "ui/widgets/label_widget.hpp"

#include "fluent/fluent_provider.hpp"
#include "meta/field_loader.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {
namespace {

gfx::SpriteFont& RequireFont(const std::string& str_font) {
  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  gfx::SpriteFont* ptr_font =
      ptr_renderer != nullptr ? ptr_renderer->TryGetFont(str_font) : nullptr;
  if (ptr_font == nullptr)
    throw std::runtime_error("Requested font '" + str_font + "' was not found.");
  return *ptr_font;
}

}  // namespace

LabelWidget::LabelWidget(WidgetArgs& args_args)
    : cache_text_([](const std::string& str_s) {
        return !str_s.empty() ? fluent::FluentProvider::Instance().GetMessage(str_s)
                              : std::string{};
      }) {
  (void)args_args;

  ChromeMetrics& metrics = RequireChromeMetrics();
  str_font = metrics.Get("TextFont");
  color_text = metrics.GetColor("TextColor");
  b_contrast = metrics.GetBool("TextContrast");
  b_shadow = metrics.GetBool("TextShadow");
  color_contrast_dark = metrics.GetColor("TextContrastColorDark");
  color_contrast_light = metrics.GetColor("TextContrastColorLight");
  int4_contrast_radius = metrics.GetInt32("TextContrastRadius");

  fn_get_text = [this] { return cache_text_.Update(opt_text.value_or("")); };
  fn_get_color = [this] { return color_text; };
  fn_get_contrast_color_dark = [this] { return color_contrast_dark; };
  fn_get_contrast_color_light = [this] { return color_contrast_light; };
}

LabelWidget::LabelWidget(const LabelWidget& widget_other)
    : Widget(widget_other),
      opt_text(widget_other.opt_text),
      kind_align(widget_other.kind_align),
      kind_valign(widget_other.kind_valign),
      str_font(widget_other.str_font),
      color_text(widget_other.color_text),
      b_contrast(widget_other.b_contrast),
      b_shadow(widget_other.b_shadow),
      color_contrast_dark(widget_other.color_contrast_dark),
      color_contrast_light(widget_other.color_contrast_light),
      int4_contrast_radius(widget_other.int4_contrast_radius),
      b_word_wrap(widget_other.b_word_wrap),
      fn_get_text(widget_other.fn_get_text),
      fn_get_color(widget_other.fn_get_color),
      fn_get_contrast_color_dark(widget_other.fn_get_contrast_color_dark),
      fn_get_contrast_color_light(widget_other.fn_get_contrast_color_light),
      cache_text_([](const std::string& str_s) {
        return !str_s.empty() ? fluent::FluentProvider::Instance().GetMessage(str_s)
                              : std::string{};
      }) {}

void LabelWidget::IncreaseHeightToFitCurrentText() {
  gfx::SpriteFont& font_text = RequireFont(str_font);

  std::string str_line = fn_get_text();
  if (b_word_wrap)
    str_line = WrapText(str_line, bounds_widget.int4_width, font_text);
  bounds_widget.int4_height =
      std::max(bounds_widget.int4_height, font_text.Measure(str_line).Y);
}

void LabelWidget::Draw() {
  gfx::SpriteFont& font_text = RequireFont(str_font);

  const std::string str_text_drawn = fn_get_text();

  std::string str_text = str_text_drawn;
  if (b_word_wrap)
    str_text = WrapText(str_text, bounds_widget.int4_width, font_text);

  const int2 int2_text_size = font_text.Measure(str_text);
  int2 int2_position = RenderOrigin();
  const int int4_offset = font_text.TopOffset();

  if (kind_valign == TextVAlign::Top)
    int2_position = int2_position + int2{0, -int4_offset};

  if (kind_valign == TextVAlign::Middle)
    int2_position = int2_position +
                    int2{0, (bounds_widget.int4_height - int2_text_size.Y - int4_offset) / 2};

  if (kind_valign == TextVAlign::Bottom)
    int2_position = int2_position + int2{0, bounds_widget.int4_height - int2_text_size.Y};

  if (kind_align == TextAlign::Center)
    int2_position = int2_position + int2{(bounds_widget.int4_width - int2_text_size.X) / 2, 0};

  if (kind_align == TextAlign::Right)
    int2_position = int2_position + int2{bounds_widget.int4_width - int2_text_size.X, 0};

  DrawInner(str_text, font_text, fn_get_color(), int2_position);
}

bool LabelWidget::LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) {
  if (str_key == "Text") {
    opt_text = std::string{str_value};
    return true;
  }
  if (str_key == "Align") {
    const std::string str_value_owned{str_value};
    if (str_value_owned == "Left")
      kind_align = TextAlign::Left;
    else if (str_value_owned == "Center")
      kind_align = TextAlign::Center;
    else if (str_value_owned == "Right")
      kind_align = TextAlign::Right;
    else
      throw std::runtime_error(
          "FieldLoader: Cannot parse `" + str_value_owned +
          "` into field `Align` of type `OpenRA.Mods.Common.Widgets.TextAlign`");
    return true;
  }
  if (str_key == "VAlign") {
    const std::string str_value_owned{str_value};
    if (str_value_owned == "Top")
      kind_valign = TextVAlign::Top;
    else if (str_value_owned == "Middle")
      kind_valign = TextVAlign::Middle;
    else if (str_value_owned == "Bottom")
      kind_valign = TextVAlign::Bottom;
    else
      throw std::runtime_error(
          "FieldLoader: Cannot parse `" + str_value_owned +
          "` into field `VAlign` of type `OpenRA.Mods.Common.Widgets.TextVAlign`");
    return true;
  }
  if (str_key == "Font") {
    str_font = std::string{str_value};
    return true;
  }
  if (str_key == "TextColor") {
    color_text = meta::GetColorValue("TextColor", str_value);
    return true;
  }
  if (str_key == "Contrast") {
    b_contrast = meta::GetBoolValue("Contrast", str_value);
    return true;
  }
  if (str_key == "Shadow") {
    b_shadow = meta::GetBoolValue("Shadow", str_value);
    return true;
  }
  if (str_key == "ContrastColorDark") {
    color_contrast_dark = meta::GetColorValue("ContrastColorDark", str_value);
    return true;
  }
  if (str_key == "ContrastColorLight") {
    color_contrast_light = meta::GetColorValue("ContrastColorLight", str_value);
    return true;
  }
  if (str_key == "ContrastRadius") {
    int4_contrast_radius = meta::GetInt32Value("ContrastRadius", str_value);
    return true;
  }
  if (str_key == "WordWrap") {
    b_word_wrap = meta::GetBoolValue("WordWrap", str_value);
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

void LabelWidget::DrawInner(const std::string& str_text, gfx::SpriteFont& font_text,
                            core::Color color_c, int2 int2_position) {
  const core::Color color_bg_dark = fn_get_contrast_color_dark();
  const core::Color color_bg_light = fn_get_contrast_color_light();
  const core::Vector2 vec_pos{static_cast<float>(int2_position.X),
                              static_cast<float>(int2_position.Y)};
  if (b_contrast)
    font_text.DrawTextWithContrast(str_text, vec_pos, color_c, color_bg_dark, color_bg_light,
                                   int4_contrast_radius);
  else if (b_shadow)
    font_text.DrawTextWithShadow(str_text, vec_pos, color_c, color_bg_dark, color_bg_light, 1);
  else
    font_text.DrawText(str_text, vec_pos, color_c);
}

}  // namespace ora::ui
