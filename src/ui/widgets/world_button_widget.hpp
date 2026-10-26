// UPSTREAM: OpenRA.Mods.Common/Widgets/WorldButtonWidget.cs @1f594e320 L19-49(全文)。
#pragma once
import std;

#include "ui/widgets/button_widget.hpp"

namespace ora::sim {
class World;
}

namespace ora::ui {

class WorldButtonWidget : public ButtonWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "WorldButtonWidget";

  explicit WorldButtonWidget(WidgetArgs& args_args);
  explicit WorldButtonWidget(const WorldButtonWidget& widget_other);
  ~WorldButtonWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new WorldButtonWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  sim::World* ptr_world = nullptr;  // 非拥有 | non-owning

  void MouseEntered() override;
};

}  // namespace ora::ui
