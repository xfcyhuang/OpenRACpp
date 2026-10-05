// UPSTREAM: OpenRA.Game/Graphics/WorldRenderer.cs @b6fc03f L23-483(逐方法)+
//           OpenRA.Game/Traits/TraitsInterfaces.cs @b6fc03f L435-476(IRenderTerrain/
//           ITerrainLighting/IRenderPostProcessPass/PostProcessPassType)+
//           OpenRA.Game/Map/MapGrid.cs @b6fc03f L20-57(默认 Ramp 角的方格形态)
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
struct IViewportSurface {
  virtual int2 WorldToViewPx(int2 int2_world) = 0;
  virtual int2 WorldToViewPx(const core::Vector3& vec_world) = 0;

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
  virtual ~IViewportSurface() = default;
};

/// Map 的地形渲染面(TerrainSpriteLayer/MarkerTileRenderable 所需的 Map/
/// Grid/Ramp 查询;完整 Map 随 Phase 5)。默认实现 = 方格网格的中心/包含/
/// 平地 Ramp(MapGrid.cs L48-57 的 Rectangular 角)。
/// The Map's terrain-render surface (the Map/Grid/Ramp queries needed by
/// TerrainSpriteLayer/MarkerTileRenderable; the full Map arrives in Phase
/// 5). The default implementation = the square grid's cell centers /
/// containment / flat ramps (the Rectangular corners of MapGrid.cs L48-57).
struct TerrainMapSurface {
  int2 int2_map_size{0, 0};
  MapGridType kind_grid_type = MapGridType::Rectangular;
  std::int32_t int4_tile_scale = 1024;  // map.Grid.TileScale(世界单位/格)| world units per cell

  /// map.CenterOfCell(默认 = cell*1024 + 512,与 World::TargetFromCell 的
  /// 默认桩同族)。
  /// map.CenterOfCell (default = cell*1024 + 512, the same family as
  /// World::TargetFromCell's default stub).
  std::function<WPos(CPos)> fn_center_of_cell;

  /// map.Tiles.Contains(uv)(默认 = 矩形包含)。
  /// map.Tiles.Contains(uv) (default = rectangular containment).
  std::function<bool(MPos)> fn_tiles_contains;

  /// map.Grid.Ramps[map.Ramp[cell]].CenterHeightOffset(默认 0 = 平地)。
  /// map.Grid.Ramps[map.Ramp[cell]].CenterHeightOffset (default 0 = flat).
  std::function<std::int32_t(CPos)> fn_ramp_center_height_offset;

  /// map.Grid.Ramps[map.Ramp[cell]].Corners(恰 4 角;默认 = 方格 ±512)。
  /// map.Grid.Ramps[map.Ramp[cell]].Corners (exactly 4; default = the
  /// square ±512).
  std::function<std::span<const WVec>(CPos)> fn_ramp_corners;

  /// 方格默认面(静态便捷;回调为按值捕获的默认实现)。
  /// The square-grid default surface (a static convenience; the callbacks
  /// capture the defaults by value).
  static TerrainMapSurface MakeSquareDefault(int2 int2_map_size, std::int32_t int4_tile_scale = 1024);
};

/// 世界渲染器(WorldRenderer.cs;上游 sealed)。
/// The world renderer (WorldRenderer.cs; sealed upstream).
class WorldRenderer {
 public:
  /// 构造参数(上游从 ModData/Map 的反射链取得;见头注注入面清单)。
  /// The construction parameters (what upstream pulls from the ModData/Map
  /// reflection chain; see the injection list in the header note).
  struct Desc {
    int2 int2_tile_size{24, 24};  // map.Rules.TerrainInfo.TileSize | TerrainInfo.TileSize
    std::int32_t int4_tile_scale = 1024;
    bool b_enable_depth_buffer = false;  // mapGrid.EnableDepthBuffer | mapGrid.EnableDepthBuffer
    Renderer* ptr_renderer = nullptr;    // Game.Renderer | Game.Renderer
  };

  /// Phase 5 的 trait 收集面(默认 no-op;见头注)。
  /// The Phase 5 trait-collection surfaces (no-op by default; see the
  /// header note).
  struct Hooks {
    /// GenerateRenderables 的渲染源(上游 L141-160:onScreenActors/WorldActor/
    /// RenderPlayer/OrderGenerator/两类 effects;回调内经 AddRenderable 入流)。
    /// The render sources of GenerateRenderables (upstream L141-160:
    /// onScreenActors/WorldActor/RenderPlayer/OrderGenerator/the two effect
    /// families; the callback feeds AddRenderable).
    std::function<void(WorldRenderer&, int2, int2)> fn_collect_renderables;

    /// GenerateOverlayRenderables(L178-216)与 GenerateAnnotationRenderables
    /// (L219-257)的 trait/effect 源(回调内经 Add*Renderable 入流)。
    /// The trait/effect sources of GenerateOverlayRenderables (L178-216) and
    /// GenerateAnnotationRenderables (L219-257) (the callbacks feed the
    /// Add*Renderable entries).
    std::function<void(WorldRenderer&, int2, int2)> fn_collect_overlay;
    std::function<void(WorldRenderer&, int2, int2)> fn_collect_annotations;

    /// IRenderTerrain.RenderTerrain(L289)。
    std::function<void(WorldRenderer&, IViewportSurface&)> fn_render_terrain;

    /// IRenderAboveWorld(L301-305)与 IRenderShroud(L312)的 trait 分发。
    /// The IRenderAboveWorld (L301-305) and IRenderShroud (L312) trait
    /// dispatches.
    std::function<void(WorldRenderer&)> fn_render_above_world;
    std::function<void(WorldRenderer&)> fn_render_shroud;

    /// DebugVisualizations 的深度缓冲更新(L281)与几何/ScreenMap 调试绘制
    /// (L350-382;Phase 5 诊断 trait)。
    /// DebugVisualizations' depth-buffer update (L281) and the geometry/
    /// ScreenMap debug drawing (L350-382; the Phase 5 diagnostics trait).
    std::function<void(WorldRenderer&)> fn_debug_update_depth;
    std::function<void(WorldRenderer&)> fn_debug_draw_geometry;
    std::function<void(WorldRenderer&)> fn_debug_draw_screen_map;
  };

  WorldRenderer(sim::World& world, const Desc& desc);
  ~WorldRenderer();

  WorldRenderer(const WorldRenderer&) = delete;
  WorldRenderer& operator=(const WorldRenderer&) = delete;

  // ———— 常量面(L28-32/53-79 的注入等价物)————
  // ———— The constant faces (the injected equivalents of L28-32/53-79) ————
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

  /// BeginFrame/EndFrame(L81-91:IRenderer trait 逐个)。
  /// BeginFrame/EndFrame (L81-91: over the IRenderer traits).
  void BeginFrame();
  void EndFrame();
  void SetRendererTraits(std::span<IRendererTrait* const> vec_renderers) {
    vec_renderer_traits_.assign(vec_renderers.begin(), vec_renderers.end());
  }
  void SetPostProcessPasses(std::span<IRenderPostProcessPass* const> vec_passes) {
    vec_post_process_passes_.assign(vec_passes.begin(), vec_passes.end());
  }

  /// UpdatePalettesForPlayer(L93-97;ILoadsPlayerPalettes trait 族 Phase 5,
  /// 订阅面注入)。
  /// UpdatePalettesForPlayer (L93-97; the ILoadsPlayerPalettes trait family
  /// is Phase 5, injected as a subscription surface).
  void SetPlayerPaletteLoader(
      std::function<void(WorldRenderer&, std::string_view, core::Color, bool)> fn_load) {
    fn_load_player_palettes_ = std::move(fn_load);
  }
  void UpdatePalettesForPlayer(std::string_view str_internal_name, core::Color color_player,
                               bool b_replace_existing);

  // ———— 调色板面(L99-138)————
  // ———— The palette faces (L99-138) ————

  /// Palette(name)(L105-110;空名 → null;GetOrAdd 缓存)。
  /// Palette(name) (L105-110; empty name → null; GetOrAdd cached).
  PaletteReference* Palette(std::string_view str_name);

  /// AddPalette(L112-124;allowOverwrite 且已存在走 ReplacePalette;高度
  /// 增长才触发 PaletteInvalidated)。
  /// AddPalette (L112-124; allowOverwrite + present routes through
  /// ReplacePalette; only a height growth fires PaletteInvalidated).
  void AddPalette(std::string_view str_name, ImmutablePalette palette_p, bool b_allow_modifiers = false,
                  bool b_allow_overwrite = false);

  /// ReplacePalette(L126-133;缓存引用同步)。
  /// ReplacePalette (L126-133; cached references sync).
  void ReplacePalette(std::string_view str_name, const IPalette& palette_p);

  void SetPaletteColorShift(std::string_view str_name, float float_hue_offset, float float_sat_offset,
                            float float_value_multiplier, float float_min_hue, float float_max_hue);

  /// RefreshPalette(L393-397):modifier 应用 + Renderer.SetPalette;modifier
  /// 表由调用方收集(上游 WorldActor.TraitsImplementing&lt;IPaletteModifier&gt;)。
  /// RefreshPalette (L393-397): applies the modifiers and calls
  /// Renderer.SetPalette; the modifier table is collected by the caller
  /// (upstream WorldActor.TraitsImplementing&lt;IPaletteModifier&gt;).
  void RefreshPalette(std::span<IPaletteModifier* const> vec_modifiers);

  HardwarePalette& hardware_palette() { return hardware_palette_; }

  /// PaletteInvalidated 事件(L34;token 订阅表,成对注销)。
  /// The PaletteInvalidated event (L34; a token subscription table, paired
  /// unsubscribe).
  std::uint64_t SubscribePaletteInvalidated(std::function<void()> fn_listener);
  void UnsubscribePaletteInvalidated(std::uint64_t uint8_token);

  // ———— 坐标换算面(L399-471 逐行)————
  // ———— The coordinate-conversion faces (L399-471 line by line) ————
  core::Vector2 ScreenPosition(const WPos& wpos_pos) const;
  core::Vector2 ScreenPosition(core::Vector2 vec_pos) const;
  core::Vector3 Screen3DPosition(const WPos& wpos_pos) const;
  int2 ScreenPxPosition(const WPos& wpos_pos) const;
  core::Vector3 Screen3DPxPosition(const WPos& wpos_pos) const;
  core::Vector3 ScreenVectorComponents(const WVec& wvec_vec) const;
  std::array<float, 4> ScreenVector(const WVec& wvec_vec) const;
  int2 ScreenPxOffset(const WVec& wvec_vec) const;
  WPos ProjectedPosition(int2 int2_screen_px) const;

  // ———— 收集流(OPT-A7;上游 L44-51 的三列表 + L141-274)————
  // ———— The collection flow (OPT-A7; upstream's three lists of L44-51 plus
  //        L141-274) ————

  /// 收集入口(上游 renderablesBuffer 的 AddRange 落点;Overlay/Annotation
  /// 上游在生成点即 PrepareRender —— 引擎 kind 恒等,Custom 调虚接口)。
  /// The collection entries (the AddRange landing spots of upstream's
  /// renderablesBuffer; Overlay/Annotation PrepareRender at the generation
  /// point upstream — the identity for engine kinds, the virtual call for
  /// Custom).
  void AddRenderable(const RenderItem& item_r);
  void AddOverlayRenderable(const RenderItem& item_r);
  void AddAnnotationRenderable(const RenderItem& item_r);

  /// PrepareRenderables(L259-274):refresh → 收集(注入源)→ 排序 →
  /// Prepare → 清缓冲。
  /// PrepareRenderables (L259-274): refresh → collect (the injected
  /// sources) → sort → prepare → clear the buffer.
  void PrepareRenderables(std::span<IPaletteModifier* const> vec_palette_modifiers);

  /// Draw(L276-328:scissor → depth → terrain → prepared → 后处理/aboveWorld/
  /// shroud → overlay 分组 → Flush)。
  /// Draw (L276-328: scissor → depth → terrain → prepared → the post-
  /// process/aboveWorld/shroud interleaves → the overlay grouping → Flush).
  void Draw();

  /// DrawAnnotations(L342-391:抗锯齿档 → annotation 绘制 → 调试钩子 →
  /// 后处理 → Flush → 清三列表)。
  /// DrawAnnotations (L342-391: the antialiasing toggle → the annotation
  /// drawing → the debug hooks → post process → Flush → clearing the three
  /// lists).
  void DrawAnnotations();

  Hooks& hooks() { return hooks_; }

  /// OPT-A7 帧临时区(排序 key/分段;DrawAnnotations 末 rewind)。
  /// The OPT-A7 frame arena (sort keys/segmentation; rewound at the end of
  /// DrawAnnotations).
  FrameArena& frame_arena() { return arena_frame_; }

  /// —— 测试面:三列表与收集缓冲观察 ——
  /// —— Test surface: the three lists and the collection buffer ——
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
