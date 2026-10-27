#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/vector_n.hpp"
#include "gfx/sheet.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class PaletteReference;
class WorldRenderer;
}

namespace ora::ui {

class TooltipContainerWidget;

class ImageWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ImageWidget";

  ImageWidget();
  explicit ImageWidget(const ImageWidget& widget_other);
  ~ImageWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ImageWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_tooltip_template;
  std::string str_tooltip_container;

  std::string str_image_collection = "";
  std::string str_image_name = "";
  bool b_click_through = true;
  std::function<std::string()> fn_get_image_name;
  std::function<std::string()> fn_get_image_collection;
  std::function<gfx::Sprite()> fn_get_sprite;

  std::optional<std::string> opt_tooltip_text;
  std::function<std::string()> fn_get_tooltip_text;

  void Draw() override;
  bool HandleMouseInput(const MouseInput& mi_input) override;
  void MouseEntered() override;
  void MouseExited() override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

 private:
  TooltipContainerWidget* TooltipContainerOrNull();
  void BindDefaults();

  std::unique_ptr<core::CachedTransform<std::pair<std::string, std::string>, gfx::Sprite>>
      cache_image_;
  std::unique_ptr<core::CachedTransform<std::string, std::string>> cache_tooltip_;

  bool b_tooltip_resolved_ = false;
  TooltipContainerWidget* ptr_tooltip_container_cached_ = nullptr;
};

class SpriteWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "SpriteWidget";

  explicit SpriteWidget(WidgetArgs& args_args);
  explicit SpriteWidget(const SpriteWidget& widget_other);
  ~SpriteWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new SpriteWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  float fp4_scale = 1.0f;
  std::function<float()> fn_get_scale;
  std::string str_palette = "chrome";
  std::function<std::string()> fn_get_palette;
  std::function<gfx::Sprite()> fn_get_sprite;

  gfx::WorldRenderer* ptr_world_renderer = nullptr;

  void Draw() override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

 private:
  gfx::Sprite sprite_cached_;
  bool b_has_cached_sprite_ = false;
  std::string str_palette_cached_;
  float fp4_scale_cached_ = 0.0f;
  gfx::PaletteReference* ptr_palette_ref_ = nullptr;
  core::Vector2 vec_offset_{};
};

}
