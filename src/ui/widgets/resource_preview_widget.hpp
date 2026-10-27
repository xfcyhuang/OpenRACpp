#pragma once
import std;

#include "core/size.hpp"
#include "gfx/renderable.hpp"
#include "sim/trait_interfaces.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {
class World;
class IResourceRenderer;
}

namespace ora::ui {

class ResourcePreviewWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ResourcePreviewWidget";

  explicit ResourcePreviewWidget(WidgetArgs& args_args);
  explicit ResourcePreviewWidget(const ResourcePreviewWidget& widget_other);
  ~ResourcePreviewWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ResourcePreviewWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void SetResourceType(std::string str_value);
  void Draw() override;

  float fp4_scale = 1.0f;

  Size IdealPreviewSize() const { return size_ideal_preview_; }

 private:
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  sim::World* ptr_world_ = nullptr;
  std::vector<sim::IResourceRenderer*> vec_resource_renderers_;
  Size size_tile_{0, 0};

  std::string str_resource_type_;
  sim::IResourceRenderer* ptr_resource_renderer_ = nullptr;
  int2 int2_sprite_offset_{0, 0};
  Size size_ideal_preview_{0, 0};
};

}
