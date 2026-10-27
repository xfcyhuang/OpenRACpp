import std;

#include "ui/logic/simple_tooltip_logic.hpp"

#include "gfx/sprite_font.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

SimpleTooltipLogic::SimpleTooltipLogic(WidgetArgs& args_args) {
  Widget* ptr_widget = std::get<Widget*>(args_args.At("widget"));
  TooltipContainerWidget& container_tooltip = *static_cast<TooltipContainerWidget*>(
      std::get<Widget*>(args_args.At("tooltipContainer")));
  std::function<std::string()> fn_get_text =
      std::get<std::function<std::string()>>(args_args.At("getText"));

  LabelWidget* ptr_label = ptr_widget->Get<LabelWidget>("LABEL");
  Widget* ptr_spacing = ptr_widget->Get("LINE_HEIGHT");

  up_label_template_.reset(static_cast<LabelWidget*>(ptr_label->Clone().release()));

  LabelWidget* ptr_label_template = up_label_template_.get();
  ptr_label_template->fn_is_visible = [ptr_label_template] {
    return ptr_label_template->b_visible;
  };
  ptr_label_template->BindStandardDelegates();
  const int int4_spacing_y = ptr_spacing->bounds_widget.int4_y;
  const int int4_spacing_height = ptr_spacing->bounds_widget.int4_height;

  ptr_widget->RemoveChildren();

  gfx::SpriteFont& font_text = RequireWidgetFont(up_label_template_->str_font);
  int int4_horizontal_padding =
      up_label_template_->bounds_widget.int4_width - ptr_widget->bounds_widget.int4_width;
  if (int4_horizontal_padding <= 0)
    int4_horizontal_padding = 2 * up_label_template_->bounds_widget.int4_x;

  auto str_cached_text = std::make_shared<std::string>();
  container_tooltip.fn_before_render =
      [ptr_widget, this, &font_text, int4_horizontal_padding, int4_spacing_y,
       int4_spacing_height, str_cached_text, fn_get_text]() {
        const std::string str_text = fn_get_text();
        if (str_text == *str_cached_text)
          return;

        std::vector<std::string> vec_lines;
        for (std::size_t int8_begin = 0; int8_begin <= str_text.size();) {
          const std::size_t int8_nl = str_text.find('\n', int8_begin);
          if (int8_nl == std::string::npos) {
            vec_lines.emplace_back(str_text.substr(int8_begin));
            break;
          }
          vec_lines.emplace_back(str_text.substr(int8_begin, int8_nl - int8_begin));
          int8_begin = int8_nl + 1;
        }
        const int int4_text_width = font_text.Measure(str_text).X;

        ptr_widget->RemoveChildren();
        int int4_bottom = 0;
        for (std::size_t int8_i = 0; int8_i < vec_lines.size(); ++int8_i) {
          std::unique_ptr<Widget> up_line = up_label_template_->Clone();
          LabelWidget* ptr_line = static_cast<LabelWidget*>(up_line.get());
          const std::string str_line_text = vec_lines[int8_i];
          ptr_line->bounds_widget.int4_y +=
              int4_spacing_y + static_cast<int>(int8_i) * int4_spacing_height;
          ptr_line->bounds_widget.int4_width = int4_text_width;
          ptr_line->fn_get_text = [str_line_text] { return str_line_text; };
          ptr_widget->AddChild(std::move(up_line));
          int4_bottom =
              ptr_line->bounds_widget.int4_y + ptr_line->bounds_widget.int4_height;
        }

        ptr_widget->bounds_widget.int4_width = int4_horizontal_padding + int4_text_width;
        ptr_widget->bounds_widget.int4_height = int4_bottom + int4_spacing_y;
        *str_cached_text = str_text;
      };
}

}
