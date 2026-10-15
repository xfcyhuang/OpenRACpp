// UPSTREAM: OpenRA.Mods.Common/Widgets/WorldButtonWidget.cs @e180e3225 L19-49(全文)。
import std;

#include "ui/widgets/world_button_widget.hpp"

#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

WorldButtonWidget::WorldButtonWidget(WidgetArgs& args_args) : ButtonWidget(args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world = *ptr_world_arg;
}

WorldButtonWidget::WorldButtonWidget(const WorldButtonWidget& widget_other)
    : ButtonWidget(widget_other), ptr_world(widget_other.ptr_world) {}

void WorldButtonWidget::MouseEntered() {
  if (str_tooltip_container.empty() || !fn_get_tooltip_text)
    return;

  TooltipContainerWidget* ptr_container = TooltipContainerOrNull();
  if (ptr_container == nullptr)
    return;

  WidgetArgs args_tooltip;
  args_tooltip.Add("button", static_cast<Widget*>(this));
  args_tooltip.Add("getText", fn_get_tooltip_text);
  args_tooltip.Add("getDesc", fn_get_tooltip_desc);
  args_tooltip.Add("world", ptr_world);
  ptr_container->SetTooltip(str_tooltip_template, std::move(args_tooltip));
}

}  // namespace ora::ui
