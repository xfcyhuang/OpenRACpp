#pragma once

import std;

#include "game/render/font.hpp"
#include "game/ui/chrome_provider.hpp"
#include "game/ui/widget.hpp"

namespace ora::game::ui {

struct UiQuad {
  int int4_x{};
  int int4_y{};
  int int4_width{};
  int int4_height{};
  int int4_src_x{};
  int int4_src_y{};
  std::uint32_t u4_sheet{};
  std::uint32_t u4_rgba{0xFFFFFFFFu};
};

struct UiRenderCtx {
  std::map<std::string, render::SpriteFont>* map_fonts{};
  std::string str_default_font;
  std::uint32_t u4_default_color{0xFFFFFFFFu};
  const std::map<std::string, ChromeCollection>* map_chrome{};
  const ChromeSheetSet* set_sheets{};
  const std::map<std::string, std::string>* map_metrics{};
  const UiState* ptr_state{};
};

[[nodiscard]] std::vector<UiQuad> ui_draw_tree(const WidgetModel& root, const UiRenderCtx& ctx);

[[nodiscard]] std::vector<std::string> widgets_draw_dump(const WidgetModel& root, const UiRenderCtx& ctx);

[[nodiscard]] std::string widget_stateful_image_name(std::string_view str_base, bool b_disabled,
                                                     bool b_pressed, bool b_hover);

}
