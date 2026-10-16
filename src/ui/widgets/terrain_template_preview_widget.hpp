#pragma once
import std;

#include "sim/trait_interfaces.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {
class World;
class ITiledTerrainRenderer;
}

namespace ora::terrain {
class TerrainTemplateInfo;
}

namespace ora::ui {

class TerrainTemplatePreviewWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "TerrainTemplatePreviewWidget";

  explicit TerrainTemplatePreviewWidget(WidgetArgs& args_args);
  explicit TerrainTemplatePreviewWidget(
      const TerrainTemplatePreviewWidget& widget_other);
  ~TerrainTemplatePreviewWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new TerrainTemplatePreviewWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void SetTemplate(const terrain::TerrainTemplateInfo& template_info);
  void Draw() override;

  float fp4_scale = 1.0f;

  int2 PreviewPos() const { return int2_preview_pos_; }
  int2 IdealPreviewSize() const { return int2_ideal_preview_size_; }

 private:
  sim::ITiledTerrainRenderer* ptr_terrain_renderer_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  const terrain::TerrainTemplateInfo* ptr_template_ = nullptr;
  int2 int2_preview_pos_{0, 0};
  int2 int2_ideal_preview_size_{0, 0};
};

}  // namespace ora::ui
