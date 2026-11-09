#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/ui/widget.hpp"

namespace ora::game::ui {

struct WidgetTypeSpec {
  std::string_view str_type;
  bool b_click_through{};
  bool b_ignore_mouse_over{};
  bool b_is_button{};
  bool b_cursor_null{};
};

[[nodiscard]] const std::vector<WidgetTypeSpec>& widget_type_registry();

[[nodiscard]] const WidgetTypeSpec* widget_type_spec_find(std::string_view str_type);

struct WidgetLoaderModel {
  std::map<std::string, const MiniYamlNode*> map_widgets;
};

[[nodiscard]] std::expected<WidgetLoaderModel, std::string> widget_loader_from_nodes(
    const std::vector<MiniYamlNode>& vec_nodes);

[[nodiscard]] std::expected<WidgetModel*, std::string> widget_loader_load_widget(
    const WidgetLoaderModel& loader, WidgetModel& parent, const WidgetInitCtx& ctx,
    std::string_view str_id);

[[nodiscard]] std::string_view widget_key_type_part(std::string_view str_key);

[[nodiscard]] std::string_view widget_key_id_part(std::string_view str_key);

}
