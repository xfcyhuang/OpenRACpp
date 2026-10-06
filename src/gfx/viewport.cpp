// UPSTREAM: OpenRA.Game/Graphics/Viewport.cs @b6fc03f L21-447
// 视口:缩放/居中/滚动、世界-屏幕像素换算、可见投影格区的惰性缓存。
// 注入面(Map/Settings/Renderer,见 viewport.hpp 头注)随 Phase 5 接线。
// The viewport: zoom/centering/scrolling, world-screen pixel conversion,
// and the lazily cached visible projected-cell regions. The injection faces
// (Map/Settings/Renderer — see the viewport.hpp header note) wire up in
// Phase 5.
import std;
#include "gfx/viewport.hpp"

#include "gfx/gfx_util.hpp"
#include "sim/world.hpp"

namespace ora::gfx {

std::int64_t Viewport::int8_last_move_run_time = 0;
int2 Viewport::int2_last_mouse_pos;

Viewport::Viewport(WorldRenderer& wr_render, const Deps& deps_map, IViewportHostRenderer& host_renderer,
                   const GraphicSettingsFace& settings_graphics, const WorldViewportSizes& sizes_viewport)
    : wr_{wr_render},
      ptr_sizes_{&sizes_viewport},
      settings_graphics_{settings_graphics},
      int2_tile_size_{wr_render.TileSize()},
      deps_map_{deps_map},
      ptr_host_renderer_{&host_renderer} {
  // Deps 缺省函数 = 方格网格 / 界内恒真形态(完整 Map 随 Phase 5)
  // Missing Deps functions default to the square-grid / always-in-bounds
  // shapes (the full Map arrives in Phase 5).
  if (deps_map_.fn_clamp_ppos == nullptr)
    deps_map_.fn_clamp_ppos = [](PPos puv_v) { return puv_v; };
  if (deps_map_.fn_height_clamp_mpos == nullptr)
    deps_map_.fn_height_clamp_mpos = [](MPos uv_v) { return uv_v; };
  if (deps_map_.fn_cell_containing == nullptr)
    deps_map_.fn_cell_containing = [](const WPos& wpos_pos) {
      // 方格网格 CellContaining(Map.cs L1054-1055;C# 整除向零截断)
      // The square-grid CellContaining (Map.cs L1054-1055; C# integer
      // division truncates towards zero).
      return CPos{wpos_pos.X / 1024, wpos_pos.Y / 1024};
    };

  fp4_default_scale_ = ptr_sizes_->fp4_default_scale;

  // 编辑器:全图可见,等距高度减半 —— 上游 L161-171
  // Editor: the full map stays visible, isometric height halved — upstream
  // L161-171.
  if (wr_.World().Type() == sim::WorldType::Editor) {
    auto int4_width = deps_map_.int2_map_size.X * int2_tile_size_.X;
    auto int4_height = deps_map_.int2_map_size.Y * int2_tile_size_.Y;
    if (wr_.TerrainSurface().kind_grid_type == MapGridType::RectangularIsometric)
      int4_height /= 2;

    rect_map_bounds_ = Rectangle{0, 0, int4_width, int4_height};
    vec_center_location_ = int2{int4_width / 2, int4_height / 2}.ToVector2();
  } else {
    const int2 tl = wr_.ScreenPxPosition(deps_map_.wpos_projected_top_left);
    const int2 br = wr_.ScreenPxPosition(deps_map_.wpos_projected_bottom_right);
    rect_map_bounds_ = Rectangle::FromLTRB(tl.X, tl.Y, br.X, br.Y);
    // ((tl + br) / 2) 先整数后转浮点 —— 上游 L177
    // ((tl + br) / 2) integers first, then to float — upstream L177.
    vec_center_location_ = ((tl + br) / 2).ToVector2();
  }

  UpdateViewportZooms();
}

WPos Viewport::CenterPosition() const {
  return wr_.ProjectedPosition(int2::FromVector(vec_center_location_));
}

int2 Viewport::TopLeft() {
  return TopLeftPx();
}

int2 Viewport::BottomRight() {
  return BottomRightPx();
}

void Viewport::OverrideDefaultHeight(float fp4_height) {
  fp4_default_scale_ = ptr_sizes_->fp4_default_scale *
                       static_cast<float>(ptr_host_renderer_->NativeResolution().Y) / fp4_height;
  b_override_user_scale_ = true;
  UpdateViewportZooms(false);
}

void Viewport::AdjustZoom(float fp4_dz) {
  // 指数步进使正负步等效 —— 上游注释逐句
  // Exponential stepping equalizes positive/negative steps — the upstream
  // comment verbatim.
  const float fp4_target = fp4_zoom_ * std::exp(fp4_dz);
  const float fp4_lo = b_unlock_min_zoom_ ? fp4_unlocked_min_zoom_ : fp4_min_zoom_;
  SetZoom(std::min(std::max(fp4_target, fp4_lo), fp4_max_zoom_));
}

void Viewport::AdjustZoom(float fp4_dz, int2 int2_center) {
  const int2 int2_old_center = ViewToWorldPx(int2_center);
  AdjustZoom(fp4_dz);
  const int2 int2_new_center = ViewToWorldPx(int2_center);

  // 以屏幕锚点为不动点补偿中心位移
  // Compensates the center shift so the screen anchor stays fixed.
  const core::Vector2 vec_candidate =
      vec_center_location_ + (int2_old_center - int2_new_center).ToVector2();
  vec_center_location_ = rect_map_bounds_.Clamp(vec_candidate);
}

void Viewport::ToggleZoom() {
  // 解锁态恒回默认档;恰在 MinZoom 跳 MaxZoom —— 上游 L120-124
  // An unlocked zoom always resets; exactly at MinZoom jumps to MaxZoom —
  // upstream L120-124.
  if (fp4_zoom_ < fp4_min_zoom_)
    SetZoom(fp4_min_zoom_);
  else
    SetZoom(fp4_zoom_ > fp4_min_zoom_ ? fp4_min_zoom_ : fp4_max_zoom_);
}

void Viewport::UnlockMinimumZoom(float fp4_scale) {
  b_unlock_min_zoom_ = true;
  fp4_unlocked_min_zoom_scale_ = fp4_scale;
  UpdateViewportZooms(false);
}

ScrollDirection Viewport::GetBlockedDirections() const {
  ScrollDirection kind_ret = ScrollDirection::None;
  if (vec_center_location_.Y <= static_cast<float>(rect_map_bounds_.Top()))
    kind_ret = SetDirection(kind_ret, ScrollDirection::Up, true);
  if (vec_center_location_.X <= static_cast<float>(rect_map_bounds_.Left()))
    kind_ret = SetDirection(kind_ret, ScrollDirection::Left, true);
  if (vec_center_location_.Y >= static_cast<float>(rect_map_bounds_.Bottom()))
    kind_ret = SetDirection(kind_ret, ScrollDirection::Down, true);
  if (vec_center_location_.X >= static_cast<float>(rect_map_bounds_.Right()))
    kind_ret = SetDirection(kind_ret, ScrollDirection::Right, true);

  return kind_ret;
}

void Viewport::Tick() {
  if (kind_last_viewport_distance_ != settings_graphics_.kind_viewport_distance)
    UpdateViewportZooms();

  if (fn_viewport_center_provider != nullptr)
    Center(fn_viewport_center_provider());

  for (const auto& [uint8_token_ignored, fn_listener] : map_viewport_tick_)
    fn_listener();
}

std::uint64_t Viewport::SubscribeViewportTick(std::function<void()> fn_listener) {
  map_viewport_tick_.emplace(uint8_next_tick_token_, std::move(fn_listener));
  return uint8_next_tick_token_++;
}

void Viewport::UnsubscribeViewportTick(std::uint64_t uint8_token) {
  map_viewport_tick_.erase(uint8_token);
}

float Viewport::CalculateMinimumZoom(std::int32_t int4_native_height, float fp4_min_height,
                                     float fp4_max_height) {
  // 原生分辨率已在最大限内 = 1(也覆盖强制小于最小窗口的场景)
  // Native resolution already within the maximum limit = 1 (also covers a
  // forced resolution below the minimum window size).
  if (int4_native_height <= fp4_max_height)
    return 1;

  // 找整齐分数进入期望区间,减少走样
  // Find a clean fraction landing inside the desired range, reducing
  // aliasing.
  float fp4_step = 1.0f;
  while (true) {
    float fp4_test_zoom = 1.0f;
    while (true) {
      const float fp4_next_zoom = fp4_test_zoom + fp4_step;
      if (static_cast<float>(int4_native_height) < fp4_min_height * fp4_next_zoom)
        break;

      fp4_test_zoom = fp4_next_zoom;
    }

    if (static_cast<float>(int4_native_height) < fp4_max_height * fp4_test_zoom)
      return fp4_test_zoom;

    fp4_step /= 2;
  }
}

void Viewport::UpdateViewportZooms(bool b_reset_current_zoom) {
  kind_last_viewport_distance_ = settings_graphics_.kind_viewport_distance;

  const WorldViewport kind_vd = settings_graphics_.kind_viewport_distance;
  if (b_override_user_scale_ ||
      (ptr_sizes_->b_allow_native_zoom && kind_vd == WorldViewport::Native)) {
    fp4_min_zoom_ = fp4_default_scale_;
  } else {
    const int2 int4_range = ptr_sizes_->GetSizeRange(kind_vd);
    fp4_min_zoom_ = CalculateMinimumZoom(ptr_host_renderer_->NativeResolution().Y,
                                         static_cast<float>(int4_range.X),
                                         static_cast<float>(int4_range.Y)) *
                    fp4_default_scale_;
  }

  fp4_max_zoom_ = std::min(
      fp4_min_zoom_ * ptr_sizes_->fp4_max_zoom_scale,
      static_cast<float>(ptr_host_renderer_->NativeResolution().Y) * fp4_default_scale_ /
          static_cast<float>(ptr_sizes_->int4_max_zoom_window_height));

  if (b_unlock_min_zoom_) {
    // 观战/编辑器可再拉远 2×;TODO(上游):全图可见的更远解锁待视口滚动
    // 居中改进 —— 注释照抄
    // Spectators and the editor zoom out by an extra 2x; TODO (upstream):
    // unlocking until the full map is visible waits on better viewport
    // scroll centering — comment kept verbatim.
    fp4_unlocked_min_zoom_ = fp4_min_zoom_ * fp4_unlocked_min_zoom_scale_;
  }

  if (b_reset_current_zoom)
    SetZoom(fp4_min_zoom_);
  else
    SetZoom(std::min(std::max(fp4_zoom_, fp4_min_zoom_), fp4_max_zoom_));

  const float fp4_min_zoom = b_unlock_min_zoom_ ? fp4_unlocked_min_zoom_ : fp4_min_zoom_;
  const core::Vector2 vec_max =
      (1.0f / fp4_min_zoom) * ptr_host_renderer_->NativeResolution().ToVector2();
  ptr_host_renderer_->SetMaximumViewportSize(
      int2{static_cast<std::int32_t>(vec_max.X), static_cast<std::int32_t>(vec_max.Y)});

  for (INotifyViewportZoomExtentsChanged* ptr_t : vec_zoom_extents_listeners_)
    ptr_t->ViewportZoomExtentsChanged(fp4_min_zoom, fp4_max_zoom_);
}

CPos Viewport::ViewToWorld(int2 int2_view) {
  const int2 int2_world = ViewToWorldPx(int2_view);
  const TerrainMapSurface& surface = wr_.TerrainSurface();
  const std::vector<MPos> vec_candidates = CandidateMouseoverCells(int2_world);

  std::vector<int2> vec_screen;  // Ramp 四角屏幕位(逐候选复用缓冲)
                                 // The ramp corners' screen positions (a
                                 // buffer reused per candidate).
  for (const MPos uv : vec_candidates) {
    // 近旁格粗滤 —— 上游 L271-274
    // The coarse filter to nearby cells — upstream L271-274.
    const WPos wpos_p = surface.fn_center_of_cell(uv.ToCPos(surface.kind_grid_type));
    const int2 int2_s = wr_.ScreenPxPosition(wpos_p);
    if (std::abs(int2_s.X - int2_world.X) <= int2_tile_size_.X &&
        std::abs(int2_s.Y - int2_world.Y) <= int2_tile_size_.Y) {
      const std::int32_t int4_ramp_height =
          surface.fn_ramp_center_height_offset(uv.ToCPos(surface.kind_grid_type));
      const WPos wpos_pos =
          surface.fn_center_of_cell(uv.ToCPos(surface.kind_grid_type)) - WVec{0, 0, int4_ramp_height};

      vec_screen.clear();
      for (const WVec& v_c : surface.fn_ramp_corners(uv.ToCPos(surface.kind_grid_type)))
        vec_screen.push_back(wr_.ScreenPxPosition(wpos_pos + v_c));
      if (PolygonContains(vec_screen, int2_world))
        return uv.ToCPos(surface.kind_grid_type);
    }
  }

  // 鼠标不直接在格上(可能在悬崖)—— 取最近格
  // The mouse is not directly over a cell (perhaps a cliff) — take the
  // closest cell.
  if (!vec_candidates.empty()) {
    // MinBy(CompareBy):平局保首见 —— 上游 Exts 语义
    // MinBy (CompareBy): ties keep the first seen — the upstream Exts
    // semantics.
    const MPos* puv_best = &vec_candidates.front();
    std::int64_t int8_best = std::numeric_limits<std::int64_t>::max();
    for (const MPos uv : vec_candidates) {
      const WPos wpos_p = surface.fn_center_of_cell(uv.ToCPos(surface.kind_grid_type));
      const int2 int2_s = wr_.ScreenPxPosition(wpos_p);
      const std::int64_t int8_dx = std::abs(int2_s.X - int2_world.X);
      const std::int64_t int8_dy = std::abs(int2_s.Y - int2_world.Y);
      const std::int64_t int8_d = int8_dx * int8_dx + int8_dy * int8_dy;
      if (int8_d < int8_best) {
        int8_best = int8_d;
        puv_best = &uv;
      }
    }

    return puv_best->ToCPos(surface.kind_grid_type);
  }

  // 兜底:返回非完全离谱的格,望调用方能恢复 —— 上游注释逐句
  // Something is very wrong: return something not completely bogus and hope
  // the caller recovers — the upstream comment verbatim.
  const WPos wpos_projected = wr_.ProjectedPosition(ViewToWorldPx(int2_view));
  return deps_map_.fn_cell_containing(wpos_projected);
}

std::vector<MPos> Viewport::CandidateMouseoverCells(int2 int2_world) const {
  const TerrainMapSurface& surface = wr_.TerrainSurface();
  const std::int32_t int4_tile_scale = surface.int4_tile_scale / 2;
  const WPos wpos_min = wr_.ProjectedPosition(int2_world);

  // 求所有可能被点到的格;等距生成偏多 —— 上游 TODO 注释照抄
  // Find every cell that could have been clicked; isometric overgenerates —
  // the upstream TODO comment kept verbatim.
  MPos uv_a;
  MPos uv_b;
  if (surface.kind_grid_type == MapGridType::RectangularIsometric) {
    uv_a = deps_map_.fn_cell_containing(wpos_min - WVec{int4_tile_scale, 0, 0})
               .ToMPos(surface.kind_grid_type);
    uv_b = deps_map_
               .fn_cell_containing(wpos_min + WVec{int4_tile_scale,
                                                   int4_tile_scale * deps_map_.int4_maximum_terrain_height, 0})
               .ToMPos(surface.kind_grid_type);
  } else {
    uv_a = deps_map_.fn_cell_containing(wpos_min).ToMPos(surface.kind_grid_type);
    uv_b = deps_map_
               .fn_cell_containing(wpos_min + WVec{0,
                                                   int4_tile_scale * deps_map_.int4_maximum_terrain_height, 0})
               .ToMPos(surface.kind_grid_type);
  }

  // 双重递减循环序照抄(上游 yield 的消费序)
  // The doubly-decreasing loop order kept verbatim (the consumption order
  // of upstream's yield).
  std::vector<MPos> vec_cells;
  for (auto int4_v = uv_b.V; int4_v >= uv_a.V; int4_v--)
    for (auto int4_u = uv_b.U; int4_u >= uv_a.U; int4_u--)
      vec_cells.emplace_back(int4_u, int4_v);
  return vec_cells;
}

int2 Viewport::ViewToWorldPx(int2 int2_view) {
  // UIScale/Zoom*view + CenterLocation - ViewportSize/2;FromVector 向零截断
  // UIScale/Zoom*view + CenterLocation - ViewportSize/2; FromVector
  // truncates towards zero.
  const core::Vector2 vec_result =
      settings_graphics_.fp4_ui_scale / fp4_zoom_ * int2_view.ToVector2() + vec_center_location_ -
      (int2_viewport_size_ / 2).ToVector2();
  return int2::FromVector(vec_result);
}

int2 Viewport::WorldToViewPx(int2 int2_world) {
  const core::Vector2 vec_result =
      fp4_zoom_ / settings_graphics_.fp4_ui_scale *
      (int2_world.ToVector2() - vec_center_location_ + (int2_viewport_size_ / 2).ToVector2());
  return int2::FromVector(vec_result);
}

int2 Viewport::WorldToViewPx(const core::Vector3& vec_world) {
  const core::Vector2 vec_xy{vec_world.X, vec_world.Y};
  const core::Vector2 vec_result = fp4_zoom_ / settings_graphics_.fp4_ui_scale *
                                   (vec_xy - vec_center_location_ +
                                    0.5f * int2_viewport_size_.ToVector2());
  return int2::FromVector(vec_result);
}

void Viewport::Center(const WPos& wpos_pos) {
  vec_center_location_ = rect_map_bounds_.Clamp(wr_.ScreenPxPosition(wpos_pos).ToVector2());
  b_cells_dirty_ = true;
  b_all_cells_dirty_ = true;
}

void Viewport::Center(core::Vector2 vec_pos) {
  vec_center_location_ = rect_map_bounds_.Clamp(wr_.ScreenPosition(vec_pos));
  b_cells_dirty_ = true;
  b_all_cells_dirty_ = true;
}

void Viewport::Scroll(core::Vector2 vec_delta, bool b_ignore_borders) {
  // 世界像素位移换算为视口像素
  // The world-px delta converts to viewport-px.
  vec_center_location_ = vec_center_location_ + (1.0f / fp4_zoom_) * vec_delta;
  b_cells_dirty_ = true;
  b_all_cells_dirty_ = true;

  if (!b_ignore_borders)
    vec_center_location_ = rect_map_bounds_.Clamp(vec_center_location_);
}

Rectangle Viewport::GetScissorBounds(bool b_inside_bounds) {
  // 可见区(扩到格角),半格余量防等距 tile 被裁 —— 上游注释逐句
  // The visible region (expanded to the cell corners) with a half-cell
  // fudge against clipping isometric tiles — upstream comment verbatim.
  const ProjectedCellRegion& region_bounds =
      b_inside_bounds ? VisibleCellsInsideBounds() : AllVisibleCells();
  const TerrainMapSurface& surface = wr_.TerrainSurface();

  const MPos uv_tl = ToMPos(region_bounds.TopLeft());
  const MPos uv_br = ToMPos(region_bounds.BottomRight());
  const WPos wpos_ctl =
      surface.fn_center_of_cell(uv_tl.ToCPos(surface.kind_grid_type)) - WVec{512, 512, 0};
  const WPos wpos_cbr =
      surface.fn_center_of_cell(uv_br.ToCPos(surface.kind_grid_type)) + WVec{512, 512, 0};

  // 转屏幕坐标时剥 Z(投影到 z=0 平面)
  // The screen conversion strips Z (projecting onto the z=0 plane).
  const int2 int2_tl =
      wr_.ScreenPxPosition(wpos_ctl - WVec{0, 0, wpos_ctl.Z}) - TopLeft();
  const int2 int2_br =
      wr_.ScreenPxPosition(wpos_cbr - WVec{0, 0, wpos_cbr.Z}) - TopLeft();

  return Rectangle::FromLTRB(int2_tl.X - int2_tile_size_.X / 2, int2_tl.Y - int2_tile_size_.Y / 2,
                             int2_br.X + int2_tile_size_.X / 2, int2_br.Y + int2_tile_size_.Y / 2);
}

ProjectedCellRegion Viewport::CalculateVisibleCells(bool b_inside_bounds) const {
  const TerrainMapSurface& surface = wr_.TerrainSurface();

  // 可见区四角的投影格位
  // The projected cell positions at the visible area's corners.
  PPos puv_tl = ToPPos(deps_map_.fn_cell_containing(wr_.ProjectedPosition(TopLeftPx()))
                           .ToMPos(surface.kind_grid_type));
  PPos puv_br = ToPPos(deps_map_.fn_cell_containing(wr_.ProjectedPosition(BottomRightPx()))
                           .ToMPos(surface.kind_grid_type));

  // 等距地图边缘不齐,四边各加一格余量 —— 上游注释逐句
  // Isometric maps have jagged edges, so pad one cell per edge — the
  // upstream comment verbatim.
  if (surface.kind_grid_type == MapGridType::RectangularIsometric) {
    puv_tl = PPos{puv_tl.U - 1, puv_tl.V - 1};
    puv_br = PPos{puv_br.U + 1, puv_br.V + 1};
  }

  // 需要时夹到可见地图界内
  // Clamped into the visible map bounds when requested.
  if (b_inside_bounds) {
    puv_tl = deps_map_.fn_clamp_ppos(puv_tl);
    puv_br = deps_map_.fn_clamp_ppos(puv_br);
  }

  return ProjectedCellRegion::Make(puv_tl, puv_br, deps_map_.fn_height_clamp_mpos,
                                   surface.kind_grid_type == MapGridType::RectangularIsometric,
                                   deps_map_.int4_maximum_terrain_height);
}

const ProjectedCellRegion& Viewport::VisibleCellsInsideBounds() {
  if (b_cells_dirty_) {
    region_cells_ = CalculateVisibleCells(true);
    b_cells_dirty_ = false;
  }

  return region_cells_;
}

const ProjectedCellRegion& Viewport::AllVisibleCells() {
  if (b_all_cells_dirty_) {
    region_all_cells_ = CalculateVisibleCells(false);
    b_all_cells_dirty_ = false;
  }

  return region_all_cells_;
}

void Viewport::SetZoom(float fp4_value) {
  fp4_zoom_ = fp4_value;
  // Size.FromVector = (int) 向零截断
  // Size.FromVector = (int) truncation towards zero.
  const core::Vector2 vec_size =
      (1.0f / fp4_zoom_) * ptr_host_renderer_->NativeResolution().ToVector2();
  int2_viewport_size_ = int2{static_cast<std::int32_t>(vec_size.X),
                             static_cast<std::int32_t>(vec_size.Y)};
  b_cells_dirty_ = true;
  b_all_cells_dirty_ = true;
}

int2 Viewport::TopLeftPx() const {
  return int2::FromVector(vec_center_location_) - int2_viewport_size_ / 2;
}

int2 Viewport::BottomRightPx() const {
  return int2::FromVector(vec_center_location_) + int2_viewport_size_ / 2;
}

}  // namespace ora::gfx
