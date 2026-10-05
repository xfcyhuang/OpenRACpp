// UPSTREAM: OpenRA.Game/Graphics/WorldRenderer.cs @b6fc03f L23-483(实现)+
//           OpenRA.Game/Map/MapGrid.cs @b6fc03f L48-57(方格默认 Ramp 角)
// 头文件携带完整 UPSTREAM 锚点、OPT-A7 收集流论证与注入面清单。
// Implementations of WorldRenderer.cs; the header carries the full UPSTREAM
// anchors, the OPT-A7 collection-flow arguments, and the injection list.
#include "gfx/world_renderer.hpp"

#include <cassert>

#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "sim/world.hpp"

namespace ora::gfx {

// ———— TerrainMapSurface:方格默认(MapGrid.cs L48-57 的 Rectangular 角)————
// ———— TerrainMapSurface: the square default (the Rectangular corners of
//      MapGrid.cs L48-57) ————

TerrainMapSurface TerrainMapSurface::MakeSquareDefault(int2 int2_map_size, std::int32_t int4_tile_scale) {
  static constexpr WVec arr_square_corners[4] = {
      WVec{-512, -512, 0}, WVec{512, -512, 0}, WVec{512, 512, 0}, WVec{-512, 512, 0}};

  TerrainMapSurface surface;
  surface.int2_map_size = int2_map_size;
  surface.int4_tile_scale = int4_tile_scale;

  surface.fn_center_of_cell = [](CPos cell) {
    // 方格子格中心公式 = World::TargetFromCell 默认桩同族(cell*1024+512)。
    // The square cell-center formula — the same family as
    // World::TargetFromCell's default stub (cell*1024+512).
    return WPos{cell.X() * 1024 + 512, cell.Y() * 1024 + 512, 0};
  };
  surface.fn_tiles_contains = [int2_map_size](MPos uv) {
    return uv.U >= 0 && uv.U < int2_map_size.X && uv.V >= 0 && uv.V < int2_map_size.Y;
  };
  surface.fn_ramp_center_height_offset = [](CPos) { return 0; };  // 平地 | flat
  surface.fn_ramp_corners = [](CPos) { return std::span<const WVec>{arr_square_corners}; };
  return surface;
}

// ———— 构造/析构(L53-79/L473-482 的注入面形态)————
// ———— Construction/teardown (the injection-surface form of L53-79/L473-482) ————

WorldRenderer::WorldRenderer(sim::World& world, const Desc& desc)
    : world_{world},
      int2_tile_size_{desc.int2_tile_size},
      int4_tile_scale_{desc.int4_tile_scale},
      b_enable_depth_buffer_{desc.b_enable_depth_buffer},
      ptr_renderer_{desc.ptr_renderer},
      hardware_palette_{ptr_renderer_ != nullptr ? &ptr_renderer_->render() : nullptr},
      arena_frame_{64 * 1024} {}

WorldRenderer::~WorldRenderer() {
  // 上游 Dispose 兼带 World.Dispose() 的所有权 HACK(L473-482)不移植:World
  // 生存期归 sim 调用方(COVERAGE 登记);调色板资源随成员 RAII 释放。
  // Upstream Dispose's world-ownership HACK (World.Dispose(), L473-482) is
  // not ported: World's lifetime belongs to the sim caller (registered in
  // COVERAGE); the palette resources release with the RAII members.
}

void WorldRenderer::BeginFrame() {
  for (IRendererTrait* ptr_renderer_trait : vec_renderer_traits_)
    ptr_renderer_trait->BeginFrame();
}

void WorldRenderer::EndFrame() {
  for (IRendererTrait* ptr_renderer_trait : vec_renderer_traits_)
    ptr_renderer_trait->EndFrame();
}

void WorldRenderer::UpdatePalettesForPlayer(std::string_view str_internal_name, core::Color color_player,
                                            bool b_replace_existing) {
  if (fn_load_player_palettes_)
    fn_load_player_palettes_(*this, str_internal_name, color_player, b_replace_existing);
}

// ———— 调色板面(L99-138)————
// ———— The palette faces (L99-138) ————

PaletteReference* WorldRenderer::Palette(std::string_view str_name) {
  // HACK: This is working around the fact that palettes are defined on traits rather than sequences
  // and can be removed once this has been fixed. (WorldRenderer.cs L107-109)
  if (str_name.empty())
    return nullptr;

  const auto it_palette = map_palettes_.find(str_name);
  if (it_palette != map_palettes_.end())
    return it_palette->second.get();

  // CreatePaletteReference(L99-103)。
  // CreatePaletteReference (L99-103).
  const IPalette& palette = hardware_palette_.GetPalette(str_name);
  auto ptr_reference = std::make_unique<PaletteReference>(std::string{str_name},
                                                          hardware_palette_.GetPaletteIndex(str_name), palette,
                                                          hardware_palette_);
  return map_palettes_.emplace(std::string{str_name}, std::move(ptr_reference)).first->second.get();
}

void WorldRenderer::AddPalette(std::string_view str_name, ImmutablePalette palette_p, bool b_allow_modifiers,
                               bool b_allow_overwrite) {
  // WorldRenderer.cs L112-124 逐句。
  // WorldRenderer.cs L112-124 statement by statement.
  if (b_allow_overwrite && hardware_palette_.Contains(str_name)) {
    ReplacePalette(str_name, palette_p);
  } else {
    const std::int32_t int4_old_height = hardware_palette_.Height();
    hardware_palette_.AddPalette(str_name, std::move(palette_p), b_allow_modifiers);

    if (int4_old_height != hardware_palette_.Height())
      for (auto& [uint8_token, fn_listener] : map_palette_invalidated_)
        fn_listener();
  }
}

void WorldRenderer::ReplacePalette(std::string_view str_name, const IPalette& palette_p) {
  hardware_palette_.ReplacePalette(str_name, palette_p);

  // Update cached PlayerReference if one exists (L130-132)
  const auto it_palette = map_palettes_.find(str_name);
  if (it_palette != map_palettes_.end())
    it_palette->second->SetPalette(palette_p);
}

void WorldRenderer::SetPaletteColorShift(std::string_view str_name, float float_hue_offset, float float_sat_offset,
                                         float float_value_multiplier, float float_min_hue, float float_max_hue) {
  hardware_palette_.SetColorShift(str_name, float_hue_offset, float_sat_offset, float_value_multiplier,
                                  float_min_hue, float_max_hue);
}

void WorldRenderer::RefreshPalette(std::span<IPaletteModifier* const> vec_modifiers) {
  hardware_palette_.ApplyModifiers(vec_modifiers);
  if (ptr_renderer_ != nullptr)
    ptr_renderer_->SetPalette(hardware_palette_);
}

std::uint64_t WorldRenderer::SubscribePaletteInvalidated(std::function<void()> fn_listener) {
  const std::uint64_t uint8_token = uint8_next_subscription_token_++;
  map_palette_invalidated_.emplace(uint8_token, std::move(fn_listener));
  return uint8_token;
}

void WorldRenderer::UnsubscribePaletteInvalidated(std::uint64_t uint8_token) {
  map_palette_invalidated_.erase(uint8_token);
}

// ———— 坐标换算(L399-471 逐行;float 化与 (int)/Math.Round 语义照抄)————
// ———— Coordinate conversion (L399-471 line by line; the float promotions and
//      the (int)/Math.Round semantics verbatim) ————

core::Vector2 WorldRenderer::ScreenPosition(const WPos& wpos_pos) const {
  return core::Vector2{static_cast<float>(int2_tile_size_.X) * static_cast<float>(wpos_pos.X) /
                           static_cast<float>(int4_tile_scale_),
                       static_cast<float>(int2_tile_size_.Y) * static_cast<float>(wpos_pos.Y - wpos_pos.Z) /
                           static_cast<float>(int4_tile_scale_)};
}

core::Vector2 WorldRenderer::ScreenPosition(core::Vector2 vec_pos) const {
  return core::Vector2{static_cast<float>(int2_tile_size_.X) * vec_pos.X / static_cast<float>(int4_tile_scale_),
                       static_cast<float>(int2_tile_size_.Y) * vec_pos.Y / static_cast<float>(int4_tile_scale_)};
}

core::Vector3 WorldRenderer::Screen3DPosition(const WPos& wpos_pos) const {
  // The projection from world coordinates to screen coordinates has
  // a non-obvious relationship between the y and z coordinates:
  // * A flat surface with constant y (e.g. a vertical wall) in world coordinates
  //   transforms into a flat surface with constant z (depth) in screen coordinates.
  // * Increasing the world y coordinate increases screen y and z coordinates equally.
  // * Increases the world z coordinate decreases screen y but doesn't change screen z.
  // (WorldRenderer.cs L417-422)
  const float float_z = static_cast<float>(wpos_pos.Y) * static_cast<float>(int2_tile_size_.Y) /
                        static_cast<float>(int4_tile_scale_);
  return core::Vector3{static_cast<float>(int2_tile_size_.X) * static_cast<float>(wpos_pos.X) /
                           static_cast<float>(int4_tile_scale_),
                       static_cast<float>(int2_tile_size_.Y) * static_cast<float>(wpos_pos.Y - wpos_pos.Z) /
                           static_cast<float>(int4_tile_scale_),
                       float_z};
}

int2 WorldRenderer::ScreenPxPosition(const WPos& wpos_pos) const {
  // Round to nearest pixel(Math.Round 默认档 = 就近偶舍入;std::nearbyint
  // 在默认 FE_TONEAREST 下同语义,与 renderer.cpp 的 RoundVector 同约定)。
  // Round to nearest pixel (Math.Round's default = round-half-to-even;
  // std::nearbyint under the default FE_TONEAREST matches — the same
  // convention as renderer.cpp's RoundVector).
  const core::Vector2 vec_px = ScreenPosition(wpos_pos);
  return int2{static_cast<std::int32_t>(std::nearbyint(vec_px.X)),
              static_cast<std::int32_t>(std::nearbyint(vec_px.Y))};
}

core::Vector3 WorldRenderer::Screen3DPxPosition(const WPos& wpos_pos) const {
  // Round to nearest pixel(z 不取整 —— 深度连续;L437-439)。
  // Round to nearest pixel (z stays continuous — depth; L437-439).
  const core::Vector3 vec_px = Screen3DPosition(wpos_pos);
  return core::Vector3{std::nearbyint(vec_px.X), std::nearbyint(vec_px.Y), vec_px.Z};
}

core::Vector3 WorldRenderer::ScreenVectorComponents(const WVec& wvec_vec) const {
  return core::Vector3{static_cast<float>(int2_tile_size_.X) * static_cast<float>(wvec_vec.X) /
                           static_cast<float>(int4_tile_scale_),
                       static_cast<float>(int2_tile_size_.Y) * static_cast<float>(wvec_vec.Y - wvec_vec.Z) /
                           static_cast<float>(int4_tile_scale_),
                       static_cast<float>(int2_tile_size_.Y) * static_cast<float>(wvec_vec.Z) /
                           static_cast<float>(int4_tile_scale_)};
}

std::array<float, 4> WorldRenderer::ScreenVector(const WVec& wvec_vec) const {
  const core::Vector3 vec_xyz = ScreenVectorComponents(wvec_vec);
  return {vec_xyz.X, vec_xyz.Y, vec_xyz.Z, 1.0f};
}

int2 WorldRenderer::ScreenPxOffset(const WVec& wvec_vec) const {
  // Round to nearest pixel(L457-462)。
  // Round to nearest pixel (L457-462).
  const core::Vector3 vec_xyz = ScreenVectorComponents(wvec_vec);
  return int2{static_cast<std::int32_t>(std::nearbyint(vec_xyz.X)),
              static_cast<std::int32_t>(std::nearbyint(vec_xyz.Y))};
}

WPos WorldRenderer::ProjectedPosition(int2 int2_screen_px) const {
  // There are many possible world positions, and the returned value chooses
  // the value with no elevation. (WorldRenderer.cs L464-470;整除向零截断)
  // Many world positions project to the given screen position; the return
  // picks the one with no elevation (WorldRenderer.cs L464-470; integer
  // divisions truncate towards zero).
  return WPos{int4_tile_scale_ * int2_screen_px.X / int2_tile_size_.X,
              int4_tile_scale_ * int2_screen_px.Y / int2_tile_size_.Y, 0};
}

// ———— 收集流(L141-274;OPT-A7)————
// ———— The collection flow (L141-274; OPT-A7) ————

void WorldRenderer::AddRenderable(const RenderItem& item_r) {
  vec_renderables_buffer_.push_back(item_r);
}

void WorldRenderer::AddOverlayRenderable(const RenderItem& item_r) {
  // 上游在生成点即 renderable.PrepareRender(this)(L186/200/210/215)—— 引擎
  // kind 恒等,Custom 在此调虚接口。
  // Upstream PrepareRenders at the generation point (L186/200/210/215) — the
  // identity for engine kinds, the virtual call for Custom here.
  vec_prepared_overlay_.push_back(item_r);
  if (item_r.kind == RenderableKind::Custom)
    vec_prepared_overlay_.back().ptr_custom_finalized = item_r.ptr_custom->PrepareRender(*this);
}

void WorldRenderer::AddAnnotationRenderable(const RenderItem& item_r) {
  vec_prepared_annotation_.push_back(item_r);
  if (item_r.kind == RenderableKind::Custom)
    vec_prepared_annotation_.back().ptr_custom_finalized = item_r.ptr_custom->PrepareRender(*this);
}

void WorldRenderer::GenerateRenderables() {
  // 上游源序(L143-160):onScreenActors → WorldActor → RenderPlayer actor →
  // OrderGenerator → 非分区 effects → ScreenMap 分区 effects。C++ 侧为单一
  // 注入回调(源序归回调内部;Phase 5 trait 面接线)。
  // The upstream source order (L143-160): onScreenActors → WorldActor →
  // the RenderPlayer actor → OrderGenerator → the unpartitioned effects →
  // the ScreenMap partitioned effects. C++ carries a single injected
  // callback (the source order belongs inside it; the Phase 5 trait surface
  // wires up).
  if (hooks_.fn_collect_renderables) {
    const int2 int2_top_left = ptr_viewport_ != nullptr ? ptr_viewport_->TopLeft() : int2{};
    const int2 int2_bottom_right = ptr_viewport_ != nullptr ? ptr_viewport_->BottomRight() : int2{};
    hooks_.fn_collect_renderables(*this, int2_top_left, int2_bottom_right);
  }

  // Renderables must be ordered using a stable sorting algorithm to avoid
  // flickering artefacts (L162;键嵌入收集序后 std::sort 与上游稳定序逐项
  // 一致,见 renderable.hpp)。
  // Renderables must be ordered using a stable sorting algorithm to avoid
  // flickering artefacts (L162; with the collection index embedded, std::sort
  // matches upstream's stable order item for item — see renderable.hpp).
  const std::size_t size_mark = arena_frame_.Mark();
  const std::span<const RenderItem> vec_sorted = SortRenderablesByZ(vec_renderables_buffer_, arena_frame_);

  // PrepareRender(L170-171):引擎 kind 恒等;Custom 走虚接口。
  // PrepareRender (L170-171): the identity for engine kinds; Custom through
  // the virtual interface.
  vec_prepared_renderables_.reserve(vec_prepared_renderables_.size() + vec_sorted.size());
  for (const RenderItem& item_r : vec_sorted) {
    vec_prepared_renderables_.push_back(item_r);
    if (item_r.kind == RenderableKind::Custom)
      vec_prepared_renderables_.back().ptr_custom_finalized = item_r.ptr_custom->PrepareRender(*this);
  }
  arena_frame_.Rewind(size_mark);

  // PERF: Reuse collection to avoid allocations. (L173-174)
  vec_renderables_buffer_.clear();
}

void WorldRenderer::GenerateOverlayRenderables() {
  if (hooks_.fn_collect_overlay) {
    const int2 int2_top_left = ptr_viewport_ != nullptr ? ptr_viewport_->TopLeft() : int2{};
    const int2 int2_bottom_right = ptr_viewport_ != nullptr ? ptr_viewport_->BottomRight() : int2{};
    hooks_.fn_collect_overlay(*this, int2_top_left, int2_bottom_right);
  }
}

void WorldRenderer::GenerateAnnotationRenderables() {
  if (hooks_.fn_collect_annotations) {
    const int2 int2_top_left = ptr_viewport_ != nullptr ? ptr_viewport_->TopLeft() : int2{};
    const int2 int2_bottom_right = ptr_viewport_ != nullptr ? ptr_viewport_->BottomRight() : int2{};
    hooks_.fn_collect_annotations(*this, int2_top_left, int2_bottom_right);
  }
}

void WorldRenderer::PrepareRenderables(std::span<IPaletteModifier* const> vec_palette_modifiers) {
  // 上游 WorldActor 恒存在;C++ 侧 Phase 5 接线前可能未注入 —— null 视作
  // 未就绪跳过(等价于上游 Disposed 早退)。
  // Upstream's WorldActor always exists; before the Phase 5 wiring it may be
  // absent on the C++ side — a null reads as not-ready and skips (the
  // equivalent of upstream's Disposed early-out).
  const sim::Actor* ptr_world_actor = world_.WorldActor();
  if (ptr_world_actor != nullptr && ptr_world_actor->Disposed())
    return;

  RefreshPalette(vec_palette_modifiers);

  GenerateRenderables();
  GenerateOverlayRenderables();
  GenerateAnnotationRenderables();
}

void WorldRenderer::ApplyPostProcessing(PostProcessPassType kind_type) {
  // WorldRenderer.cs L330-340 逐句。
  // WorldRenderer.cs L330-340 statement by statement.
  for (IRenderPostProcessPass* ptr_pass : vec_post_process_passes_) {
    if (ptr_pass->Type() != kind_type || !ptr_pass->Enabled())
      continue;

    if (ptr_renderer_ != nullptr)
      ptr_renderer_->Flush();
    ptr_pass->Draw(*this);
  }
}

// ———— Draw / DrawAnnotations(L276-328 / L342-391)————
// ———— Draw / DrawAnnotations (L276-328 / L342-391) ————

void WorldRenderer::Draw() {
  const sim::Actor* ptr_world_actor = world_.WorldActor();
  if (ptr_world_actor != nullptr && ptr_world_actor->Disposed())
    return;

  if (hooks_.fn_debug_update_depth)
    hooks_.fn_debug_update_depth(*this);

  // 上游 Viewport 恒存在;C++ 注入面未接时跳过 scissor(等价域:无裁剪 =
  // 全屏;Phase 5 Viewport 批次恢复全量语义)。
  // Upstream's Viewport always exists; without the injected surface the
  // scissor is skipped (equivalence domain: no scissor = the full screen; the
  // Phase 5 Viewport batch restores the full semantics).
  if (ptr_viewport_ != nullptr && ptr_renderer_ != nullptr)
    ptr_renderer_->EnableScissor(ptr_viewport_->GetScissorBounds(world_.Type() != sim::WorldType::Editor));

  if (b_enable_depth_buffer_ && ptr_renderer_ != nullptr)
    ptr_renderer_->EnableDepthBuffer();

  if (hooks_.fn_render_terrain && ptr_viewport_ != nullptr)
    hooks_.fn_render_terrain(*this, *ptr_viewport_);

  if (ptr_renderer_ != nullptr)
    ptr_renderer_->Flush();

  for (std::size_t i = 0; i < vec_prepared_renderables_.size(); i++)
    RenderRenderable(vec_prepared_renderables_[i], *this);

  if (b_enable_depth_buffer_ && ptr_renderer_ != nullptr)
    ptr_renderer_->ClearDepthBuffer();

  ApplyPostProcessing(PostProcessPassType::AfterActors);

  if (hooks_.fn_render_above_world)
    hooks_.fn_render_above_world(*this);

  if (b_enable_depth_buffer_ && ptr_renderer_ != nullptr)
    ptr_renderer_->ClearDepthBuffer();

  ApplyPostProcessing(PostProcessPassType::AfterWorld);

  if (hooks_.fn_render_shroud)
    hooks_.fn_render_shroud(*this);

  if (b_enable_depth_buffer_ && ptr_renderer_ != nullptr)
    ptr_renderer_->DisableDepthBuffer();

  if (ptr_renderer_ != nullptr)
    ptr_renderer_->DisableScissor();

  // HACK: Keep old grouping behaviour(L319-323)—— GroupBy(GetType()) 的
  // 两阶稳定序 = kind 计数分段(首遇序 + 段内收集序;见 renderable.hpp)。
  // HACK: Keep old grouping behaviour (L319-323) — GroupBy(GetType())'s
  // two-level stable order = the per-kind counting segmentation
  // (first-encounter between, collection order within; see renderable.hpp).
  {
    const std::size_t size_mark = arena_frame_.Mark();
    const KindSegments segments = SegmentByKind(vec_prepared_overlay_, arena_frame_);
    for (std::int32_t i = 0; i < segments.int4_kind_order_count; ++i) {
      const auto kind = static_cast<std::int32_t>(segments.arr_kind_order[i]);
      const std::int32_t int4_start = segments.arr_starts[kind];
      for (std::int32_t j = 0; j < segments.arr_counts[kind]; ++j)
        RenderRenderable(segments.vec_segmented[int4_start + j], *this);
    }
    arena_frame_.Rewind(size_mark);
  }

  ApplyPostProcessing(PostProcessPassType::AfterShroud);

  if (ptr_renderer_ != nullptr)
    ptr_renderer_->Flush();
}

void WorldRenderer::DrawAnnotations() {
  if (ptr_renderer_ != nullptr)
    ptr_renderer_->EnableAntialiasingFilter();

  for (std::size_t i = 0; i < vec_prepared_annotation_.size(); i++)
    RenderRenderable(vec_prepared_annotation_[i], *this);

  if (ptr_renderer_ != nullptr)
    ptr_renderer_->DisableAntialiasingFilter();

  // Engine debugging overlays(L350-382;debugVis 注入面,默认空)。
  // Engine debugging overlays (L350-382; the debugVis injection surface,
  // empty by default).
  if (hooks_.fn_debug_draw_geometry) {
    for (std::size_t i = 0; i < vec_prepared_renderables_.size(); i++)
      RenderRenderableDebugGeometry(vec_prepared_renderables_[i], *this);
    for (std::size_t i = 0; i < vec_prepared_overlay_.size(); i++)
      RenderRenderableDebugGeometry(vec_prepared_overlay_[i], *this);
    for (std::size_t i = 0; i < vec_prepared_annotation_.size(); i++)
      RenderRenderableDebugGeometry(vec_prepared_annotation_[i], *this);
  }

  if (hooks_.fn_debug_draw_screen_map)
    hooks_.fn_debug_draw_screen_map(*this);

  ApplyPostProcessing(PostProcessPassType::AfterAnnotations);

  if (ptr_renderer_ != nullptr)
    ptr_renderer_->Flush();

  // 帧末:三列表清空 + 帧临时区重置(上游 L388-390 的 clear;OPT-A7 arena)。
  // Frame end: the three lists clear + the frame arena reset (upstream's
  // L388-390 clear; the OPT-A7 arena).
  vec_prepared_renderables_.clear();
  vec_prepared_overlay_.clear();
  vec_prepared_annotation_.clear();
  arena_frame_.Reset();
}

}  // namespace ora::gfx
