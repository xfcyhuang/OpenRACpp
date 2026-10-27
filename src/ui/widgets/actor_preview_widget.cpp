import std;

#include "ui/widgets/actor_preview_widget.hpp"

#include "gfx/renderable.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/type_dictionary.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

ActorPreviewWidget::ActorPreviewWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr;
}

ActorPreviewWidget::ActorPreviewWidget(const ActorPreviewWidget& widget_other)
    : Widget(widget_other),
      ptr_world_renderer_(widget_other.ptr_world_renderer_) {}

void ActorPreviewWidget::SetPreview(const game::ActorInfo& actor,
                                    sim::TypeDictionary& dict) {
  gfx::WorldViewportSizes& sizes = gfx::GlobalViewportSizes();
  gfx::ActorPreviewInitializer init(actor, *ptr_world_renderer_, dict);
  vec_preview_ = gfx::RenderActorPreviewOf(actor, init);

  std::vector<Rectangle> vec_rects;
  for (const auto& preview : vec_preview_)
    preview->ScreenBounds(*ptr_world_renderer_, WPos::Zero(), vec_rects);

  Rectangle bounds = Rectangle::Empty();
  bool first = true;
  for (const Rectangle& rect : vec_rects) {
    bounds = first ? rect : Rectangle::Union(bounds, rect);
    first = false;
  }

  int2_ideal_preview_size_ = int2{
      static_cast<std::int32_t>(bounds.Width * sizes.fp4_default_scale),
      static_cast<std::int32_t>(bounds.Height * sizes.fp4_default_scale)};
  const int2 int2_preview_top_left{
      static_cast<std::int32_t>(bounds.Left() * sizes.fp4_default_scale),
      static_cast<std::int32_t>(bounds.Top() * sizes.fp4_default_scale)};
  int2_preview_offset_ = int2_preview_top_left + int2_ideal_preview_size_ / 2;
}

void ActorPreviewWidget::PrepareRenderables() {
  gfx::WorldViewportSizes& sizes = gfx::GlobalViewportSizes();
  const float scale = fp4_scale * sizes.fp4_default_scale;
  const Rectangle rb = RenderBounds();
  const int2 int2_origin =
      RenderOrigin() - int2{
          static_cast<std::int32_t>(int2_preview_offset_.X * scale -
                                    rb.Width / 2),
          static_cast<std::int32_t>(int2_preview_offset_.Y * scale -
                                    rb.Height / 2)};

  vec_renderables_.clear();
  for (const auto& preview : vec_preview_)
    preview->RenderUI(*ptr_world_renderer_, int2_origin, scale,
                      vec_renderables_);

  std::stable_sort(vec_renderables_.begin(), vec_renderables_.end(),
                   [](const gfx::RenderItem& item_a,
                      const gfx::RenderItem& item_b) {
                     return gfx::RenderableZSortKey(item_a) <
                            gfx::RenderableZSortKey(item_b);
                   });
}

void ActorPreviewWidget::Draw() {
  gfx::Renderer& renderer = RequireRenderer();
  renderer.EnableAntialiasingFilter();
  for (const gfx::RenderItem& item : vec_renderables_)
    gfx::RenderRenderable(item, *ptr_world_renderer_);
  renderer.DisableAntialiasingFilter();
}

void ActorPreviewWidget::Tick() {
  if (!b_animate)
    return;
  for (const auto& preview : vec_preview_)
    preview->Tick();
}

}
