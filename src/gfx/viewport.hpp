// UPSTREAM: OpenRA.Game/Graphics/Viewport.cs @b6fc03f L21-447(全文逐语义)
// 视口:缩放/居中/滚动/世界-屏幕坐标换算 + 可见投影格区(惰性缓存)。
// 形态适配(注入面,D89~D92 同族;完整 Map/Settings/Renderer 随 Phase 5):
//   - Game.Renderer.NativeResolution / SetMaximumViewportSize →
//     IViewportHostRenderer 注入(上游进程单例;测试/Phase 5 适配);
//   - Game.Settings.Graphics(ViewportDistance/UIScale)→ GraphicSettingsFace
//     值注入;Game.Settings.Sound.Repeat 的写入面在 Sound 门面侧;
//   - WorldViewportSizes(IGlobalModData)→ 值结构 + GetSizeRange(随
//     Phase 5 规则链加载);
//   - Map 依赖:ProjectedTopLeft/BottomRight(WPos 角)、MapSize、
//     Clamp(PPos)/Height.Clamp(MPos)/CellContaining(WPos)→ Deps 函数注入
//     (默认 = 方格网格 / 界内恒真形态,与 TerrainMapSurface.MakeSquareDefault
//     同族);CenterOfCell/TileScale/Ramp 查询复用 wr.TerrainSurface();
//   - CandidateMouseoverCells 的 yield 惰性 → vector 物化(遍历序保持
//     上游双重递减循环);
//   - ViewportTick 事件 → token 订阅表(成对注销);
//   - Center(IEnumerable&lt;Actor&gt;) 的 Average → 调用方求均值注入
//     (Actor::CenterPosition 面 Phase 5)。
// The viewport: zoom/centering/scrolling/world-screen conversion plus the
// lazily cached visible projected-cell regions. Shape adaptations (the
// injection faces, the D89-D92 family; the full Map/Settings/Renderer land
// with Phase 5):
//   - Game.Renderer.NativeResolution / SetMaximumViewportSize → the injected
//     IViewportHostRenderer (an upstream process singleton; tests and Phase
//     5 adapt it);
//   - Game.Settings.Graphics (ViewportDistance/UIScale) → the injected
//     GraphicSettingsFace value; Game.Settings.Sound.Repeat's write face
//     lives on the Sound facade side;
//   - WorldViewportSizes (IGlobalModData) → a value struct + GetSizeRange
//     (loaded with the Phase 5 rules chain);
//   - the Map dependencies: ProjectedTopLeft/BottomRight (the WPos corners),
//     MapSize, Clamp(PPos)/Height.Clamp(MPos)/CellContaining(WPos) → Deps
//     function injections (defaults = the square-grid / always-in-bounds
//     shapes, the same family as TerrainMapSurface.MakeSquareDefault);
//     CenterOfCell/TileScale/Ramp queries reuse wr.TerrainSurface();
//   - CandidateMouseoverCells' lazy yield → a materialized vector (the
//     traversal order keeps upstream's doubly-decreasing loops);
//   - the ViewportTick event → a token subscription table (paired
//     unsubscribe);
//   - Center(IEnumerable<Actor>)'s Average → the caller supplies the mean
//     (the Actor::CenterPosition face is Phase 5).
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

/// 滚动方向(Viewport.cs L22;[Flags])。
/// Scroll directions (Viewport.cs L22; [Flags]).
enum class ScrollDirection : std::uint8_t {
  None = 0,
  Up = 1,
  Left = 2,
  Down = 4,
  Right = 8,
};

/// ViewportExts.Includes(L31-35):(d &amp; s) == s(上游 PERF 注释:快于
/// Enum.HasFlag 且零分配)。
/// ViewportExts.Includes (L31-35): (d & s) == s (the upstream PERF note:
/// faster than Enum.HasFlag with zero allocations).
constexpr bool Includes(ScrollDirection d_kind, ScrollDirection s_kind) {
  return (static_cast<std::uint8_t>(d_kind) & static_cast<std::uint8_t>(s_kind)) ==
         static_cast<std::uint8_t>(s_kind);
}

/// ViewportExts.Set(L37-40):包含态与 val 不一致时翻转 s。
/// ViewportExts.Set (L37-40): toggles s when the inclusion state differs
/// from val.
constexpr ScrollDirection SetDirection(ScrollDirection d_kind, ScrollDirection s_kind, bool b_val) {
  return Includes(d_kind, s_kind) != b_val
             ? static_cast<ScrollDirection>(static_cast<std::uint8_t>(d_kind) ^
                                            static_cast<std::uint8_t>(s_kind))
             : d_kind;
}

/// 视口距离档(Settings.cs L50)。
/// The viewport-distance tiers (Settings.cs L50).
enum class WorldViewport : std::uint8_t { Native, Close, Medium, Far };

/// 缩放界变化的订阅面(Viewport.cs L24-27;上游经 WorldActor trait 反射
/// 收集,Phase 5 接线)。
/// The zoom-extents subscription face (Viewport.cs L24-27; upstream collects
/// it via WorldActor trait reflection, wired in Phase 5).
struct INotifyViewportZoomExtentsChanged {
  virtual void ViewportZoomExtentsChanged(float fp4_min_zoom, float fp4_max_zoom) = 0;
  virtual ~INotifyViewportZoomExtentsChanged() = default;
};

/// Renderer 的视口面(上游 Game.Renderer 静态;NativeResolution = 窗口像
/// 素尺寸,SetMaximumViewportSize = 最大 world 帧缓冲需求)。
/// The viewport face of the Renderer (upstream's Game.Renderer static;
/// NativeResolution is the window size in pixels, SetMaximumViewportSize the
/// maximum world-framebuffer demand).

struct IViewportHostRenderer {
  virtual int2 NativeResolution() const = 0;
  virtual void SetMaximumViewportSize(int2 int2_size) = 0;
  virtual ~IViewportHostRenderer() = default;
};

/// GraphicSettings 的视口相关字段值面(Settings.cs L252-253)。
/// The value face of GraphicSettings' viewport-relevant fields (Settings.cs
/// L252-253).
struct GraphicSettingsFace {
  WorldViewport kind_viewport_distance = WorldViewport::Medium;
  float fp4_ui_scale = 1.0f;
};

/// WorldViewportSizes(WorldViewportSizes.cs L19-42;IGlobalModData 值面,
/// Phase 5 规则链加载)。
/// WorldViewportSizes (WorldViewportSizes.cs L19-42; the IGlobalModData
/// value face, loaded by the Phase 5 rules chain).
struct WorldViewportSizes {
  int2 int2_close_window_heights{480, 600};
  int2 int2_medium_window_heights{600, 900};
  int2 int2_far_window_heights{900, 1300};

  float fp4_default_scale = 1.0f;
  float fp4_max_zoom_scale = 2.0f;
  std::int32_t int4_max_zoom_window_height = 240;
  bool b_allow_native_zoom = true;

  int2 GetSizeRange(WorldViewport kind_distance) const {
    return kind_distance == WorldViewport::Close ? int2_close_window_heights
           : kind_distance == WorldViewport::Medium ? int2_medium_window_heights
                                                    : int2_far_window_heights;
  }
};

/// Viewport(Viewport.cs L43)。
class Viewport : public IViewportSurface {
 public:
  /// Map 依赖的注入面(见文件头;默认 = 方格网格 / 界内恒真)。
  /// The injected Map face (see the header note; defaults = the square grid
  /// / always-in-bounds shapes).
  struct Deps {
    WPos wpos_projected_top_left{};      // map.ProjectedTopLeft
    WPos wpos_projected_bottom_right{};  // map.ProjectedBottomRight
    int2 int2_map_size{0, 0};            // map.MapSize
    std::int32_t int4_maximum_terrain_height = 0;  // map.Grid.MaximumTerrainHeight

    /// map.Clamp(PPos)(Map.cs L1277-1281);默认恒在界内(恒等)。
    /// map.Clamp(PPos) (Map.cs L1277-1281); the default is
    /// always-in-bounds (identity).
    std::function<PPos(PPos)> fn_clamp_ppos;

    /// map.Height.Clamp(MPos)(CellLayer 的界内夹取);默认恒等。
    /// map.Height.Clamp(MPos) (the CellLayer in-bounds clamp); identity by
    /// default.
    std::function<MPos(MPos)> fn_height_clamp_mpos;

    /// map.CellContaining(WPos)(Map.cs L1053-);默认 = 方格网格公式
    /// (pos/1024;C# 整除向零截断)。
    /// map.CellContaining(WPos) (Map.cs L1053-); the default is the
    /// square-grid formula (pos/1024; C# integer division truncates towards
    /// zero).
    std::function<CPos(const WPos&)> fn_cell_containing;
  };

  Viewport(WorldRenderer& wr_render, const Deps& deps_map, IViewportHostRenderer& host_renderer,
           const GraphicSettingsFace& settings_graphics,
           const WorldViewportSizes& sizes_viewport);

  // ———— 几何面(L54-61)————
  // ———— The geometry faces (L54-61) ————
  core::Vector2 CenterLocation() const { return vec_center_location_; }
  WPos CenterPosition() const;
  int2 TopLeft() override;
  int2 BottomRight() override;
  int2 ViewportSize() const { return int2_viewport_size_; }
  float Zoom() const { return fp4_zoom_; }
  float MinZoom() const { return fp4_min_zoom_; }
  float MaxZoom() const { return fp4_max_zoom_; }

  /// OverrideDefaultHeight(L95-100)。
  void OverrideDefaultHeight(float fp4_height);

  /// AdjustZoom(L102-106):指数步进(正负步等效 —— 上游注释逐句)。
  /// AdjustZoom (L102-106): the exponential step (equal positive and
  /// negative steps have the same effect — the upstream comment verbatim).
  void AdjustZoom(float fp4_dz);
  /// AdjustZoom(dz, center)(L108-116):以屏幕点为锚缩放(旧/新世界像素差
  /// 补偿回滚 CenterLocation)。
  /// AdjustZoom (dz, center) (L108-116): zooms anchored at a screen point
  /// (the old/new world-px delta compensates CenterLocation back).
  void AdjustZoom(float fp4_dz, int2 int2_center);

  /// ToggleZoom(L118-125):解锁档恒回默认;MinZoom 之上回 MinZoom,恰在
  /// MinZoom 跳 MaxZoom。
  /// ToggleZoom (L118-125): an unlocked zoom always resets to the default;
  /// above MinZoom it returns to MinZoom, exactly at MinZoom it jumps to
  /// MaxZoom.
  void ToggleZoom();

  /// UnlockMinimumZoom(L127-132):观战/编辑器的 2× 额外远距。
  /// UnlockMinimumZoom (L127-132): the spectators/editor extra 2x distance.
  void UnlockMinimumZoom(float fp4_scale);

  static std::int64_t int8_last_move_run_time;  // LastMoveRunTime(L134)
  static int2 int2_last_mouse_pos;              // LastMousePos(L135)

  /// GetBlockedDirections(L137-150)。
  ScrollDirection GetBlockedDirections() const;

  /// Tick(L183-192):ViewportDistance 变化重算 + 居中供应器 + 事件分发。
  /// Tick (L183-192): recalculates on ViewportDistance changes, applies the
  /// center provider, and dispatches the tick event.
  void Tick();

  /// ViewportCenterProvider(L76;可空函数)。
  /// ViewportCenterProvider (L76; a nullable function).
  std::function<core::Vector2()> fn_viewport_center_provider;

  /// ViewportTick 事件(L77;token 订阅表,成对注销)。
  /// The ViewportTick event (L77; a token subscription table, paired
  /// unsubscribe).
  std::uint64_t SubscribeViewportTick(std::function<void()> fn_listener);
  void UnsubscribeViewportTick(std::uint64_t uint8_token);

  /// ViewToWorld(L263-301):粗滤 + Ramp 角多边形精确命中 + 最近格回退。
  /// ViewToWorld (L263-301): the coarse filter + the ramp-corner polygon
  /// exact hit + the closest-cell fallback.
  CPos ViewToWorld(int2 int2_view);

  /// ViewToWorldPx(L330-331)。
  int2 ViewToWorldPx(int2 int2_view);
  /// WorldToViewPx(L333-337;两形态;IViewportSurface 面)。
  /// WorldToViewPx (L333-337; both forms; the IViewportSurface face).
  int2 WorldToViewPx(int2 int2_world) override;
  int2 WorldToViewPx(const core::Vector3& vec_world) override;

  /// Center(WPos)(L350-355)/ Center(Vector2)(L357-362)。
  /// Center (WPos) (L350-355) / Center (Vector2) (L357-362).
  void Center(const WPos& wpos_pos);
  void Center(core::Vector2 vec_pos);

  /// Scroll(L364-373):世界像素 → 视口像素换算后加位移。
  /// Scroll (L364-373): converts the world-px delta to viewport-px before
  /// shifting.
  void Scroll(core::Vector2 vec_delta, bool b_ignore_borders);

  /// GetScissorBounds(L376-391;IViewportSurface 面)。
  Rectangle GetScissorBounds(bool b_inside_bounds) override;

  /// VisibleCellsInsideBounds / AllVisibleCells(L419-445;惰性缓存)。
  /// VisibleCellsInsideBounds / AllVisibleCells (L419-445; lazily cached).
  const ProjectedCellRegion& VisibleCellsInsideBounds();
  const ProjectedCellRegion& AllVisibleCells();

  // ———— 测试面 / the test faces ————
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
  /// Zoom 私有 setter(L79-90):zoom / ViewportSize / 双脏标记。
  /// The private Zoom setter (L79-90): zoom / ViewportSize / both dirty
  /// flags.
  void SetZoom(float fp4_value);

  /// CalculateMinimumZoom(L194-222;上游 static 读 Game.Renderer,宿主分辨
  /// 率作参)。
  /// CalculateMinimumZoom (L194-222; upstream's static reads Game.Renderer —
  /// the host resolution arrives as a parameter).
  static float CalculateMinimumZoom(std::int32_t int4_native_height, float fp4_min_height,
                                    float fp4_max_height);

  /// UpdateViewportZooms(L224-261)。
  void UpdateViewportZooms(bool b_reset_current_zoom = true);

  /// CandidateMouseoverCells(L304-328;物化,序保持)。
  /// CandidateMouseoverCells (L304-328; materialized, the order kept).
  std::vector<MPos> CandidateMouseoverCells(int2 int2_world) const;

  /// CalculateVisibleCells(L393-417)。
  ProjectedCellRegion CalculateVisibleCells(bool b_inside_bounds) const;

  /// TopLeft/BottomRight 的 const 实现(override 面为非 const,与
  /// IViewportSurface 一致)。
  /// The const implementations of TopLeft/BottomRight (the override face is
  /// non-const, matching IViewportSurface).
  int2 TopLeftPx() const;
  int2 BottomRightPx() const;

  WorldRenderer& wr_;
  const WorldViewportSizes* ptr_sizes_;
  GraphicSettingsFace settings_graphics_;

  // 地图界(world-px)| the map bounds (world-px).
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

}  // namespace ora::gfx
