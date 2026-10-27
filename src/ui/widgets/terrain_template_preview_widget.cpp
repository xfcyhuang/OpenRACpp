import std;

#include "ui/widgets/terrain_template_preview_widget.hpp"

#include "gfx/renderable.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/actor.hpp"
#include "sim/world.hpp"
#include "terrain/terrain_info.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::ui {

TerrainTemplatePreviewWidget::TerrainTemplatePreviewWidget(
    WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr;
  if (const WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value)) {
      sim::Actor* ptr_world_actor = (*ptr_world_arg)->WorldActor();
      ptr_terrain_renderer_ =
          ptr_world_actor != nullptr
              ? ptr_world_actor->TraitOrDefault<sim::ITiledTerrainRenderer>()
              : nullptr;
    }

  if (ptr_terrain_renderer_ == nullptr)
    throw yaml::YamlException(
        "TerrainTemplatePreviewWidget requires a tile-based terrain renderer.");
}

TerrainTemplatePreviewWidget::TerrainTemplatePreviewWidget(
    const TerrainTemplatePreviewWidget& widget_other)
    : Widget(widget_other),
      ptr_terrain_renderer_(widget_other.ptr_terrain_renderer_),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      ptr_template_(widget_other.ptr_template_),
      int2_preview_pos_(widget_other.int2_preview_pos_),
      int2_ideal_preview_size_(widget_other.int2_ideal_preview_size_) {}

void TerrainTemplatePreviewWidget::SetTemplate(
    const terrain::TerrainTemplateInfo& template_info) {
  ptr_template_ = &template_info;
  const Rectangle b = ptr_terrain_renderer_->TemplateBounds(template_info);
  const float scale_default = gfx::GlobalViewportSizes().fp4_default_scale;
  int2_ideal_preview_size_ =
      int2{static_cast<std::int32_t>(b.Width * scale_default),
           static_cast<std::int32_t>(b.Height * scale_default)};
  int2_preview_pos_ =
      int2{static_cast<std::int32_t>(b.Left() * scale_default),
           static_cast<std::int32_t>(b.Top() * scale_default)};
}

void TerrainTemplatePreviewWidget::Draw() {
  if (ptr_template_ == nullptr)
    return;

  const float scale = fp4_scale * gfx::GlobalViewportSizes().fp4_default_scale;
  const int2 int2_origin =
      RenderOrigin() - int2{static_cast<std::int32_t>(int2_preview_pos_.X * scale),
                            static_cast<std::int32_t>(int2_preview_pos_.Y * scale)};

  std::vector<gfx::RenderItem> vec_items;
  ptr_terrain_renderer_->RenderUIPreview(*ptr_world_renderer_, *ptr_template_,
                                         int2_origin, scale, vec_items);
  for (const gfx::RenderItem& item : vec_items)
    gfx::RenderRenderable(item, *ptr_world_renderer_);
}

}
