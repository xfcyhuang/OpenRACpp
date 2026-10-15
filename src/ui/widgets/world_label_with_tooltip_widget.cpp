// UPSTREAM: OpenRA.Mods.Common/Widgets/WorldLabelWithTooltipWidget.cs @e180e3225 L19-44(全文)。
import std;

#include "ui/widgets/world_label_with_tooltip_widget.hpp"

#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

WorldLabelWithTooltipWidget::WorldLabelWithTooltipWidget(WidgetArgs& args_args)
    : LabelWithTooltipWidget(args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world = *ptr_world_arg;
}

WorldLabelWithTooltipWidget::WorldLabelWithTooltipWidget(
    const WorldLabelWithTooltipWidget& widget_other)
    : LabelWithTooltipWidget(widget_other), ptr_world(widget_other.ptr_world) {}

void WorldLabelWithTooltipWidget::MouseEntered() {
  if (str_tooltip_container.empty())
    return;

  if (fn_get_tooltip_text) {
    WidgetArgs args_tooltip;
    args_tooltip.Add("getText", fn_get_tooltip_text);
    args_tooltip.Add("world", ptr_world);
    TooltipContainerValue().SetTooltip(str_tooltip_template, std::move(args_tooltip));
  }
}

}  // namespace ora::ui
