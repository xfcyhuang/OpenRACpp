// UPSTREAM: OpenRA.Game/Graphics/TerrainSpriteLayer.cs @b6fc03f L21-269(逐方法)
// 地形精灵层:全图静态大 VB(每格 4 顶点),脏行级 SetData 增量上传 +
// 共享静态 IB(每 World 一份,引用计数)+ 调色板失效全图标脏 +
// 地形光照的四角采样 tint。
// OPT-A7(docs/OPTIMIZATION_TRACKER.md)本批待办"地形层 palette_index/tint
// 独立数组"落地:
//   - 上游 UpdatePaletteIndices(L76-88)在调色板行上移时**逐顶点重写整个
//     Vertex 数组**(每顶点一次 48B 结构体构造);C++ 把调色板行索引分离为
//     `vec_palette_`(每格 PaletteReference*)、把地形光照 tint 分离为
//     `vec_corner_tint_`(每顶点 Vector3,FastCreateQuad 时置 (1,1,1)、
//     UpdateTint 写四角采样权重)—— 两者都在**脏行上传点**组合进临时行
//     (c = (p&lt;&lt;16)|(v.c&amp;0xFFFF),RGB = v.RGB × cornerTint),上传字节
//     与上游逐顶点重写**逐字节一致**(OPT-C5 同型论证:写同一 GL_STATIC_DRAW
//     缓冲的同一行区间);
//   - 调色板失效 = 全行标脏(不再有任何逐顶点工作)。
// 形态适配:
//   - map(map.MapSize/CenterOfCell/Grid.Ramps/Ramp/Tiles.Contains)→
//     WorldRenderer 的 TerrainMapSurface 注入面(完整 Map 随 Phase 5;
//     COVERAGE 登记);
//   - Viewport 的可见行范围(Draw 的 CandidateMapCoords)→ first/last 行
//     参数(完整 Viewport 随 Phase 5;Clamp 语义照抄 L215-216);
//   - Update(CPos, ISpriteSequence, …) 重载随序列系统(Phase 5/6);
//   - RenderThread* 可空 = 纯数据模式(脏行跟踪/组合照常,VB/IB 不建、
//     上传与绘制跳过 —— Sheet/HardwarePalette 同族形态);
//   - 上游 ConditionalWeakTable&lt;World, IndexBufferRc&gt; + Lock → 静态
//     map&lt;World*, Rc&gt; + mutex(引擎单线程构造,锁照抄上游形态);
//   - dirtyRows HashSet → 行标志数组(Remove 语义 = 标志清除,集合序不可
//     观测)。
// The terrain sprite layer: the whole-map static VB (4 vertices per cell)
// with dirty-row SetData incremental uploads + a shared static IB (one per
// World, reference counted) + palette-invalidation full re-dirty + the
// terrain-lighting four-corner tint sampling. This batch lands OPT-A7's
// outstanding "terrain-layer palette_index/tint as separate arrays"
// (docs/OPTIMIZATION_TRACKER.md): upstream's UpdatePaletteIndices (L76-88)
// rewrites the entire Vertex array vertex by vertex on every palette-row
// shift (a 48-byte struct construction per vertex); C++ splits the palette
// row index into `vec_palette_` (one PaletteReference* per cell) and the
// terrain-lighting tint into `vec_corner_tint_` (one Vector3 per vertex —
// (1,1,1) from FastCreateQuad, the four-corner sampling weights from
// UpdateTint), composing both into a scratch row at the dirty-row upload
// point (c = (p&lt;&lt;16)|(v.c&amp;0xFFFF), RGB = v.RGB × cornerTint) — the uploaded
// bytes are byte-identical to upstream's vertex-by-vertex rewrite (the
// OPT-C5-style argument: the same row range of the same GL_STATIC_DRAW
// buffer), and palette invalidation becomes "mark all rows dirty" with zero
// per-vertex work. Shape adaptations: the map (MapSize/CenterOfCell/
// Grid.Ramps/Ramp/Tiles.Contains) → the TerrainMapSurface injection surface
// on WorldRenderer (the full Map arrives in Phase 5; registered in COVERAGE);
// the Viewport's visible-row range (Draw's CandidateMapCoords) → the
// first/last row parameters (the full Viewport arrives in Phase 5; the Clamp
// semantics of L215-216 kept verbatim); the Update(CPos, ISpriteSequence, …)
// overload awaits the sequence system (Phase 5/6); a null RenderThread*
// selects the data-only mode (dirty tracking/composition continue, the VB/IB
// are never created, uploads and draws skip — the same family as Sheet/
// HardwarePalette); upstream's ConditionalWeakTable&lt;World, IndexBufferRc&gt; +
// Lock → a static map&lt;World*, Rc&gt; + mutex (single-threaded construction in
// engine; the lock mirrors upstream's shape); dirtyRows' HashSet → a row
// flag array (Remove = clearing the flag; set ordering is unobservable).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "core/wpos.hpp"
#include "gfx/palette.hpp"
#include "gfx/sprite.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/vertex.hpp"
#include "gfx/vertex_buffer.hpp"
#include "gfx/world_renderer.hpp"

namespace ora::sim {
class World;
}

namespace ora::gfx {

class RenderThread;

/// 共享 IB 的引用计数壳(IndexBufferRc,L249-268;定义在 .cpp)。
/// The shared-IB reference-counting shell (IndexBufferRc, L249-268; defined
/// in the .cpp).
struct SharedIndexBufferRc;

/// 地形精灵层(TerrainSpriteLayer.cs;不可拷贝移动)。
/// The terrain sprite layer (TerrainSpriteLayer.cs; non-copyable and
/// non-movable).
class TerrainSpriteLayer {
 public:
  /// ptr_render 为空 = 纯数据模式(见头注)。
  /// A null ptr_render selects the data-only mode (see the header note).
  TerrainSpriteLayer(sim::World& world, WorldRenderer& wr, const Sprite& sprite_empty, BlendMode kind_blend,
                     bool b_restrict_to_bounds, RenderThread* ptr_render);
  ~TerrainSpriteLayer();

  TerrainSpriteLayer(const TerrainSpriteLayer&) = delete;
  TerrainSpriteLayer& operator=(const TerrainSpriteLayer&) = delete;

  BlendMode kind_blend() const { return kind_blend_; }

  /// restrictToBounds 构造参数(上游 Draw 以之选择可见格集合;C++ 行参数化
  /// 后由调用方决定,此值仅存证)。
  /// The restrictToBounds construction parameter (upstream Draw selects the
  /// visible-cell set with it; after the row parameterization the choice
  /// belongs to the caller — this keeps the value on record).
  bool b_restrict_to_bounds() const { return b_restrict_to_bounds_; }

  /// Clear(cell)(L90-93):置空精灵。
  /// Clear(cell) (L90-93): the empty sprite.
  void Clear(CPos cpos_cell);

  /// Update(CPos cell, Sprite, PaletteReference, scale, alpha, ignoreTint)
  /// (L100-110:cell 原点 = 格心 − ramp 抬升,xyz = Screen3DPosition +
  /// scale×(offset − ½size))。
  /// Update(CPos cell, Sprite, PaletteReference, scale, alpha, ignoreTint)
  /// (L100-110: the cell origin = the cell center − the ramp lift, xyz =
  /// Screen3DPosition + scale×(offset − ½size)).
  void Update(CPos cpos_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
              float float_scale = 1.0f, float float_alpha = 1.0f, bool b_ignore_tint = false);

  /// Update(MPos uv, …)(L171-208 逐句;samplers/palette null 规则/界外早退/
  /// FastCreateQuad + 调色板登记/光照 ignoreTint + UpdateTint/脏行)。
  /// Update(MPos uv, …) (L171-208 statement by statement; the samplers/
  /// palette-null rules/out-of-bounds early-out/FastCreateQuad + palette
  /// registration/lighting ignoreTint + UpdateTint/the dirty row).
  void Update(MPos uv_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
              const core::Vector3& vec_pos, float float_scale, float float_alpha, bool b_ignore_tint);

  /// UpdateTint(L112-149:ignoreTint 早退;四角 TintAt 采样写 corner tint;
  /// 脏行)。上游事件 CellChanged 的落点。
  /// UpdateTint (L112-149: the ignoreTint early-out; the four-corner TintAt
  /// sampling writing the corner tint; the dirty row). The landing spot of
  /// upstream's CellChanged event.
  void UpdateTint(MPos uv_cell);

  /// Draw(L210-235):int4_top_left_v/int4_bottom_right_v = 上游可见格范围
  /// 的 CandidateMapCoords TopLeft.V/BottomRight.V(Clamp 语义照抄;
  /// 完整 Viewport 随 Phase 5);行循环含端(L221 的 &lt;=),绘制长度不含端
  /// (L232 的 lastRow − firstRow)—— 上游形态照抄。
  /// Draw (L210-235): int4_top_left_v/int4_bottom_right_v = the
  /// CandidateMapCoords TopLeft.V/BottomRight.V of upstream's visible-cell
  /// region (the Clamp semantics verbatim; the full Viewport arrives in
  /// Phase 5); the row loop is endpoint-inclusive (L221's &lt;=), the draw
  /// length endpoint-exclusive (L232's lastRow − firstRow) — upstream's
  /// shape kept verbatim.
  void Draw(std::int32_t int4_top_left_v, std::int32_t int4_bottom_right_v);

  /// —— 测试面 ——
  /// —— Test surface ——
  bool RowDirtyForTest(std::int32_t int4_row) const {
    return int4_row >= 0 && int4_row < static_cast<std::int32_t>(vecb_dirty_rows_.size()) &&
           vecb_dirty_rows_[static_cast<std::size_t>(int4_row)];
  }
  std::span<const Vertex> VerticesForTest() const { return vec_vertices_; }
  const PaletteReference* PaletteAtForTest(MPos uv_cell) const {
    return vec_palette_[static_cast<std::size_t>(uv_cell.V) * static_cast<std::size_t>(int4_width_) +
                        static_cast<std::size_t>(uv_cell.U)];
  }
  std::span<const core::Vector3> CornerTintForTest() const { return vec_corner_tint_; }
  const std::vector<Vertex>& StagingForTest() const { return vec_staging_row_; }

  /// 组合一行进 staging(Draw 的脏行上传体;亦为测试面 —— 上传字节 = 上游
  /// AOS 逐顶点重写,测试以参考实现逐字节对比)。
  /// Composes one row into the staging buffer (the dirty-row upload body of
  /// Draw; also the test surface — the uploaded bytes = the upstream AOS
  /// vertex-by-vertex rewrite, compared byte-for-byte against a reference
  /// implementation in the tests).
  void ComposeRow(std::int32_t int4_row);

 private:
  /// 上游 GetOrAddSheetIndex(L151-169;满槽 throw "Sheet overflow")。
  /// Upstream's GetOrAddSheetIndex (L151-169; a full set throws "Sheet
  /// overflow").
  std::int32_t GetOrAddSheetIndex(const Sheet* ptr_sheet);

  /// 上游 UpdatePaletteIndices(L76-88;OPT-A7:全行标脏,见头注)。
  /// Upstream's UpdatePaletteIndices (L76-88; OPT-A7: mark every row dirty —
  /// see the header note).
  void UpdatePaletteIndices();

  WorldRenderer& wr_;
  bool b_restrict_to_bounds_ = false;
  Sprite sprite_empty_;
  BlendMode kind_blend_;
  std::int32_t int4_width_ = 0;
  std::int32_t int4_height_ = 0;

  std::array<Sheet*, SpriteRenderer::kSheetCount> arr_sheets_{};

  std::optional<VertexBuffer> opt_vertex_buffer_;   // 纯数据模式空 | empty in data-only mode
  std::vector<Vertex> vec_vertices_;                // W×H×4(c 仅低 16 位)| W×H×4 (c low 16 bits only)
  std::vector<const PaletteReference*> vec_palette_;      // 每格(OPT-A7 分离)| per cell (the OPT-A7 split)
  std::vector<core::Vector3> vec_corner_tint_;            // 每顶点(OPT-A7 分离)| per vertex (the OPT-A7 split)
  std::vector<bool> vecb_ignore_tint_;                    // 仅光照启用 | allocated only with lighting
  std::vector<std::uint8_t> vecb_dirty_rows_;             // 行标志(上游 HashSet)| row flags (upstream's HashSet)
  std::vector<Vertex> vec_staging_row_;                   // 行组合缓冲(复用)| the row compose buffer (reused)
  std::int32_t int4_index_row_stride_ = 0;                // 6×W
  std::int32_t int4_vertex_row_stride_ = 0;               // 4×W

  SharedIndexBufferRc* ptr_shared_index_rc_ = nullptr;   // 静态表内槽 | the slot inside the static table
  std::uint64_t uint8_palette_invalidated_token_ = 0;     // 退订用 | for unsubscribe
  std::uint64_t uint8_cell_changed_token_ = 0;            // 退订用;0 = 未订阅 | for unsubscribe; 0 = not subscribed
};

}  // namespace ora::gfx
