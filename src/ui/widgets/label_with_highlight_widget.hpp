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

struct LabelHighlightComponent {
  std::string str_text;
  bool b_highlighted = false;
};

class LabelWithHighlightWidget : public LabelWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "LabelWithHighlightWidget";

  explicit LabelWithHighlightWidget(WidgetArgs& args_args);
  explicit LabelWithHighlightWidget(const LabelWithHighlightWidget& widget_other);
  ~LabelWithHighlightWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new LabelWithHighlightWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  core::Color color_highlight{};

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 protected:
  void DrawInner(const std::string& str_text, gfx::SpriteFont& font_text,
                 core::Color color_c, int2 int2_position) override;

 private:
  std::vector<LabelHighlightComponent> MakeComponents(const std::string& str_text);

  core::CachedTransform<std::string, std::vector<LabelHighlightComponent>> cache_components_;
};

}
