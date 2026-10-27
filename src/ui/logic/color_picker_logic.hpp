#pragma once
import std;

#include "core/color.hpp"
#include "server/color_picker_manager.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class ColorPickerLogic : public ChromeLogic {
 public:
  explicit ColorPickerLogic(WidgetArgs& args_args);

  void Tick() override;

 private:
  int int4_palette_tab_highlighted = 0;
  server::ColorPickerManagerData data_manager_{};
  std::vector<core::Color> vec_terrain_colors_{};
};

}
