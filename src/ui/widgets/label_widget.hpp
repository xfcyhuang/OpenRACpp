// UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWidget.cs @e180e3225 L20-133(全文)。
// Text 为 Fluent 键(CachedTransform 空串/空值直通);字体经 Game.Renderer.
// Fonts 注入面(缺名抛 ArgumentException 文本逐字);WordWrap/三态对齐/
// IncreaseHeightToFitCurrentText 保真;DrawInner 虚面供 LabelWithTooltip 等
// 子类(随其批)。ModData ctor 形参 → WidgetArgs 注入(ObjectCreator 等价)。
// The whole of LabelWidget.cs L20-133. Text is a Fluent key (CachedTransform
// passing empty through); the font goes through the Game.Renderer.Fonts
// injected face (a miss throws the ArgumentException text verbatim);
// WordWrap / the three-state alignment / IncreaseHeightToFitCurrentText kept;
// the DrawInner virtual awaits the LabelWithTooltip subclasses. The ModData
// ctor parameter becomes the WidgetArgs injection (the ObjectCreator
// equivalent).
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

/// TextAlign/TextVAlign(LabelWidget.cs L18-19)。
/// TextAlign/TextVAlign (LabelWidget.cs L18-19).
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

  std::optional<std::string> opt_text;  // Text(null 可) | Text (nullable)
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

  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

  const std::string* GetCursor(int2) const override { return nullptr; }

 protected:
  /// DrawInner(L110-121)。
  /// DrawInner (L110-121).
  virtual void DrawInner(const std::string& str_text, gfx::SpriteFont& font_text,
                         core::Color color_c, int2 int2_position);

 private:
  core::CachedTransform<std::string, std::string> cache_text_;
};

}  // namespace ora::ui
