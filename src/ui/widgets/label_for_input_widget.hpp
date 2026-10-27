#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::gfx {
class SpriteFont;
}

namespace ora::ui {

class LabelForInputWidget : public LabelWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "LabelForInputWidget";

  explicit LabelForInputWidget(WidgetArgs& args_args);
  explicit LabelForInputWidget(const LabelForInputWidget& widget_other);
  ~LabelForInputWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new LabelForInputWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_for;
  core::Color color_text_disabled{};

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 protected:
  void DrawInner(const std::string& str_text, gfx::SpriteFont& font_text,
                 core::Color color_c, int2 int2_position) override;

 private:
  InputWidget& InputWidgetValue();

  bool b_input_widget_created_ = false;
  InputWidget* ptr_input_widget_ = nullptr;

  core::CachedTransform<bool, core::Color> cache_text_color_;
};

}
