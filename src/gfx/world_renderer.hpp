// UPSTREAM: OpenRA.Game/Graphics/WorldRenderer.cs @e180e3225 L23-483(逐方法)+
//           OpenRA.Game/Traits/TraitsInterfaces.cs @e180e3225 L435-476(IRenderTerrain/
//           ITerrainLighting/IRenderPostProcessPass/PostProcessPassType)+
//           OpenRA.Game/Map/MapGrid.cs @e180e3225 L20-57(默认 Ramp 角的方格形态)
// 世界渲染器:调色板管理 + 世界坐标到屏幕坐标换算 + 渲染收集/排序/绘制序。
// OPT-A7(docs/OPTIMIZATION_TRACKER.md)本批落地的"渲染收集 SOA + 帧 arena":
//   - 上游 GenerateRenderables(L141-175)每帧向 renderablesBuffer 装入 IRenderable
//     堆对象、PrepareRender 再分配 IFinalizedRenderable;C++ 收集进复用的
//     vector<RenderItem>(POD 值语义,见 renderable.hpp),排序 key 数组与分段
//     缓冲在 FrameArena 上(帧末 rewind);
//   - 上游 Draw 的 overlay GroupBy(GetType()) HACK(L319-323)→ kind 计数分段
//     (首遇序 + 段内收集序,两阶稳定序与 LINQ 逐项一致)。
// 注入面(上游构造期 L53-79 从 ModData/World 的 trait 反射链取的全部依赖,
// Phase 5 随 Game/Map 接线;机制面完整移植):
//   - TileSize/TileScale(上游 = map.Rules.TerrainInfo.TileSize / map.Grid.
//     TileScale)→ Desc 构造参数;enableDepthBuffer(mapGrid.EnableDepthBuffer)
//     → Desc;
//   - Viewport(上游 new Viewport(this, map);Viewport.cs 依赖 Map 的
//     ProjectedCellRegion/Ramp/CellContaining 全家)→ IViewportSurface 接口
//     + SetViewport;完整 Viewport 随 Phase 5 的 Map 落地(COVERAGE 登记);
//   - ILoadsPalettes/ILoadsPlayerPalettes trait 收集、ScreenMap 查询、
//     OrderGenerator/Selection/Effects 渲染源、IRenderer/IRenderTerrain/
//     IRenderAboveWorld/IRenderShroud trait 族、DebugVisualizations →
//     Hooks/收集回调/接口表注入(默认 no-op);
//   - Game.Renderer 静态 → Renderer* 注入(上游为进程单例,等价);
//   - IPaletteModifier 收集(上游 World.WorldActor.TraitsImplementing)→
//     RefreshPalette 的 span 参数。
// 形态适配:三 List<IFinalizedRenderable> → 三 vector<RenderItem>(复用,
// 帧内零分配);event → 订阅表(成对注销,token 制);上游 Dispose 的
// "World 所有权 HACK"(L473-482)不移植 —— C++ 侧 World 生存期归 sim 调用方,
// 析构只释放本类资源(COVERAGE 登记)。
// The world renderer: palette management + world-to-screen conversion + the
// render collection/sorting/draw order. This batch lands OPT-A7's "render
// collection SOA + frame arena" (docs/OPTIMIZATION_TRACKER.md): upstream's
// GenerateRenderables (L141-175) heap-allocates an IRenderable per source
// item and then an IFinalizedRenderable per PrepareRender; C++ collects into
// a reused vector<RenderItem> (POD value semantics — see renderable.hpp)
// with the sort-key and segmentation buffers on a FrameArena (rewound at
// frame end), and the overlay GroupBy(GetType()) HACK of Draw (L319-323)
// becomes per-kind counting segmentation (first-encounter order between
// segments, collection order within — item-for-item equal to LINQ's
// two-level stable order). Injection surfaces (everything upstream's
// constructor L53-79 pulls from the ModData/World trait reflection chain,
// wired with Game/Map in Phase 5; the mechanics are fully ported):
// TileSize/TileScale (upstream = map.Rules.TerrainInfo.TileSize /
// map.Grid.TileScale) and enableDepthBuffer arrive via Desc; the Viewport
// (upstream new Viewport(this, map); Viewport.cs leans on the whole Map
// family of ProjectedCellRegion/Ramp/CellContaining) becomes the
// IViewportSurface interface + SetViewport, with the full Viewport landing
// with Phase 5's Map (registered in COVERAGE); the
// ILoadsPalettes/ILoadsPlayerPalettes trait collection, the ScreenMap
// queries, the OrderGenerator/Selection/Effects render sources, the
// IRenderer/IRenderTerrain/IRenderAboveWorld/IRenderShroud trait families,
// and DebugVisualizations arrive as Hooks/collection callbacks/interface
// tables (no-op by default); the Game.Renderer static becomes an injected
// Renderer* (upstream is a process singleton, equivalent); the IPaletteModifier
// collection (upstream World.WorldActor.TraitsImplementing) becomes the span
// parameter of RefreshPalette. Shape adaptations: the three
// List<IFinalizedRenderable> → three reused vector<RenderItem> (zero
// in-frame allocation); events → token-managed subscription tables (paired
// unsubscribe); upstream Dispose's "world ownership HACK" (L473-482) is not
// ported — World's lifetime belongs to the sim caller on the C++ side, and
// the destructor frees only this class's own resources (registered in
// COVERAGE).
#pragma once
import std;

#include "core/arena.hpp"
#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "core/wpos.hpp"
#include "core/wvec.hpp"
#include "gfx/hardware_palette.hpp"
#include "gfx/palette.hpp"
#include "gfx/renderable.hpp"
#include "gfx/sprite.hpp"

namespace ora::sim {
class World;
}

namespace ora::gfx {

class Renderer;

/// 后处理趟类型(TraitsInterfaces.cs L468;序 = 上游声明序)。
/// The post-process pass types (TraitsInterfaces.cs L468; the order =
/// upstream's declaration order).
enum class PostProcessPassType : std::uint8_t { AfterShroud, AfterWorld, AfterActors, AfterAnnotations };

/// IRenderPostProcessPass(TraitsInterfaces.cs L470-476)。
/// IRenderPostProcessPass (TraitsInterfaces.cs L470-476).
struct IRenderPostProcessPass {
  virtual PostProcessPassType Type() const = 0;
  virtual bool Enabled() const = 0;
  virtual void Draw(WorldRenderer& wr) = 0;
  virtual ~IRenderPostProcessPass() = default;
};

/// IRenderer trait(TraitsInterfaces.cs;BeginFrame/EndFrame 面)。
/// The IRenderer trait (TraitsInterfaces.cs; the BeginFrame/EndFrame face).
struct IRendererTrait {
  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;
  virtual ~IRendererTrait() = default;
};

/// ITerrainLighting(TraitsInterfaces.cs L438-442):TintAt 查询 + CellChanged
/// 事件(上游 event Action&lt;MPos&gt;;C++ 以 token 订阅表承载,TerrainSpriteLayer
/// 的 tint 增量更新挂此)。
/// ITerrainLighting (TraitsInterfaces.cs L438-442): the TintAt query plus the
/// CellChanged event (upstream event Action&lt;MPos&gt;; carried as a
/// token-managed subscription table — TerrainSpriteLayer's incremental tint
/// updates hang off it).
struct ITerrainLighting {
  virtual core::Vector3 TintAt(const WPos& wpos_pos) const = 0;
  virtual std::uint64_t AddCellChangedListener(std::function<void(MPos)> fn_listener) = 0;
  virtual void RemoveCellChangedListener(std::uint64_t uint8_token) = 0;
  virtual ~ITerrainLighting() = default;
};

/// Viewport 的最小渲染面(完整 Viewport 随 Phase 5 的 Map;上游 Viewport.cs)。
/// The minimal render surface of the Viewport (the full Viewport arrives with
/// Phase 5's Map; upstream Viewport.cs).
enum class ScrollDirection : std::uint8_t {
  None = 0,
  Up = 1,
  Left = 2,
  Down = 4,
  Right = 8,
};

constexpr bool Includes(ScrollDirection d_kind, ScrollDirection s_kind) {
  return (static_cast<std::uint8_t>(d_kind) & static_cast<std::uint8_t>(s_kind)) ==
         static_cast<std::uint8_t>(s_kind);
}

constexpr ScrollDirection operator|(ScrollDirection d_a, ScrollDirection d_b) {
  return static_cast<ScrollDirection>(static_cast<std::uint8_t>(d_a) |
                                      static_cast<std::uint8_t>(d_b));
}

constexpr ScrollDirection SetDirection(ScrollDirection d_kind, ScrollDirection s_kind,
                                       bool b_val) {
  return Includes(d_kind, s_kind) != b_val
             ? static_cast<ScrollDirection>(static_cast<std::uint8_t>(d_kind) ^
                                            static_cast<std::uint8_t>(s_kind))
             : d_kind;
}

struct IViewportSurface {
  virtual int2 WorldToViewPx(int2 int2_world) = 0;
  virtual int2 WorldToViewPx(const core::Vector3& vec_world) = 0;

  /// Zoom/MinZoom(Viewport.cs L180-181;缺省 1.0 = 桩永不触发缩出隐藏)
  /// Zoom/MinZoom (Viewport.cs L180-181; 1.0 defaults never hide).
  virtual float Zoom() const { return 1.0f; }
  virtual float MinZoom() const { return 1.0f; }

  /// 裁剪矩形(Viewport.cs L376-391;insideBounds = VisibleCellsInsideBounds
  /// 否则 AllVisibleCells)。
  /// The scissor rectangle (Viewport.cs L376-391; insideBounds selects
  /// VisibleCellsInsideBounds over AllVisibleCells).
  virtual Rectangle GetScissorBounds(bool b_inside_bounds) = 0;

  /// 收集查询框(WorldRenderer 上游 ScreenMap.RenderableActorsInBox 的入参)。
  /// The collection query box (the argument of upstream's ScreenMap
  /// RenderableActorsInBox).
  virtual int2 TopLeft() = 0;
  virtual int2 BottomRight() = 0;
  /// Center 族(Viewport.cs L350-362 的接口面;Center(IEnumerable&lt;Actor&gt;)
  /// 的均值由调用方求 —— ControlGroupsWidget 消费)
  /// The Center family (the interface face of Viewport.cs L350-362; the
  /// Center(IEnumerable<Actor>) mean is the caller's — consumed by
  /// ControlGroupsWidget).
  virtual void Center(const WPos& wpos_pos) { (void)wpos_pos; }
  virtual void Center(core::Vector2 vec_pos) { (void)vec_pos; }
  virtual CPos ViewToWorld(int2 int2_view) {
    (void)int2_view;
    return CPos{0, 0};
  }
  virtual int2 ViewToWorldPx(int2 int2_view) {
    (void)int2_view;
    return int2{0, 0};
  }
  virtual WPos CenterPosition() const { return WPos{}; }
  virtual void Scroll(core::Vector2 vec_delta, bool b_ignore_borders) {
    (void)vec_delta;
    (void)b_ignore_borders;
  }
  virtual void AdjustZoom(float fp4_dz) { (void)fp4_dz; }
  virtual void AdjustZoom(float fp4_dz, int2 int2_center) {
    (void)fp4_dz;
    (void)int2_center;
  }
  virtual ScrollDirection GetBlockedDirections() const { return ScrollDirection::None; }
  virtual std::uint64_t SubscribeViewportTick(std::function<void()> fn_listener) {
    (void)fn_listener;
    return 0;
  }
  virtual void UnsubscribeViewportTick(std::uint64_t uint8_token) { (void)uint8_token; }
  virtual ~IViewportSurface() = default;
};

struct TerrainMapSurface {
  int2 int2_map_size{0, 0};
  MapGridType kind_grid_type = MapGridType::Rectangular;
  std::int32_t int4_tile_scale = 1024;  // map.Grid.TileScale(世界单位/格)| world units per cell

  std::function<WPos(CPos)> fn_center_of_cell;

  std::function<bool(MPos)> fn_tiles_contains;

  std::function<std::int32_t(CPos)> fn_ramp_center_height_offset;

  std::function<std::span<const WVec>(CPos)> fn_ramp_corners;

  static TerrainMapSurface MakeSquareDefault(int2 int2_map_size, std::int32_t int4_tile_scale = 1024);
};

class WorldRenderer {
 public:
  struct Desc {
    int2 int2_tile_size{24, 24};  // map.Rules.TerrainInfo.TileSize | TerrainInfo.TileSize
    std::int32_t int4_tile_scale = 1024;
    bool b_enable_depth_buffer = false;  // mapGrid.EnableDepthBuffer | mapGrid.EnableDepthBuffer
    Renderer* ptr_renderer = nullptr;    // Game.Renderer | Game.Renderer
  };

  struct Hooks {
    std::function<void(WorldRenderer&, int2, int2)> fn_collect_renderables;

    std::function<void(WorldRenderer&, int2, int2)> fn_collect_overlay;
    std::function<void(WorldRenderer&, int2, int2)> fn_collect_annotations;

    std::function<void(WorldRenderer&, IViewportSurface&)> fn_render_terrain;

    std::function<void(WorldRenderer&)> fn_render_above_world;
    std::function<void(WorldRenderer&)> fn_render_shroud;

    std::function<void(WorldRenderer&)> fn_debug_update_depth;
    std::function<void(WorldRenderer&)> fn_debug_draw_geometry;
    std::function<void(WorldRenderer&)> fn_debug_draw_screen_map;
  };

  WorldRenderer(sim::World& world, const Desc& desc);
  ~WorldRenderer();

  WorldRenderer(const WorldRenderer&) = delete;
  WorldRenderer& operator=(const WorldRenderer&) = delete;

  const int2& TileSize() const { return int2_tile_size_; }
  std::int32_t TileScale() const { return int4_tile_scale_; }
  sim::World& World() const { return world_; }
  IViewportSurface* Viewport() const { return ptr_viewport_; }
  void SetViewport(IViewportSurface* ptr_viewport) { ptr_viewport_ = ptr_viewport; }
  ITerrainLighting* TerrainLighting() const { return ptr_terrain_lighting_; }
  void SetTerrainLighting(ITerrainLighting* ptr_lighting) { ptr_terrain_lighting_ = ptr_lighting; }
  const TerrainMapSurface& TerrainSurface() const { return surface_terrain_; }
  void SetTerrainSurface(TerrainMapSurface surface) { surface_terrain_ = std::move(surface); }
  Renderer* RendererPtr() const { return ptr_renderer_; }

  void BeginFrame();
  void EndFrame();
  void SetRendererTraits(std::span<IRendererTrait* const> vec_renderers) {
    vec_renderer_traits_.assign(vec_renderers.begin(), vec_renderers.end());
  }
  void SetPostProcessPasses(std::span<IRenderPostProcessPass* const> vec_passes) {
    vec_post_process_passes_.assign(vec_passes.begin(), vec_passes.end());
  }

  void SetPlayerPaletteLoader(
      std::function<void(WorldRenderer&, std::string_view, core::Color, bool)> fn_load) {
    fn_load_player_palettes_ = std::move(fn_load);
  }
  void UpdatePalettesForPlayer(std::string_view str_internal_name, core::Color color_player,
                               bool b_replace_existing);

  PaletteReference* Palette(std::string_view str_name);

  void AddPalette(std::string_view str_name, ImmutablePalette palette_p, bool b_allow_modifiers = false,
                  bool b_allow_overwrite = false);

  void ReplacePalette(std::string_view str_name, const IPalette& palette_p);

  void SetPaletteColorShift(std::string_view str_name, float float_hue_offset, float float_sat_offset,
                            float float_value_multiplier, float float_min_hue, float float_max_hue);

  void RefreshPalette(std::span<IPaletteModifier* const> vec_modifiers);

  HardwarePalette& hardware_palette() { return hardware_palette_; }

  std::uint64_t SubscribePaletteInvalidated(std::function<void()> fn_listener);
  void UnsubscribePaletteInvalidated(std::uint64_t uint8_token);

  core::Vector2 ScreenPosition(const WPos& wpos_pos) const;
  core::Vector2 ScreenPosition(core::Vector2 vec_pos) const;
  core::Vector3 Screen3DPosition(const WPos& wpos_pos) const;
  int2 ScreenPxPosition(const WPos& wpos_pos) const;
  core::Vector3 Screen3DPxPosition(const WPos& wpos_pos) const;
  core::Vector3 ScreenVectorComponents(const WVec& wvec_vec) const;
  std::array<float, 4> ScreenVector(const WVec& wvec_vec) const;
  int2 ScreenPxOffset(const WVec& wvec_vec) const;
  WPos ProjectedPosition(int2 int2_screen_px) const;

  void AddRenderable(const RenderItem& item_r);
  void AddOverlayRenderable(const RenderItem& item_r);
  void AddAnnotationRenderable(const RenderItem& item_r);

  void PrepareRenderables(std::span<IPaletteModifier* const> vec_palette_modifiers);

  void Draw();

  void DrawAnnotations();

  Hooks& hooks() { return hooks_; }

  FrameArena& frame_arena() { return arena_frame_; }

  std::span<const RenderItem> PreparedRenderablesForTest() const { return vec_prepared_renderables_; }
  std::span<const RenderItem> PreparedOverlayForTest() const { return vec_prepared_overlay_; }
  std::span<const RenderItem> PreparedAnnotationForTest() const { return vec_prepared_annotation_; }
  std::span<const RenderItem> RenderablesBufferForTest() const { return vec_renderables_buffer_; }
  const HardwarePalette& PaletteDataForTest() const { return hardware_palette_; }

 private:
  void GenerateRenderables();           // L141-175
  void GenerateOverlayRenderables();    // L178-216
  void GenerateAnnotationRenderables(); // L219-257
  void ApplyPostProcessing(PostProcessPassType kind_type);  // L330-340

  sim::World& world_;
  int2 int2_tile_size_;
  std::int32_t int4_tile_scale_;
  bool b_enable_depth_buffer_;
  Renderer* ptr_renderer_;

  IViewportSurface* ptr_viewport_ = nullptr;
  ITerrainLighting* ptr_terrain_lighting_ = nullptr;
  TerrainMapSurface surface_terrain_ = TerrainMapSurface::MakeSquareDefault({0, 0});
  Hooks hooks_;
  std::function<void(WorldRenderer&, std::string_view, core::Color, bool)> fn_load_player_palettes_;

  HardwarePalette hardware_palette_;
  std::map<std::string, std::unique_ptr<PaletteReference>, std::less<>> map_palettes_;

  std::vector<IRendererTrait*> vec_renderer_traits_;
  std::vector<IRenderPostProcessPass*> vec_post_process_passes_;

  std::vector<RenderItem> vec_renderables_buffer_;         // 复用(上游 renderablesBuffer)| reused (upstream's renderablesBuffer)
  std::vector<RenderItem> vec_prepared_renderables_;       // 复用 | reused
  std::vector<RenderItem> vec_prepared_overlay_;           // 复用 | reused
  std::vector<RenderItem> vec_prepared_annotation_;        // 复用 | reused
  FrameArena arena_frame_;                                 // OPT-A7:排序/分段 | sort/segmentation

  std::map<std::uint64_t, std::function<void()>> map_palette_invalidated_;  // 订阅表 | subscription table
  std::uint64_t uint8_next_subscription_token_ = 1;
};

}  // namespace ora::gfx
