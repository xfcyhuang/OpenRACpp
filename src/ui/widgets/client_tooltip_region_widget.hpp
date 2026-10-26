// UPSTREAM: OpenRA.Mods.Common/Widgets/ClientTooltipRegionWidget.cs @1f594e320 L20-76(全文)。
#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::net {
class OrderManager;
struct SessionClient;
}
namespace ora::gfx {
class WorldRenderer;
}

namespace ora::ui {

class TooltipContainerWidget;

class ClientTooltipRegionWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ClientTooltipRegionWidget";

  ClientTooltipRegionWidget();
  explicit ClientTooltipRegionWidget(const ClientTooltipRegionWidget& widget_other);
  ~ClientTooltipRegionWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ClientTooltipRegionWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_tooltip_container;
  std::string str_template;

  void Bind(net::OrderManager* ptr_order_manager, gfx::WorldRenderer* ptr_world_renderer,
            net::SessionClient* ptr_client);

  void MouseEntered() override;
  void MouseExited() override;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 private:
  TooltipContainerWidget& TooltipContainerValue();

  net::OrderManager* ptr_order_manager_ = nullptr;      // 非拥有 | non-owning
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;    // 非拥有 | non-owning
  net::SessionClient* ptr_client_ = nullptr;            // 非拥有 | non-owning

  bool b_tooltip_container_created_ = false;
  TooltipContainerWidget* ptr_tooltip_container_ = nullptr;
};

}  // namespace ora::ui
