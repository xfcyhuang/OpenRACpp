#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/sprite.hpp"
#include "ui/widget.hpp"

namespace ora::map {
class MapPreview;
}
namespace ora::net {
struct SessionClient;
}
namespace ora::server {
struct GameInformationPlayer;
struct GameClientData;
}
namespace ora::gfx {
class SpriteFont;
}

namespace ora::ui {

class TooltipContainerWidget;

class MapPreviewWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "MapPreviewWidget";

  MapPreviewWidget();
  explicit MapPreviewWidget(const MapPreviewWidget& widget_other);
  ~MapPreviewWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new MapPreviewWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool b_ignore_mouse_input = false;
  bool b_show_spawn_points = true;

  std::string str_tooltip_container;
  std::string str_tooltip_template = "SPAWN_TOOLTIP";

  std::function<map::MapPreview*()> fn_preview = [] { return nullptr; };
  std::function<std::map<int, SpawnOccupant>()> fn_spawn_occupants = [] {
    return std::map<int, SpawnOccupant>{};
  };
  std::function<std::vector<int>()> fn_disabled_spawn_points = [] {
    return std::vector<int>{};
  };
  std::function<void(const MouseInput&)> fn_on_mouse_down = [](const MouseInput&) {};
  int int4_tooltip_spawn_index = -1;
  bool b_show_unoccupied_spawnpoints = true;

  bool HandleMouseInput(const MouseInput& mi_input) override;
  void MouseEntered() override;
  void MouseExited() override;

  int2 ConvertToPreview(CPos cell, MapGridType grid_type);

  void UpdateMinimap(const gfx::Sprite& sprite_minimap);
  void Draw() override;

  bool Loaded() const { return sprite_minimap_.ptr_sheet != nullptr; }
  Rectangle MapRect() const { return rect_map_; }
  float PreviewScale() const { return fp4_preview_scale_; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 private:
  TooltipContainerWidget& TooltipContainerValue();

  bool b_tooltip_container_created_ = false;
  TooltipContainerWidget* ptr_tooltip_container_ = nullptr;

  gfx::Sprite sprite_spawn_claimed_;
  gfx::Sprite sprite_spawn_unclaimed_;
  gfx::Sprite sprite_spawn_disabled_;
  gfx::SpriteFont* ptr_spawn_font_ = nullptr;
  core::Color color_spawn_{};
  core::Color color_spawn_contrast{};
  int2 int2_spawn_label_offset{};

  Rectangle rect_map_{};
  float fp4_preview_scale_ = 0;
  gfx::Sprite sprite_minimap_;
};

}
