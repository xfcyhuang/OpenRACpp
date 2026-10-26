// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ButtonTooltipLogic.cs @1f594e320 L17-68(全文)。
import std;

#include "ui/logic/button_tooltip_logic.hpp"

#include "gfx/sprite_font.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

ButtonTooltipLogic::ButtonTooltipLogic(WidgetArgs& args_args) {
  Widget* ptr_widget = std::get<Widget*>(args_args.At("widget"));
  ButtonWidget* ptr_button = static_cast<ButtonWidget*>(std::get<Widget*>(args_args.At("button")));

  LabelWidget* ptr_label = ptr_widget->Get<LabelWidget>("LABEL");
  gfx::SpriteFont& font_text = RequireWidgetFont(ptr_label->str_font);
  const std::string str_text = ptr_button->fn_get_tooltip_text();
  const int int4_label_width = font_text.Measure(str_text).X;
  const Hotkey key_hotkey = ptr_button->key_hotkey.GetValue();

  ptr_label->fn_get_text = [str_text] { return str_text; };
  ptr_label->bounds_widget.int4_width = int4_label_width;
  ptr_widget->bounds_widget.int4_width =
      2 * ptr_label->bounds_widget.int4_x + int4_label_width;

  if (key_hotkey.IsValid()) {
    LabelWidget* ptr_hotkey = ptr_widget->Get<LabelWidget>("HOTKEY");
    ptr_hotkey->b_visible = true;

    const std::string str_hotkey_label = "(" + key_hotkey.DisplayString() + ")";
    ptr_hotkey->fn_get_text = [str_hotkey_label] { return str_hotkey_label; };
    ptr_hotkey->bounds_widget.int4_x =
        int4_label_width + 2 * ptr_label->bounds_widget.int4_x;

    ptr_widget->bounds_widget.int4_width = ptr_hotkey->bounds_widget.int4_x +
                                           ptr_label->bounds_widget.int4_x +
                                           font_text.Measure(str_hotkey_label).X;
  }

  const std::string str_desc =
      ptr_button->fn_get_tooltip_desc ? ptr_button->fn_get_tooltip_desc() : std::string{};
  if (!str_desc.empty()) {
    LabelWidget* ptr_desc_template_found = ptr_widget->Get<LabelWidget>("DESC");
    up_desc_template_.reset(static_cast<LabelWidget*>(ptr_desc_template_found->Clone().release()));
    LabelWidget* ptr_desc_template = up_desc_template_.get();
    ptr_widget->RemoveChild(*ptr_desc_template_found);

    gfx::SpriteFont& font_desc = RequireWidgetFont(ptr_desc_template->str_font);
    int int4_desc_width = 0;
    int int4_desc_offset = ptr_desc_template->bounds_widget.int4_y;
    std::size_t int8_line_begin = 0;
    while (true) {
      const std::size_t int8_line_end = str_desc.find('\n', int8_line_begin);
      const std::string str_line =
          int8_line_end == std::string::npos
              ? str_desc.substr(int8_line_begin)
              : str_desc.substr(int8_line_begin, int8_line_end - int8_line_begin);
      int4_desc_width = std::max(int4_desc_width, font_desc.Measure(str_line).X);

      std::unique_ptr<Widget> up_line = ptr_desc_template->Clone();
      LabelWidget* ptr_line = static_cast<LabelWidget*>(up_line.get());
      ptr_line->fn_is_visible = [ptr_line] { return ptr_line->b_visible; };
      ptr_line->BindStandardDelegates();
      ptr_line->fn_get_text = [str_line] { return str_line; };
      ptr_line->bounds_widget.int4_y = int4_desc_offset;
      ptr_widget->AddChild(std::move(up_line));
      int4_desc_offset += ptr_desc_template->bounds_widget.int4_height;

      if (int8_line_end == std::string::npos)
        break;
      int8_line_begin = int8_line_end + 1;
    }

    ptr_widget->bounds_widget.int4_width = std::max(
        ptr_widget->bounds_widget.int4_width,
        ptr_desc_template->bounds_widget.int4_x * 2 + int4_desc_width);
    ptr_widget->bounds_widget.int4_height +=
        int4_desc_offset - ptr_desc_template->bounds_widget.int4_y +
        ptr_desc_template->bounds_widget.int4_x;
  }
}

}  // namespace ora::ui
