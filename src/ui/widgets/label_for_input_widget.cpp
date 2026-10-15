// UPSTREAM: OpenRA.Mods.Common/Widgets/LabelForInputWidget.cs @e180e3225 L20-48(全文)。
import std;

#include "ui/widgets/label_for_input_widget.hpp"

#include "gfx/sprite_font.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

namespace {

core::CachedTransform<bool, core::Color> MakeTextColorTransform(LabelForInputWidget* ptr_self) {
  return core::CachedTransform<bool, core::Color>(
      [ptr_self](bool b_disabled) -> core::Color {
        return b_disabled ? ptr_self->color_text_disabled : ptr_self->color_text;
      });
}

}  // namespace

LabelForInputWidget::LabelForInputWidget(WidgetArgs& args_args) : LabelWidget(args_args) {
  color_text_disabled = RequireChromeMetrics().GetColor("TextDisabledColor");
  cache_text_color_ = MakeTextColorTransform(this);
}

LabelForInputWidget::LabelForInputWidget(const LabelForInputWidget& widget_other)
    : LabelWidget(widget_other),
      str_for(widget_other.str_for),
      color_text_disabled(RequireChromeMetrics().GetColor("TextDisabledColor")) {
  cache_text_color_ = MakeTextColorTransform(this);
}

InputWidget& LabelForInputWidget::InputWidgetValue() {
  if (!b_input_widget_created_) {
    ptr_input_widget_ = ptr_parent->Get<InputWidget>(str_for);
    b_input_widget_created_ = true;
  }
  return *ptr_input_widget_;
}

void LabelForInputWidget::DrawInner(const std::string& str_text,
                                    gfx::SpriteFont& font_text, core::Color color_c,
                                    int2 int2_position) {
  (void)color_c;
  const core::Color color_draw = cache_text_color_.Update(InputWidgetValue().fn_is_disabled());
  const core::Vector2 vec_pos{static_cast<float>(int2_position.X),
                              static_cast<float>(int2_position.Y)};
  font_text.DrawText(str_text, vec_pos, color_draw);
}

bool LabelForInputWidget::LoadFieldOrProperty(std::string_view str_key,
                                              std::string_view str_value) {
  if (str_key == "For") {
    str_for = std::string{str_value};
    return true;
  }

  return LabelWidget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
