// UPSTREAM: OpenRA.Mods.Common/Widgets/MenuButtonWidget.cs @1f594e320 L19-33(全文)。
import std;

#include "ui/widgets/menu_button_widget.hpp"

#include "meta/field_loader.hpp"

namespace ora::ui {

MenuButtonWidget::MenuButtonWidget(WidgetArgs& args_args) : WorldButtonWidget(args_args) {}

MenuButtonWidget::MenuButtonWidget(const MenuButtonWidget& widget_other)
    : WorldButtonWidget(widget_other),
      str_menu_container(widget_other.str_menu_container),
      b_pause(widget_other.b_pause),
      b_hide_ingame_ui(widget_other.b_hide_ingame_ui) {}

bool MenuButtonWidget::LoadFieldOrProperty(std::string_view str_key,
                                           std::string_view str_value) {
  if (str_key == "MenuContainer") {
    str_menu_container = std::string{str_value};
    return true;
  }
  if (str_key == "Pause") {
    b_pause = meta::GetBoolValue("Pause", str_value);
    return true;
  }
  if (str_key == "HideIngameUI") {
    b_hide_ingame_ui = meta::GetBoolValue("HideIngameUI", str_value);
    return true;
  }
  if (str_key == "DisableWorldSounds") {
    b_disable_world_sounds = meta::GetBoolValue("DisableWorldSounds", str_value);
    return true;
  }

  return WorldButtonWidget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
