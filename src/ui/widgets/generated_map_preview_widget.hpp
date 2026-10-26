// UPSTREAM: OpenRA.Mods.Common/Widgets/GeneratedMapPreviewWidget.cs @1f594e320
// L24-168(全文;自有 Sheet 的预览位图 + 出生点标签对)。
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class SpriteFont;
}
namespace ora::fmt {
class Png;
}
namespace ora::map {
class Map;
class MapPreview;
}

namespace ora::ui {

class GeneratedMapPreviewWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "GeneratedMapPreviewWidget";

  GeneratedMapPreviewWidget();
  explicit GeneratedMapPreviewWidget(const GeneratedMapPreviewWidget& widget_other);
  ~GeneratedMapPreviewWidget() override;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new GeneratedMapPreviewWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool b_show_spawn_points = true;

  void Update(map::MapPreview& map_preview);
  void Update(map::Map& map_world, bool b_force_preview);

  void Clear() { sprite_map_ = gfx::Sprite{}; }

  int2 ConvertToPreview(CPos cell, Rectangle rect_bounds, MapGridType grid_type,
                        float fp4_preview_scale);

  void Draw() override;
  void Removed() override;

  bool Loaded() const { return sprite_map_.ptr_sheet != nullptr; }
  Rectangle MapRect() const { return rect_map_; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 private:
  void Update(std::vector<CPos> vec_spawn_points, Rectangle rect_bounds,
              MapGridType grid_type, const fmt::Png& png_preview);

  gfx::Sprite sprite_spawn_unclaimed_;
  gfx::SpriteFont* ptr_spawn_font_ = nullptr;
  core::Color color_spawn_{};
  core::Color color_spawn_contrast{};
  int2 int2_spawn_label_offset{};

  std::unique_ptr<gfx::Sheet> up_map_sheet_;
  gfx::Sprite sprite_map_;
  Rectangle rect_map_{};
  std::vector<std::tuple<int2, std::string, int2>> vec_spawns_;
};

}  // namespace ora::ui
