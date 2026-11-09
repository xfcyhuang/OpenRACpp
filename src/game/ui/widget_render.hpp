#pragma once

import std;

#include "game/render/font.hpp"
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
};

[[nodiscard]] std::vector<UiQuad> ui_draw_tree(const WidgetModel& root, const UiRenderCtx& ctx);

}
