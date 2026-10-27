#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "core/wpos.hpp"
#include "gfx/projected_cell_region.hpp"
#include "gfx/world_renderer.hpp"

namespace ora::gfx {

enum class WorldViewport : std::uint8_t { Native, Close, Medium, Far };

struct INotifyViewportZoomExtentsChanged {
  virtual void ViewportZoomExtentsChanged(float fp4_min_zoom, float fp4_max_zoom) = 0;
  virtual ~INotifyViewportZoomExtentsChanged() = default;
};

struct IViewportHostRenderer {
  virtual int2 NativeResolution() const = 0;
  virtual void SetMaximumViewportSize(int2 int2_size) = 0;
  virtual ~IViewportHostRenderer() = default;
};

struct GraphicSettingsFace {
  WorldViewport kind_viewport_distance = WorldViewport::Medium;
  float fp4_ui_scale = 1.0f;
};

struct WorldViewportSizes {
  int2 int2_close_window_heights{480, 600};
  int2 int2_medium_window_heights{600, 900};
  int2 int2_far_window_heights{900, 1300};

  float fp4_default_scale = 1.0f;
  float fp4_max_zoom_scale = 2.0f;
  std::int32_t int4_max_zoom_window_height = 240;
  bool b_allow_native_zoom = true;
  int2 int2_min_effective_resolution{1024, 720};

  int2 GetSizeRange(WorldViewport kind_distance) const {
    return kind_distance == WorldViewport::Close ? int2_close_window_heights
           : kind_distance == WorldViewport::Medium ? int2_medium_window_heights
                                                    : int2_far_window_heights;
  }
};

WorldViewportSizes& GlobalViewportSizes();

class Viewport : public IViewportSurface {
 public:

  struct Deps {
    WPos wpos_projected_top_left{};
    WPos wpos_projected_bottom_right{};
    int2 int2_map_size{0, 0};
    std::int32_t int4_maximum_terrain_height = 0;

    std::function<PPos(PPos)> fn_clamp_ppos;

    std::function<MPos(MPos)> fn_height_clamp_mpos;

    std::function<CPos(const WPos&)> fn_cell_containing;
  };

  Viewport(WorldRenderer& wr_render, const Deps& deps_map, IViewportHostRenderer& host_renderer,
           const GraphicSettingsFace& settings_graphics,
           const WorldViewportSizes& sizes_viewport);

  core::Vector2 CenterLocation() const { return vec_center_location_; }
  WPos CenterPosition() const;
  int2 TopLeft() override;
  int2 BottomRight() override;
  int2 ViewportSize() override { return int2_viewport_size_; }
  float Zoom() const override { return fp4_zoom_; }
  float MinZoom() const override { return fp4_min_zoom_; }
  float MaxZoom() const { return fp4_max_zoom_; }

  void OverrideDefaultHeight(float fp4_height);

  void AdjustZoom(float fp4_dz);

  void AdjustZoom(float fp4_dz, int2 int2_center);

  void ToggleZoom();

  void UnlockMinimumZoom(float fp4_scale);

  static std::int64_t int8_last_move_run_time;
  static int2 int2_last_mouse_pos;

  ScrollDirection GetBlockedDirections() const;

  void Tick();

  std::function<core::Vector2()> fn_viewport_center_provider;

  std::uint64_t SubscribeViewportTick(std::function<void()> fn_listener);
  void UnsubscribeViewportTick(std::uint64_t uint8_token);

  CPos ViewToWorld(int2 int2_view) override;

  int2 ViewToWorldPx(int2 int2_view);

  int2 WorldToViewPx(int2 int2_world) override;
  int2 WorldToViewPx(const core::Vector3& vec_world) override;

  void Center(const WPos& wpos_pos) override;
  void Center(core::Vector2 vec_pos) override;

  void Scroll(core::Vector2 vec_delta, bool b_ignore_borders);

  Rectangle GetScissorBounds(bool b_inside_bounds) override;

  const ProjectedCellRegion& VisibleCellsInsideBounds();
  const ProjectedCellRegion& AllVisibleCells();

  float CalculateMinimumZoomForTest(float fp4_min_height, float fp4_max_height) const {
    return CalculateMinimumZoom(ptr_host_renderer_->NativeResolution().Y, fp4_min_height,
                                fp4_max_height);
  }
  int2 TopLeftPxForTest() const { return TopLeftPx(); }
  std::vector<MPos> CandidateMouseoverCellsForTest(int2 int2_world) const {
    return CandidateMouseoverCells(int2_world);
  }
  const Deps& deps_for_test() const { return deps_map_; }
  void SetZoomExtentsListeners(std::span<INotifyViewportZoomExtentsChanged* const> vec_listeners) {
    vec_zoom_extents_listeners_.assign(vec_listeners.begin(), vec_listeners.end());
  }

 private:

  void SetZoom(float fp4_value);

  static float CalculateMinimumZoom(std::int32_t int4_native_height, float fp4_min_height,
                                    float fp4_max_height);

  void UpdateViewportZooms(bool b_reset_current_zoom = true);

  std::vector<MPos> CandidateMouseoverCells(int2 int2_world) const;

  ProjectedCellRegion CalculateVisibleCells(bool b_inside_bounds) const;

  int2 TopLeftPx() const;
  int2 BottomRightPx() const;

  WorldRenderer& wr_;
  const WorldViewportSizes* ptr_sizes_;
  GraphicSettingsFace settings_graphics_;

  Rectangle rect_map_bounds_;
  int2 int2_tile_size_;

  core::Vector2 vec_center_location_{};
  int2 int2_viewport_size_{};

  bool b_cells_dirty_ = true;
  bool b_all_cells_dirty_ = true;
  ProjectedCellRegion region_cells_{};
  ProjectedCellRegion region_all_cells_{};

  WorldViewport kind_last_viewport_distance_;
  float fp4_zoom_ = 1.0f;
  bool b_unlock_min_zoom_ = false;
  float fp4_unlocked_min_zoom_scale_ = 0.0f;
  float fp4_unlocked_min_zoom_ = 1.0f;
  float fp4_default_scale_ = 1.0f;
  bool b_override_user_scale_ = false;

  float fp4_min_zoom_ = 1.0f;
  float fp4_max_zoom_ = 2.0f;

  Deps deps_map_;
  IViewportHostRenderer* ptr_host_renderer_;

  std::vector<INotifyViewportZoomExtentsChanged*> vec_zoom_extents_listeners_;
  std::map<std::uint64_t, std::function<void()>> map_viewport_tick_;
  std::uint64_t uint8_next_tick_token_ = 1;
};

}
