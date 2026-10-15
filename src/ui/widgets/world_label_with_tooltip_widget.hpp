// UPSTREAM: OpenRA.Mods.Common/Widgets/WorldLabelWithTooltipWidget.cs @e180e3225 L19-44(全文)。
#pragma once
import std;

#include "ui/widgets/label_with_tooltip_widget.hpp"

namespace ora::sim {
class World;
}

namespace ora::ui {

class WorldLabelWithTooltipWidget : public LabelWithTooltipWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "WorldLabelWithTooltipWidget";

  explicit WorldLabelWithTooltipWidget(WidgetArgs& args_args);
  explicit WorldLabelWithTooltipWidget(const WorldLabelWithTooltipWidget& widget_other);
  ~WorldLabelWithTooltipWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new WorldLabelWithTooltipWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  sim::World* ptr_world = nullptr;  // 非拥有 | non-owning

  void MouseEntered() override;
};

}  // namespace ora::ui
