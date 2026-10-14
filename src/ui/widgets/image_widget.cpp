// UPSTREAM: OpenRA.Mods.Common/Widgets/ImageWidget.cs + SpriteWidget.cs
// @e180e3225(image_widget.hpp 的实现)
// The implementation of image_widget.hpp.
import std;

#include "ui/widgets/image_widget.hpp"

#include "fluent/fluent_provider.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

ImageWidget::ImageWidget() {
  BindDefaults();
}

ImageWidget::ImageWidget(const ImageWidget& widget_other)
    : Widget(widget_other),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_tooltip_container(widget_other.str_tooltip_container),
      str_image_collection(widget_other.str_image_collection),
      str_image_name(widget_other.str_image_name),
      b_click_through(widget_other.b_click_through),
      fn_get_image_name(widget_other.fn_get_image_name),
      fn_get_image_collection(widget_other.fn_get_image_collection),
      opt_tooltip_text(widget_other.opt_tooltip_text) {
  BindDefaults();
}

void ImageWidget::BindDefaults() {
  fn_get_image_name = [this] { return str_image_name; };
  fn_get_image_collection = [this] { return str_image_collection; };
  cache_tooltip_ = std::make_unique<core::CachedTransform<std::string, std::string>>(
      [](const std::string& str_s) {
        return !str_s.empty() ? fluent::FluentProvider::Instance().GetMessage(str_s)
                              : std::string{};
      });
  fn_get_tooltip_text = [this] { return cache_tooltip_->Update(opt_tooltip_text.value_or("")); };
  cache_image_ =
      std::make_unique<core::CachedTransform<std::pair<std::string, std::string>, gfx::Sprite>>(
          [](const std::pair<std::string, std::string>& pair_args) -> gfx::Sprite {
            gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
            if (ptr_chrome == nullptr)
              return gfx::Sprite{};
            return ptr_chrome->GetImage(pair_args.first, pair_args.second);
          });
  fn_get_sprite = [this] {
    return cache_image_->Update(std::make_pair(fn_get_image_collection(), fn_get_image_name()));
  };
  b_tooltip_resolved_ = false;
  ptr_tooltip_container_cached_ = nullptr;
}

void ImageWidget::Draw() {
  const gfx::Sprite sprite_s = fn_get_sprite();
  const int2 int2_origin = RenderOrigin();
  DrawSprite(sprite_s,
             core::Vector2{static_cast<float>(int2_origin.X), static_cast<float>(int2_origin.Y)});
}

bool ImageWidget::HandleMouseInput(const MouseInput& mi_input) {
  return !b_click_through && RenderBounds().Contains(mi_input.Location);
}

TooltipContainerWidget* ImageWidget::TooltipContainerOrNull() {
  if (b_tooltip_resolved_)
    return ptr_tooltip_container_cached_;

  b_tooltip_resolved_ = true;
  if (Ui::HasInstance() && Ui::Instance().Root() != nullptr && !str_tooltip_container.empty())
    ptr_tooltip_container_cached_ =
        Ui::Instance().Root()->GetOrNull<TooltipContainerWidget>(str_tooltip_container);
  return ptr_tooltip_container_cached_;
}

void ImageWidget::MouseEntered() {
  if (str_tooltip_container.empty() || !fn_get_tooltip_text)
    return;

  if (TooltipContainerWidget* ptr_container = TooltipContainerOrNull()) {
    WidgetArgs args_tooltip;
    args_tooltip.Add("getText", fn_get_tooltip_text);
    ptr_container->SetTooltip(str_tooltip_template, std::move(args_tooltip));
  }
}

void ImageWidget::MouseExited() {
  if (str_tooltip_container.empty())
    return;

  if (TooltipContainerWidget* ptr_container = TooltipContainerOrNull())
    ptr_container->RemoveTooltip();
}

bool ImageWidget::LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) {
  if (str_key == "TooltipTemplate") {
    str_tooltip_template = std::string{str_value};
    return true;
  }
  if (str_key == "TooltipContainer") {
    str_tooltip_container = std::string{str_value};
    return true;
  }
  if (str_key == "ImageCollection") {
    str_image_collection = std::string{str_value};
    return true;
  }
  if (str_key == "ImageName") {
    str_image_name = std::string{str_value};
    return true;
  }
  if (str_key == "ClickThrough") {
    b_click_through = meta::GetBoolValue("ClickThrough", str_value);
    return true;
  }
  if (str_key == "TooltipText") {
    opt_tooltip_text = std::string{str_value};
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

SpriteWidget::SpriteWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer = *ptr_wr;

  fn_get_palette = [this] { return str_palette; };
  fn_get_scale = [this] { return fp4_scale; };
}

SpriteWidget::SpriteWidget(const SpriteWidget& widget_other)
    : Widget(widget_other),
      fp4_scale(widget_other.fp4_scale),
      str_palette(widget_other.str_palette),
      fn_get_palette(widget_other.fn_get_palette),
      fn_get_sprite(widget_other.fn_get_sprite),
      ptr_world_renderer(widget_other.ptr_world_renderer) {}

void SpriteWidget::Draw() {
  const gfx::Sprite sprite_s = fn_get_sprite();
  const std::string str_palette_name = fn_get_palette();
  const float fp4_scale_value = fn_get_scale();

  if (sprite_s.ptr_sheet == nullptr || str_palette_name.empty())
    return;

  if (!b_has_cached_sprite_ || sprite_s.ptr_sheet != sprite_cached_.ptr_sheet ||
      sprite_s.Bounds != sprite_cached_.Bounds || fp4_scale_value != fp4_scale_cached_) {
    vec_offset_ = core::Vector2{
        0.5f * (static_cast<float>(bounds_widget.int4_width) -
                fp4_scale_value * sprite_s.vec_size.X),
        0.5f * (static_cast<float>(bounds_widget.int4_height) -
                fp4_scale_value * sprite_s.vec_size.Y)};
    sprite_cached_ = sprite_s;
    b_has_cached_sprite_ = true;
    fp4_scale_cached_ = fp4_scale_value;
  }

  if (str_palette_name != str_palette_cached_) {
    ptr_palette_ref_ =
        ptr_world_renderer != nullptr ? ptr_world_renderer->Palette(str_palette_name)
                                      : nullptr;
    str_palette_cached_ = str_palette_name;
  }

  gfx::Renderer& renderer_ui = RequireRenderer();
  renderer_ui.EnableAntialiasingFilter();
  const int2 int2_origin = RenderOrigin();
  renderer_ui.UISpriteRenderer().DrawSprite(
      sprite_s, ptr_palette_ref_,
      core::Vector3{static_cast<float>(int2_origin.X) + vec_offset_.X,
                    static_cast<float>(int2_origin.Y) + vec_offset_.Y, 0.0f},
      fp4_scale_value);
  renderer_ui.DisableAntialiasingFilter();
}

bool SpriteWidget::LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) {
  if (str_key == "Scale") {
    fp4_scale = meta::GetFloatValue("Scale", str_value);
    return true;
  }
  if (str_key == "Palette") {
    str_palette = std::string{str_value};
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

const bool kWidgetRegistered_SpriteWidget = [] {
  WidgetTypeRegistry::Instance().Register(
      "SpriteWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<SpriteWidget>(args_args);
      });
  return true;
}();

}  // namespace ora::ui
