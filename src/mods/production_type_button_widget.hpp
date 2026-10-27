#pragma once
import std;

#include "ui/widgets/world_button_widget.hpp"

namespace ora::mods {

class ProductionTypeButtonWidget final : public ora::ui::WorldButtonWidget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "ProductionTypeButtonWidget";

  explicit ProductionTypeButtonWidget(ora::ui::WidgetArgs& args_args)
      : ora::ui::WorldButtonWidget(args_args) {}
  explicit ProductionTypeButtonWidget(
      const ProductionTypeButtonWidget& widget_other)
      : ora::ui::WorldButtonWidget(widget_other),
        str_production_group(widget_other.str_production_group) {}
  ~ProductionTypeButtonWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new ProductionTypeButtonWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override {
    if (str_key == "ProductionGroup") {
      str_production_group = std::string{str_value};
      return true;
    }
    return ora::ui::WorldButtonWidget::LoadFieldOrProperty(str_key, str_value);
  }

  const std::string& ProductionGroup() const { return str_production_group; }

 private:
  std::string str_production_group;
};

void RegisterProductionTypeButtonWidget();

}
