// UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWithHighlightWidget.cs @e180e3225 L20-86(全文)。
import std;

#include "ui/widgets/label_with_highlight_widget.hpp"

#include "gfx/sprite_font.hpp"
#include "meta/field_loader.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

LabelWithHighlightWidget::LabelWithHighlightWidget(WidgetArgs& args_args)
    : LabelWidget(args_args) {
  color_highlight = RequireChromeMetrics().GetColor("TextHighlightColor");
  cache_components_ = core::CachedTransform<std::string, std::vector<LabelHighlightComponent>>(
      [this](const std::string& str_text) { return MakeComponents(str_text); });
}

LabelWithHighlightWidget::LabelWithHighlightWidget(
    const LabelWithHighlightWidget& widget_other)
    : LabelWidget(widget_other), color_highlight(widget_other.color_highlight) {
  cache_components_ = core::CachedTransform<std::string, std::vector<LabelHighlightComponent>>(
      [this](const std::string& str_text) { return MakeComponents(str_text); });
}

std::vector<LabelHighlightComponent> LabelWithHighlightWidget::MakeComponents(
    const std::string& str_text) {
  std::vector<LabelHighlightComponent> vec_components;
  std::size_t int8_line_begin = 0;
  while (true) {
    const std::size_t int8_line_end = str_text.find("\\n", int8_line_begin);
    const std::string_view sv_line =
        int8_line_end == std::string::npos
            ? std::string_view{str_text}.substr(int8_line_begin)
            : std::string_view{str_text}.substr(int8_line_begin,
                                                int8_line_end - int8_line_begin);

    std::string_view sv_rest = sv_line;
    while (!sv_rest.empty()) {
      const std::size_t int8_highlight_start = sv_rest.find('<');
      const std::size_t int8_highlight_end = sv_rest.find('>');
      if (int8_highlight_start != std::string_view::npos && int8_highlight_start > 0 &&
          int8_highlight_end != std::string_view::npos &&
          int8_highlight_end > int8_highlight_start) {
        vec_components.push_back(
            {std::string{sv_rest.substr(0, int8_highlight_start)}, false});
        vec_components.push_back({std::string{sv_rest.substr(
                                      int8_highlight_start + 1,
                                      int8_highlight_end - int8_highlight_start - 1)},
                                  true});
        sv_rest = sv_rest.substr(int8_highlight_end + 1);
      } else {
        vec_components.push_back({std::string{sv_rest}, false});
        break;
      }
    }

    if (int8_line_end == std::string::npos)
      break;
    int8_line_begin = int8_line_end + 2;
  }
  return vec_components;
}

void LabelWithHighlightWidget::DrawInner(const std::string& str_text,
                                         gfx::SpriteFont& font_text, core::Color color_c,
                                         int2 int2_position) {
  int int4_advance = 0;
  for (const LabelHighlightComponent& comp_text :
       cache_components_.Update(str_text)) {
    LabelWidget::DrawInner(comp_text.str_text, font_text,
                           comp_text.b_highlighted ? color_highlight : color_c,
                           int2_position + int2{int4_advance, 0});
    int4_advance += font_text.Measure(comp_text.str_text).X;
  }
}

bool LabelWithHighlightWidget::LoadFieldOrProperty(std::string_view str_key,
                                                   std::string_view str_value) {
  if (str_key == "HighlightColor") {
    color_highlight = meta::GetColorValue("HighlightColor", str_value);
    return true;
  }

  return LabelWidget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
