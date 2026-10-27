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

enum class PostProcessPassType : std::uint8_t { AfterShroud, AfterWorld, AfterActors, AfterAnnotations };

struct IRenderPostProcessPass {
  virtual PostProcessPassType Type() const = 0;
  virtual bool Enabled() const = 0;
  virtual void Draw(WorldRenderer& wr) = 0;
  virtual ~IRenderPostProcessPass() = default;
};

struct IRendererTrait {
  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;
  virtual ~IRendererTrait() = default;
};

struct ITerrainLighting {
  virtual core::Vector3 TintAt(const WPos& wpos_pos) const = 0;
  virtual std::uint64_t AddCellChangedListener(std::function<void(MPos)> fn_listener) = 0;
  virtual void RemoveCellChangedListener(std::uint64_t uint8_token) = 0;
  virtual ~ITerrainLighting() = default;
};

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

  virtual float Zoom() const { return 1.0f; }
  virtual float MinZoom() const { return 1.0f; }

  virtual Rectangle GetScissorBounds(bool b_inside_bounds) = 0;

  virtual int2 TopLeft() = 0;
  virtual int2 BottomRight() = 0;

  virtual int2 ViewportSize() { return int2{0, 0}; }

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
  std::int32_t int4_tile_scale = 1024;

  std::function<WPos(CPos)> fn_center_of_cell;

  std::function<bool(MPos)> fn_tiles_contains;

  std::function<std::int32_t(CPos)> fn_ramp_center_height_offset;

  std::function<std::span<const WVec>(CPos)> fn_ramp_corners;

  static TerrainMapSurface MakeSquareDefault(int2 int2_map_size, std::int32_t int4_tile_scale = 1024);
};

class WorldRenderer {
 public:
  struct Desc {
    int2 int2_tile_size{24, 24};
    std::int32_t int4_tile_scale = 1024;
    bool b_enable_depth_buffer = false;
    Renderer* ptr_renderer = nullptr;
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
  void GenerateRenderables();
  void GenerateOverlayRenderables();
  void GenerateAnnotationRenderables();
  void ApplyPostProcessing(PostProcessPassType kind_type);

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

  std::vector<RenderItem> vec_renderables_buffer_;
  std::vector<RenderItem> vec_prepared_renderables_;
  std::vector<RenderItem> vec_prepared_overlay_;
  std::vector<RenderItem> vec_prepared_annotation_;
  FrameArena arena_frame_;

  std::map<std::uint64_t, std::function<void()>> map_palette_invalidated_;
  std::uint64_t uint8_next_subscription_token_ = 1;
};

}
