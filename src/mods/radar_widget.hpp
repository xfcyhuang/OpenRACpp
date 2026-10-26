// UPSTREAM: OpenRA.Mods.Common/Widgets/RadarWidget.cs @1f594e320
//          L25-539 全文(四层 2x2 单 sheet 的雷达 minimap:地形/迷雾/actor
//          三层 + 视口框与 ping 星;开关动画与 Classic 的 minimap 移动)
//          The whole of RadarWidget.cs L25-539 (the radar minimap on a
//          single 2x2 four-layer sheet: terrain/shroud/actor plus the
//          viewport rect and ping stars; the toggle animation and Classic's
//          minimap movement).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "game/input.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite.hpp"
#include "mods/radar.hpp"
#include "sim/trait_interfaces.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::game {
class ModData;
}

namespace ora::mods {

class RadarWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "RadarWidget";

  explicit RadarWidget(ora::ui::WidgetArgs& args_args);
  RadarWidget(const RadarWidget& widget_other);
  ~RadarWidget() override;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(new RadarWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;
  void Initialize(ora::ui::WidgetArgs& args_args) override;

  void Tick() override;
  bool HandleMouseInput(const ora::MouseInput& mi_input) override;
  const std::string* GetCursor(int2 pt_pos) const override;
  void Draw() override;
  void Removed() override;

  std::uint32_t uint4_color_fog = 0x80000000u;  // L27(128-alpha 黑)
  std::uint32_t uint4_color_shroud = 0xFF000000u;  // L28

  std::string str_world_interaction_controller;  // L30
  int int4_animation_length = 5;                 // L31
  std::string str_radar_online_sound;            // L32
  std::string str_radar_offline_sound;           // L33
  std::string str_sound_up;                      // L34
  std::string str_sound_down;                    // L35
  std::function<bool()> fn_is_enabled = [] { return true; };
  std::function<void()> fn_after_open = [] {};
  std::function<void()> fn_after_close = [] {};
  std::function<void(float)> fn_animating = [](float) {};

  const Rectangle& MapRectForTest() const { return rect_map_; }
  float PreviewScaleForTest() const { return fp4_preview_scale_; }
  int2 PreviewOriginForTest() const { return int2_preview_origin_; }
  int2 CellToMinimapPixel(CPos p) const;
  core::Vector2 MinimapPixelToWorldCoords(int2 pixel) const;
  const gfx::Sheet& RadarSheetForTest() const { return *ptr_radar_sheet_; }
  std::span<std::byte> RadarDataForTest() { return ptr_radar_sheet_->GetData(); }
  bool HasRadarForTest() const { return b_has_radar_; }

 private:
  void MapBoundsChanged();
  void UpdateTerrainColor(MPos uv);
  void UpdateShroudCell(PPos puv);
  void SetPlayer(sim::Player* ptr_player, bool b_force_update = false);
  void WorldOnRenderPlayerChanged(sim::Player* ptr_player);
  void DrawRadarPings();

  game::ModData* ptr_mod_data_ = nullptr;
  sim::World* ptr_world_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  RadarPings* ptr_radar_pings_ = nullptr;
  std::vector<sim::IRadarTerrainLayer*> vec_radar_terrain_layers_;
  bool b_is_rectangular_isometric_ = false;
  int int4_cell_width_ = 1;
  int int4_preview_width_ = 0;
  int int4_preview_height_ = 0;
  std::string str_world_select_cursor_{"select"};
  std::string str_world_default_cursor_{"default"};
  GameSettingsFace game_settings_{};

  float fp4_radar_minimap_height_ = 0.0f;
  int int4_frame_ = 0;
  bool b_has_radar_ = false;
  bool b_cached_enabled_ = false;
  bool b_is_minimap_moving_ = false;
  MouseButton kind_minimap_move_button_ = MouseButton::None;

  float fp4_preview_scale_ = 0.0f;
  int2 int2_preview_origin_{0, 0};
  Rectangle rect_map_ = Rectangle::Empty();

  std::unique_ptr<gfx::Sheet> ptr_radar_sheet_;
  std::span<std::byte> vec_radar_data_;

  gfx::Sprite sprite_terrain_;
  gfx::Sprite sprite_actor_;
  gfx::Sprite sprite_shroud_;
  sim::Shroud* ptr_shroud_ = nullptr;
  std::optional<std::size_t> n_shroud_token_;
  PlayerRadarTerrain* ptr_player_radar_ = nullptr;
  std::optional<std::size_t> n_player_radar_terrain_token_;
  std::optional<std::size_t> n_map_tiles_token_;
  std::vector<std::size_t> vec_rtl_tokens_;
  sim::Player* ptr_current_player_ = nullptr;
};

void RegisterRadarWidget();

}  // namespace ora::mods
