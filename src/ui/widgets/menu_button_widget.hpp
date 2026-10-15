// UPSTREAM: OpenRA.Mods.Common/Widgets/MenuButtonWidget.cs @e180e3225 L19-33(全文)。
#pragma once
import std;

#include "ui/widgets/world_button_widget.hpp"

namespace ora::ui {

class MenuButtonWidget : public WorldButtonWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "MenuButtonWidget";

  explicit MenuButtonWidget(WidgetArgs& args_args);
  explicit MenuButtonWidget(const MenuButtonWidget& widget_other);
  ~MenuButtonWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new MenuButtonWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_menu_container = "INGAME_MENU";
  bool b_pause = true;
  bool b_hide_ingame_ui = true;
  bool b_disable_world_sounds = false;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;
};

}  // namespace ora::ui
