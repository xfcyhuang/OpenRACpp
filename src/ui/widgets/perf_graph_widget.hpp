#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::gfx {
class SpriteFont;
}

namespace ora::ui {

class PerfGraphWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "PerfGraphWidget";

  PerfGraphWidget();
  explicit PerfGraphWidget(WidgetArgs& args_args) : PerfGraphWidget() {
    (void)args_args;
  }
  ~PerfGraphWidget() override = default;

  std::string_view TypeName() const override { return kWidgetTypeName; }

  static void SixCharacterFormatFloat(std::string& str_output, double fp8_value);

  void Draw() override;

 private:
  int GetTextWidth(const std::string& str_text);

  gfx::SpriteFont* ptr_font_ = nullptr;
  int int4_dot_width = 0;
  int int4_next_dot_advance = 0;
  std::map<std::string, int> map_text_width_cache_;
  std::string str_builder_;
};

}
