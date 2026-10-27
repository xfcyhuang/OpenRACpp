#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class SpriteFont;
}

namespace ora::ui {

enum class TextAlign { Left, Center, Right };
enum class TextVAlign { Top, Middle, Bottom };

class LabelWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "LabelWidget";

  explicit LabelWidget(WidgetArgs& args_args);
  explicit LabelWidget(const LabelWidget& widget_other);
  ~LabelWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new LabelWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::optional<std::string> opt_text;
  TextAlign kind_align = TextAlign::Left;
  TextVAlign kind_valign = TextVAlign::Middle;
  std::string str_font;
  core::Color color_text{};
  bool b_contrast = false;
  bool b_shadow = false;
  core::Color color_contrast_dark{};
  core::Color color_contrast_light{};
  int int4_contrast_radius = 0;
  bool b_word_wrap = false;
  std::function<std::string()> fn_get_text;
  std::function<core::Color()> fn_get_color;
  std::function<core::Color()> fn_get_contrast_color_dark;
  std::function<core::Color()> fn_get_contrast_color_light;

  void IncreaseHeightToFitCurrentText();
  void Draw() override;

  void BindStandardDelegates();

  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

  const std::string* GetCursor(int2) const override { return nullptr; }

 protected:

  virtual void DrawInner(const std::string& str_text, gfx::SpriteFont& font_text,
                         core::Color color_c, int2 int2_position);

 private:
  core::CachedTransform<std::string, std::string> cache_text_;
};

}
