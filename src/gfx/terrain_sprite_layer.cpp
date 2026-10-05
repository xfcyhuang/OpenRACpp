// UPSTREAM: OpenRA.Game/Graphics/TerrainSpriteLayer.cs @b6fc03f L21-269(实现)
// 头文件携带完整 UPSTREAM 锚点与 OPT-A7 分离数组论证。
// Implementations of TerrainSpriteLayer.cs; the header carries the full
// UPSTREAM anchors and the OPT-A7 split-array arguments.
#include "gfx/terrain_sprite_layer.hpp"

#include <cassert>

#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "sim/world.hpp"

namespace ora::gfx {

namespace {

/// 上游 Math.Clamp(int,int,int)。
/// Upstream's Math.Clamp(int, int, int).
std::int32_t ClampInt(std::int32_t int4_v, std::int32_t int4_lo, std::int32_t int4_hi) {
  return std::min(std::max(int4_v, int4_lo), int4_hi);
}

}  // namespace

/// 共享 IB 的引用计数壳(IndexBufferRc,L249-268;AddRef/Dispose 的 count
/// 语义逐行)。静态表 = 上游 ConditionalWeakTable&lt;World, IndexBufferRc&gt;
/// (L24-25;构造/析构全在主线程,mutex 照抄上游 Lock 形态;World 归属键
/// 存于壳内供析构回删)。
/// The shared-IB reference-counting shell (IndexBufferRc, L249-268; the
/// AddRef/Dispose count semantics line by line). The static table = upstream's
/// ConditionalWeakTable&lt;World, IndexBufferRc&gt; (L24-25; construction and
/// teardown stay on the main thread, the mutex mirrors upstream's Lock shape;
/// the owning World key lives in the shell for the teardown erase).
struct SharedIndexBufferRc {
  std::unique_ptr<IndexBuffer> ptr_buffer;
  std::int32_t int4_ref_count = 0;
  const sim::World* ptr_world = nullptr;
};

namespace {

std::mutex& SharedIndexTableLock() {
  static std::mutex mutex_shared;
  return mutex_shared;
}

std::map<const sim::World*, SharedIndexBufferRc>& SharedIndexTable() {
  static std::map<const sim::World*, SharedIndexBufferRc> map_shared;
  return map_shared;
}

}  // namespace

TerrainSpriteLayer::TerrainSpriteLayer(sim::World& world, WorldRenderer& wr, const Sprite& sprite_empty,
                                       BlendMode kind_blend, bool b_restrict_to_bounds, RenderThread* ptr_render)
    : wr_{wr},
      b_restrict_to_bounds_{b_restrict_to_bounds},
      sprite_empty_{sprite_empty},
      kind_blend_{kind_blend} {
  // 上游 world.Map;C++ 经 WorldRenderer 的注入面(Phase 5 接 Map)。
  // Upstream reads world.Map; C++ goes through WorldRenderer's injection
  // surface (Map wires up in Phase 5).
  const TerrainMapSurface& surface = wr.TerrainSurface();
  int4_width_ = surface.int2_map_size.X;
  int4_height_ = surface.int2_map_size.Y;

  int4_vertex_row_stride_ = 4 * int4_width_;
  vec_vertices_.assign(static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_),
                       Vertex{});

  int4_index_row_stride_ = 6 * int4_width_;

  if (ptr_render != nullptr) {
    opt_vertex_buffer_.emplace(*ptr_render, MakeCombinedAttributes(), sizeof(Vertex));
    const std::size_t size_bytes =
        static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_) * sizeof(Vertex);
    std::vector<std::byte> vec_zero(size_bytes);  // 上游 new Vertex[len](零值)| upstream's new Vertex[len] (zeroed)
    opt_vertex_buffer_->InitStatic(vec_zero);

    // 上游 IndexBuffers.GetValue(world, …) + AddRef(L60-64)。
    // Upstream's IndexBuffers.GetValue(world, …) + AddRef (L60-64).
    std::lock_guard lock_shared{SharedIndexTableLock()};
    auto& map_shared = SharedIndexTable();
    auto [it_rc, b_inserted] = map_shared.try_emplace(&world);
    if (b_inserted) {
      it_rc->second.ptr_world = &world;
      it_rc->second.ptr_buffer = std::make_unique<IndexBuffer>(
          *ptr_render,
          CreateQuadIndices(static_cast<std::int32_t>(
              static_cast<std::size_t>(int4_width_) * static_cast<std::size_t>(int4_height_))));
    }
    ptr_shared_index_rc_ = &it_rc->second;
    ++ptr_shared_index_rc_->int4_ref_count;
  }

  vec_palette_.assign(static_cast<std::size_t>(int4_width_) * static_cast<std::size_t>(int4_height_), nullptr);
  vec_corner_tint_.assign(static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_),
                          core::Vector3{1.0f, 1.0f, 1.0f});
  vecb_dirty_rows_.assign(static_cast<std::size_t>(int4_height_), 0);
  uint8_palette_invalidated_token_ = wr_.SubscribePaletteInvalidated([this]() { UpdatePaletteIndices(); });

  if (wr_.TerrainLighting() != nullptr) {
    vecb_ignore_tint_.assign(
        static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_), false);
    uint8_cell_changed_token_ = wr_.TerrainLighting()->AddCellChangedListener([this](MPos uv) { UpdateTint(uv); });
  }
}

TerrainSpriteLayer::~TerrainSpriteLayer() {
  wr_.UnsubscribePaletteInvalidated(uint8_palette_invalidated_token_);
  if (uint8_cell_changed_token_ != 0 && wr_.TerrainLighting() != nullptr)
    wr_.TerrainLighting()->RemoveCellChangedListener(uint8_cell_changed_token_);

  // opt_vertex_buffer_ 随 RAII;共享 IB 引用计数回落(L237-247 的 Dispose 形态;
  // 归零删除并从静态表摘除 —— 上游 ConditionalWeakTable 的弱引用等价物)。
  // opt_vertex_buffer_ rides RAII; the shared-IB refcount falls back (the
  // Dispose shape of L237-247; zeroing deletes and removes the static-table
  // entry — the equivalent of the ConditionalWeakTable's weak reference).
  if (ptr_shared_index_rc_ != nullptr) {
    std::lock_guard lock_shared{SharedIndexTableLock()};
    if (--ptr_shared_index_rc_->int4_ref_count == 0)
      SharedIndexTable().erase(ptr_shared_index_rc_->ptr_world);
  }
}

void TerrainSpriteLayer::Clear(CPos cpos_cell) {
  Update(cpos_cell, nullptr, nullptr, 1.0f, 1.0f, true);
}

void TerrainSpriteLayer::Update(CPos cpos_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
                                float float_scale, float float_alpha, bool b_ignore_tint) {
  // L102-109 逐句(ramp 抬升查询经 TerrainMapSurface)。
  // L102-109 statement by statement (the ramp-lift query through
  // TerrainMapSurface).
  core::Vector3 vec_xyz{};
  if (ptr_sprite != nullptr) {
    const TerrainMapSurface& surface = wr_.TerrainSurface();
    const WPos wpos_cell_origin = surface.fn_center_of_cell(cpos_cell) -
                                  WVec{0, 0, surface.fn_ramp_center_height_offset(cpos_cell)};
    vec_xyz = wr_.Screen3DPosition(wpos_cell_origin) +
              float_scale * (ptr_sprite->vec_offset - 0.5f * ptr_sprite->vec_size);
  }

  Update(cpos_cell.ToMPos(wr_.TerrainSurface().kind_grid_type), ptr_sprite, ptr_palette, vec_xyz, float_scale,
         float_alpha, b_ignore_tint);
}

void TerrainSpriteLayer::UpdateTint(MPos uv_cell) {
  assert(wr_.TerrainLighting() != nullptr);  // 订阅与 Update 均以光照存在为前提 | both the subscription and Update presuppose lighting

  const std::size_t size_offset =
      static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(uv_cell.V) +
      4 * static_cast<std::size_t>(uv_cell.U);
  if (!vecb_ignore_tint_.empty() && vecb_ignore_tint_[size_offset]) {
    // L116-121:上游逐顶点写回 v.A × One(= FastCreateQuad 的原值)并 return
    // —— 组合域等价于 corner tint 复位 One;此分支不标脏(照抄)。
    // L116-121: upstream writes v.A × One back per vertex (= FastCreateQuad's
    // original value) and returns — equivalent, in the composition domain, to
    // resetting the corner tint to One; this branch marks nothing dirty
    // (verbatim).
    for (std::size_t i = 0; i < 4; i++)
      vec_corner_tint_[size_offset + i] = core::Vector3{1.0f, 1.0f, 1.0f};
    return;
  }

  // Allow the terrain tint to vary linearly across the cell to smooth out the staircase effect
  // This is done by sampling the lighting the corners of the sprite, even though those pixels are
  // transparent for isometric tiles (L126-128)
  const ITerrainLighting& lighting = *wr_.TerrainLighting();
  const TerrainMapSurface& surface = wr_.TerrainSurface();
  const WPos wpos_center = surface.fn_center_of_cell(uv_cell.ToCPos(surface.kind_grid_type));
  const std::int32_t int4_step = surface.int4_tile_scale / 2;
  const core::Vector3 arr_weights[4] = {
      lighting.TintAt(wpos_center + WVec{-int4_step, -int4_step, 0}),
      lighting.TintAt(wpos_center + WVec{int4_step, -int4_step, 0}),
      lighting.TintAt(wpos_center + WVec{int4_step, int4_step, 0}),
      lighting.TintAt(wpos_center + WVec{-int4_step, int4_step, 0}),
  };

  // Apply tint directly to the underlying vertices
  // This saves us from having to re-query the sprite information, which has not changed (L140-145)
  // —— OPT-A7:直写分离的 corner tint 数组(组合点在上传;字节等价见头注)。
  // —— OPT-A7: written straight into the split corner-tint array (composed at
  //      the upload; byte equivalence argued in the header).
  for (std::size_t i = 0; i < 4; i++)
    vec_corner_tint_[size_offset + i] = arr_weights[i];

  vecb_dirty_rows_[static_cast<std::size_t>(uv_cell.V)] = 1;
}

std::int32_t TerrainSpriteLayer::GetOrAddSheetIndex(const Sheet* ptr_sheet) {
  // L151-169 逐句。
  // L151-169 statement by statement.
  if (ptr_sheet == nullptr)
    return 0;

  for (std::int32_t i = 0; i < SpriteRenderer::kSheetCount; i++) {
    if (arr_sheets_[static_cast<std::size_t>(i)] == ptr_sheet)
      return i;

    if (arr_sheets_[static_cast<std::size_t>(i)] == nullptr) {
      arr_sheets_[static_cast<std::size_t>(i)] = const_cast<Sheet*>(ptr_sheet);
      return i;
    }
  }

  throw std::runtime_error("Sheet overflow");
}

void TerrainSpriteLayer::Update(MPos uv_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
                                const core::Vector3& vec_pos, float float_scale, float float_alpha,
                                bool b_ignore_tint) {
  // L173-191 逐句。
  // L173-191 statement by statement.
  int2 int2_samplers{0, 0};
  if (ptr_sprite != nullptr) {
    if (ptr_sprite->kind_blend != kind_blend_)
      throw std::runtime_error("Attempted to add sprite with a different blend mode");

    const Sheet* ptr_secondary = nullptr;
    if (ptr_sprite->b_secondary)
      ptr_secondary = static_cast<const SpriteWithSecondaryData*>(ptr_sprite)->ptr_secondary_sheet;
    int2_samplers = int2{GetOrAddSheetIndex(ptr_sprite->ptr_sheet), GetOrAddSheetIndex(ptr_secondary)};

    // PERF: Remove useless palette assignments for RGBA sprites
    // HACK: This is working around the limitation that palettes are defined on traits rather than on sequences,
    // and can be removed once this has been fixed (L181-185)
    if (ptr_sprite->kind_channel == TextureChannel::RGBA &&
        !(ptr_palette != nullptr && ptr_palette->HasColorShift()))
      ptr_palette = nullptr;
  } else {
    ptr_sprite = &sprite_empty_;
    int2_samplers = int2{0, 0};
  }

  // The vertex buffer does not have geometry for cells outside the map (L193-195)
  if (!wr_.TerrainSurface().fn_tiles_contains(uv_cell))
    return;

  const std::size_t size_offset =
      static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(uv_cell.V) +
      4 * static_cast<std::size_t>(uv_cell.U);

  // OPT-A7:FastCreateQuad 以 palette=0 写低 16 位;调色板行在上传点组合
  // (上游以 palette?.TextureIndex ?? 0 整写 c —— 组合字节等价,见头注)。
  // OPT-A7: FastCreateQuad writes the low 16 bits with palette = 0; the
  // palette row composes at the upload point (upstream writes c whole with
  // palette?.TextureIndex ?? 0 — the composed bytes are identical; see the
  // header).
  FastCreateQuad(std::span<Vertex>(vec_vertices_).subspan(size_offset, 4), vec_pos, *ptr_sprite, int2_samplers, 0, 0,
                 float_scale * ptr_sprite->vec_size, float_alpha * core::Vector3{1.0f, 1.0f, 1.0f}, float_alpha);
  vec_palette_[static_cast<std::size_t>(uv_cell.V) * static_cast<std::size_t>(int4_width_) +
               static_cast<std::size_t>(uv_cell.U)] = ptr_palette;
  for (std::size_t i = 0; i < 4; i++)
    vec_corner_tint_[size_offset + i] = core::Vector3{1.0f, 1.0f, 1.0f};

  if (wr_.TerrainLighting() != nullptr) {
    vecb_ignore_tint_[size_offset] = b_ignore_tint;
    UpdateTint(uv_cell);
  }

  vecb_dirty_rows_[static_cast<std::size_t>(uv_cell.V)] = 1;
}

void TerrainSpriteLayer::UpdatePaletteIndices() {
  // OPT-A7:上游逐顶点重写 palette 位(L76-88)→ 全行标脏(上传点以
  // PaletteReference 现值组合,字节等价;见头注论证)。
  // OPT-A7: upstream rewrites the palette bits vertex by vertex (L76-88) →
  // mark every row dirty (the upload composes with the PaletteReference's
  // current value, byte-identical; see the header argument).
  for (std::int32_t int4_row = 0; int4_row < int4_height_; int4_row++)
    vecb_dirty_rows_[static_cast<std::size_t>(int4_row)] = 1;
}

void TerrainSpriteLayer::ComposeRow(std::int32_t int4_row) {
  const std::size_t size_row_start =
      static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_row);
  vec_staging_row_.resize(static_cast<std::size_t>(int4_vertex_row_stride_));
  for (std::int32_t i = 0; i < int4_vertex_row_stride_; i++) {
    const std::size_t size_index = size_row_start + static_cast<std::size_t>(i);
    const Vertex& vertex_v = vec_vertices_[size_index];

    // c = (p & 0xFFFF) << 16 | (v.C & 0xFFFF)(上游 UpdatePaletteIndices L82)
    // —— p 现值即"等价重写"。
    // c = (p & 0xFFFF) << 16 | (v.C & 0xFFFF) (upstream
    // UpdatePaletteIndices L82) — p's current value is the "equivalent
    // rewrite".
    const std::uint32_t uint4_p = static_cast<std::uint32_t>(
        vec_palette_[size_index / 4] != nullptr ? vec_palette_[size_index / 4]->TextureIndex() : 0);
    Vertex vertex_out = vertex_v;
    vertex_out.c = ((uint4_p & 0xFFFF) << 16) | (vertex_v.c & 0xFFFF);

    // RGB = v.RGB × cornerTint(上游 UpdateTint L145:v.A × weights;因
    // FastCreateQuad 已写 RGB = alpha,两域重合)。
    // RGB = v.RGB × cornerTint (upstream UpdateTint L145: v.A × weights;
    // FastCreateQuad already wrote RGB = alpha, so the two domains coincide).
    const core::Vector3& vec_tint = vec_corner_tint_[size_index];
    vertex_out.r = vertex_v.r * vec_tint.X;
    vertex_out.g = vertex_v.g * vec_tint.Y;
    vertex_out.b = vertex_v.b * vec_tint.Z;

    vec_staging_row_[static_cast<std::size_t>(i)] = vertex_out;
  }
}

void TerrainSpriteLayer::Draw(std::int32_t int4_top_left_v, std::int32_t int4_bottom_right_v) {
  // Only draw the rows that are visible. (L214-216;上游行源 = viewport 的
  // CandidateMapCoords,此处行参数化 —— 见头注。)
  // Only draw the rows that are visible. (L214-216; upstream's row source is
  // the viewport's CandidateMapCoords, parameterized as rows here — see the
  // header note.)
  const std::int32_t int4_first_row = ClampInt(int4_top_left_v, 0, int4_height_);
  const std::int32_t int4_last_row = ClampInt(int4_bottom_right_v + 1, int4_first_row, int4_height_);

  if (wr_.RendererPtr() != nullptr)
    wr_.RendererPtr()->Flush();

  // Flush any visible changes to the GPU (L220-228;行循环含端)
  // Flush any visible changes to the GPU (L220-228; the row loop includes
  // the endpoint)
  for (std::int32_t int4_row = int4_first_row; int4_row <= int4_last_row; int4_row++) {
    // 端行可达 H(上游 lastRow = (V+1).Clamp(…, H)),HashSet.Remove 对未登记
    // 行是 no-op —— 标志数组以界检查等价(合法 update 的行号恒 < H)。
    // The endpoint row may reach H (upstream lastRow = (V+1).Clamp(…, H));
    // HashSet.Remove of an absent row is a no-op — the flag array mirrors
    // that with a bounds check (legitimate updates always carry V < H).
    if (int4_row >= int4_height_ || !vecb_dirty_rows_[static_cast<std::size_t>(int4_row)])
      continue;

    ComposeRow(int4_row);
    const std::size_t size_row_offset =
        static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_row);
    if (opt_vertex_buffer_.has_value())
      opt_vertex_buffer_->UpdateStaticSubData(static_cast<std::uint32_t>(size_row_offset * sizeof(Vertex)),
                                              std::as_bytes(std::span<const Vertex>{vec_staging_row_}));
    vecb_dirty_rows_[static_cast<std::size_t>(int4_row)] = 0;
  }

  // L230-232:整批绘制(绘制长度不含端)。
  // L230-232: the whole-batch draw (the draw length excludes the endpoint).
  if (wr_.RendererPtr() != nullptr && opt_vertex_buffer_.has_value() && ptr_shared_index_rc_ != nullptr)
    wr_.RendererPtr()->WorldSpriteRenderer().DrawVertexBuffer(
        *opt_vertex_buffer_, *ptr_shared_index_rc_->ptr_buffer, int4_index_row_stride_ * int4_first_row,
        int4_index_row_stride_ * (int4_last_row - int4_first_row), std::span<Sheet* const>{arr_sheets_},
        kind_blend_);

  if (wr_.RendererPtr() != nullptr)
    wr_.RendererPtr()->Flush();
}

}  // namespace ora::gfx
