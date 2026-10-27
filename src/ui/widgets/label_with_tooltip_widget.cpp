import std;

#include "ui/widgets/label_with_tooltip_widget.hpp"

#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

LabelWithTooltipWidget::LabelWithTooltipWidget(
    const LabelWithTooltipWidget& widget_other)
    : LabelWidget(widget_other),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_tooltip_container(widget_other.str_tooltip_container),
      fn_get_tooltip_text(widget_other.fn_get_tooltip_text) {}

TooltipContainerWidget& LabelWithTooltipWidget::TooltipContainerValue() {
  if (!b_tooltip_container_created_) {
    ptr_tooltip_container_ =
        Ui::Instance().Root()->Get<TooltipContainerWidget>(str_tooltip_container);
    b_tooltip_container_created_ = true;
  }
  return *ptr_tooltip_container_;
}

void LabelWithTooltipWidget::MouseEntered() {
  if (str_tooltip_container.empty())
    return;

  if (fn_get_tooltip_text) {
    WidgetArgs args_tooltip;
    args_tooltip.Add("getText", fn_get_tooltip_text);
    TooltipContainerValue().SetTooltip(str_tooltip_template, std::move(args_tooltip));
  }
}

void LabelWithTooltipWidget::MouseExited() {

  if (!str_tooltip_container.empty() && b_tooltip_container_created_)
    ptr_tooltip_container_->RemoveTooltip();
}

bool LabelWithTooltipWidget::LoadFieldOrProperty(std::string_view str_key,
                                                 std::string_view str_value) {
  if (str_key == "TooltipTemplate") {
    str_tooltip_template = std::string{str_value};
    return true;
  }
  if (str_key == "TooltipContainer") {
    str_tooltip_container = std::string{str_value};
    return true;
  }

  return LabelWidget::LoadFieldOrProperty(str_key, str_value);
}

}
