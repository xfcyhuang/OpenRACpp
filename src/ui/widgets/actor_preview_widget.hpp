#pragma once
import std;

#include "gfx/actor_preview.hpp"
#include "gfx/renderable.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::sim {
class TypeDictionary;
}

namespace ora::ui {

class ActorPreviewWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ActorPreviewWidget";

  explicit ActorPreviewWidget(WidgetArgs& args_args);
  explicit ActorPreviewWidget(const ActorPreviewWidget& widget_other);
  ~ActorPreviewWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ActorPreviewWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void SetPreview(const game::ActorInfo& actor, sim::TypeDictionary& dict);

  void PrepareRenderables() override;
  void Draw() override;
  void Tick() override;

  bool b_animate = false;
  float fp4_scale = 1.0f;

  int2 PreviewOffset() const { return int2_preview_offset_; }
  int2 IdealPreviewSize() const { return int2_ideal_preview_size_; }

 private:
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  std::vector<std::unique_ptr<gfx::IActorPreview>> vec_preview_;
  int2 int2_preview_offset_{0, 0};
  int2 int2_ideal_preview_size_{0, 0};
  std::vector<gfx::RenderItem> vec_renderables_;
};

}
