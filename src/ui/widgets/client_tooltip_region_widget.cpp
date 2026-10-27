import std;

#include "ui/widgets/client_tooltip_region_widget.hpp"

#include "ui/ui.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

ClientTooltipRegionWidget::ClientTooltipRegionWidget() = default;

ClientTooltipRegionWidget::ClientTooltipRegionWidget(
    const ClientTooltipRegionWidget& widget_other)
    : Widget(widget_other),
      str_tooltip_container(widget_other.str_tooltip_container),
      str_template(widget_other.str_template),
      ptr_order_manager_(widget_other.ptr_order_manager_),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      ptr_client_(widget_other.ptr_client_) {}

void ClientTooltipRegionWidget::Bind(net::OrderManager* ptr_order_manager,
                                     gfx::WorldRenderer* ptr_world_renderer,
                                     net::SessionClient* ptr_client) {
  ptr_order_manager_ = ptr_order_manager;
  ptr_world_renderer_ = ptr_world_renderer;
  ptr_client_ = ptr_client;
}

TooltipContainerWidget& ClientTooltipRegionWidget::TooltipContainerValue() {
  if (!b_tooltip_container_created_) {
    ptr_tooltip_container_ =
        Ui::Instance().Root()->Get<TooltipContainerWidget>(str_tooltip_container);
    b_tooltip_container_created_ = true;
  }
  return *ptr_tooltip_container_;
}

void ClientTooltipRegionWidget::MouseEntered() {
  if (str_tooltip_container.empty())
    return;

  WidgetArgs args_tooltip;
  args_tooltip.Add("orderManager", ptr_order_manager_);
  args_tooltip.Add("worldRenderer", ptr_world_renderer_);
  args_tooltip.Add("client", ptr_client_);
  TooltipContainerValue().SetTooltip(str_template, std::move(args_tooltip));
}

void ClientTooltipRegionWidget::MouseExited() {
  if (str_tooltip_container.empty())
    return;

  TooltipContainerValue().RemoveTooltip();
}

bool ClientTooltipRegionWidget::LoadFieldOrProperty(std::string_view str_key,
                                                    std::string_view str_value) {
  if (str_key == "TooltipContainer") {
    str_tooltip_container = std::string{str_value};
    return true;
  }
  if (str_key == "Template") {
    str_template = std::string{str_value};
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

}
