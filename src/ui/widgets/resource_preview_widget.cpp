import std;

#include "ui/widgets/resource_preview_widget.hpp"

#include "game/ruleset.hpp"
#include "gfx/renderable.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/actor.hpp"
#include "sim/world.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

ResourcePreviewWidget::ResourcePreviewWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr;
  if (const WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world_arg;

  if (ptr_world_ != nullptr && ptr_world_->MapPtr() != nullptr) {
    sim::Actor* ptr_world_actor = ptr_world_->WorldActor();
    if (ptr_world_actor != nullptr)
      vec_resource_renderers_ =
          ptr_world_actor->TraitsImplementing<sim::IResourceRenderer>();
    const Size size_tile = ptr_world_->Map().Rules().TerrainInfo().TileSize();
    size_tile_ = size_tile;
  }
}

ResourcePreviewWidget::ResourcePreviewWidget(
    const ResourcePreviewWidget& widget_other)
    : Widget(widget_other),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      ptr_world_(widget_other.ptr_world_),
      vec_resource_renderers_(widget_other.vec_resource_renderers_),
      size_tile_(widget_other.size_tile_),
      str_resource_type_(widget_other.str_resource_type_),
      ptr_resource_renderer_(widget_other.ptr_resource_renderer_),
      int2_sprite_offset_(widget_other.int2_sprite_offset_),
      size_ideal_preview_(widget_other.size_ideal_preview_) {}

void ResourcePreviewWidget::SetResourceType(std::string str_value) {
  str_resource_type_ = std::move(str_value);
  ptr_resource_renderer_ = nullptr;
  if (!str_resource_type_.empty())
    for (sim::IResourceRenderer* renderer : vec_resource_renderers_) {
      const std::vector<std::string> vec_types = renderer->ResourceTypes();
      if (std::find(vec_types.begin(), vec_types.end(), str_resource_type_) !=
          vec_types.end()) {
        ptr_resource_renderer_ = renderer;
        break;
      }
    }

  std::optional<Rectangle> opt_bounds;
  if (ptr_resource_renderer_ != nullptr) {
    std::vector<gfx::RenderItem> vec_items;
    ptr_resource_renderer_->RenderUIPreview(*ptr_world_renderer_,
                                            str_resource_type_, int2{0, 0},
                                            gfx::GlobalViewportSizes().fp4_default_scale,
                                            vec_items);
    if (!vec_items.empty())
      opt_bounds = gfx::RenderableScreenBounds(vec_items.front(),
                                               *ptr_world_renderer_);
  }

  if (opt_bounds.has_value()) {
    int2_sprite_offset_ = int2{-opt_bounds->X, -opt_bounds->Y};
    size_ideal_preview_ = Size{opt_bounds->Width, opt_bounds->Height};
  } else {
    size_ideal_preview_ =
        Size{static_cast<std::int32_t>(size_tile_.Width *
                                       gfx::GlobalViewportSizes().fp4_default_scale),
             static_cast<std::int32_t>(size_tile_.Height *
                                       gfx::GlobalViewportSizes().fp4_default_scale)};
  }
}

void ResourcePreviewWidget::Draw() {
  if (ptr_resource_renderer_ == nullptr)
    return;

  const float scale = fp4_scale * gfx::GlobalViewportSizes().fp4_default_scale;
  std::vector<gfx::RenderItem> vec_items;
  ptr_resource_renderer_->RenderUIPreview(
      *ptr_world_renderer_, str_resource_type_,
      RenderOrigin() + int2_sprite_offset_, scale, vec_items);
  for (const gfx::RenderItem& item : vec_items)
    gfx::RenderRenderable(item, *ptr_world_renderer_);
}

}
