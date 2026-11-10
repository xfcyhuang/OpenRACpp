#pragma once

import std;

namespace ora::game::ui {

[[nodiscard]] std::expected<int, std::string> widget_int_eval(
    std::string_view str_expr, const std::map<std::string, int>& map_symbols);

}
