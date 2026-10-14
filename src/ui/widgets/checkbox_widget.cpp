// UPSTREAM: OpenRA.Mods.Common/Widgets/CheckboxWidget.cs @e180e3225
// (checkbox_widget.hpp 的实现)
// The implementation of checkbox_widget.hpp.
import std;

#include "ui/widgets/checkbox_widget.hpp"

#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {
namespace {

gfx::SpriteFont& RequireFont(const std::string& str_font) {
  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  gfx::SpriteFont* ptr_font =
      ptr_renderer != nullptr ? ptr_renderer->TryGetFont(str_font) : nullptr;
  if (ptr_font == nullptr)
    throw std::runtime_error("Requested font '" + str_font + "' was not found.");
  return *ptr_font;
}

}  // namespace

CheckboxWidget::CheckboxWidget(WidgetArgs& args_args)
    : ButtonWidget(args_args),
      cache_checkmark_([](const std::pair<std::string, bool>& pair_args) {
        const std::string str_variant_image_collection = "checkmark-" + pair_args.first;
        const std::string str_variant_base_name = pair_args.second ? "checked" : "unchecked";
        return GetCachedStatefulImage(str_variant_image_collection, str_variant_base_name);
      }) {
  ChromeMetrics& metrics = RequireChromeMetrics();
  color_text = metrics.GetColor("TextColor");
  color_text_disabled = metrics.GetColor("TextDisabledColor");
  fn_get_color = [this] { return color_text; };
  fn_get_color_disabled = [this] { return color_text_disabled; };
  fn_get_checkmark = [this] { return str_checkmark; };
}

CheckboxWidget::CheckboxWidget(const CheckboxWidget& widget_other)
    : ButtonWidget(widget_other),
      str_background(widget_other.str_background),
      str_checkmark(widget_other.str_checkmark),
      fn_get_checkmark(widget_other.fn_get_checkmark),
      fn_is_checked(widget_other.fn_is_checked),
      cache_checkmark_([](const std::pair<std::string, bool>& pair_args) {
        const std::string str_variant_image_collection = "checkmark-" + pair_args.first;
        const std::string str_variant_base_name = pair_args.second ? "checked" : "unchecked";
        return GetCachedStatefulImage(str_variant_image_collection, str_variant_base_name);
      }) {}

void CheckboxWidget::Draw() {
  const bool b_disabled = fn_is_disabled();
  gfx::SpriteFont& font_text = RequireFont(str_font);
  const bool b_hover = Ui::Instance().MouseOverWidget() == this;
  const core::Color color_drawn = fn_get_color();
  const core::Color color_drawn_disabled = fn_get_color_disabled();
  const std::string str_text_drawn = fn_get_text();
  const Rectangle rect_bounds{RenderBounds().TopLeft().X, RenderBounds().TopLeft().Y,
                              bounds_widget.int4_height, bounds_widget.int4_height};

  ButtonWidget::DrawBackground(str_background, rect_bounds, b_disabled, b_depressed, b_hover,
                                fn_is_highlighted());

  const core::Vector2 vec_text_position{
      static_cast<float>(RenderBounds().Left()) +
          static_cast<float>(RenderBounds().Height) * 1.5f,
      static_cast<float>(RenderOrigin().Y) +
          static_cast<float>(bounds_widget.int4_height -
                             font_text.Measure(str_text_drawn).Y - font_text.TopOffset()) /
              2.0f};
  if (b_contrast)
    font_text.DrawTextWithContrast(str_text_drawn, vec_text_position,
                                   b_disabled ? color_drawn_disabled : color_drawn,
                                   fn_get_contrast_color_dark(), fn_get_contrast_color_light(),
                                   2);
  else
    font_text.DrawText(str_text_drawn, vec_text_position,
                       b_disabled ? color_drawn_disabled : color_drawn);

  const gfx::Sprite& sprite_checkmark =
      cache_checkmark_
          .Update(std::make_pair(fn_get_checkmark(), fn_is_checked()))
          .Update(StatefulImageKey{b_disabled, b_depressed, b_hover, false,
                                   fn_is_highlighted()});
  DrawSprite(sprite_checkmark,
             core::Vector2{
                 static_cast<float>(rect_bounds.Right()) -
                     static_cast<int>((rect_bounds.Height + sprite_checkmark.vec_size.X) / 2),
                 static_cast<float>(rect_bounds.Top()) +
                     static_cast<int>((rect_bounds.Height - sprite_checkmark.vec_size.Y) / 2)});
}

bool CheckboxWidget::LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) {
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }
  if (str_key == "Checkmark") {
    str_checkmark = std::string{str_value};
    return true;
  }

  return ButtonWidget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
