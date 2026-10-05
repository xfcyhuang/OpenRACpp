// UPSTREAM: OpenRA.Game/Graphics/Renderable.cs @b6fc03f L18-60(接口族 + TintModifiers)+
//           OpenRA.Game/Graphics/SpriteRenderable.cs @b6fc03f L18-131 +
//           OpenRA.Game/Graphics/UISpriteRenderable.cs @b6fc03f L17-79 +
//           OpenRA.Game/Graphics/TargetLineRenderable.cs @b6fc03f L19-77 +
//           OpenRA.Game/Graphics/MarkerTileRenderable.cs @b6fc03f L17-55 +
//           OpenRA.Game/Graphics/WorldRenderer.cs @b6fc03f L25-26/141-175(排序键公式)
// 可渲染物族。上游是接口 + 每次调用 With* 都 new 一个堆对象的不可变类;
// C++ 侧按 OPT-A7(docs/OPTIMIZATION_TRACKER.md)以 **RenderItem POD(带 kind
// 判别)** 承载引擎内四种具体 renderable —— With* 返回按值拷贝,零分配;
// 自定义(mod 侧)renderable 走 IRenderable/IFinalizedRenderable 虚接口
// (Phase 5 trait 面),经 Custom 槽非拥有指针进入收集流。
// OPT-A7 落地对照(上游 WorldRenderer.cs L141-175/319-323):
//   - 上游每帧:renderablesBuffer 装箱对象 + keys 数组 + span.Sort 双联动 +
//     PrepareRender 再一次堆分配(多数实现返回 this);
//   - C++:收集进复用 vector<RenderItem>(值语义),排序在 FrameArena 上做
//     (key<<32|index 的 uint64 数组 + std::sort;键嵌入 index 后全序唯一,
//     与上游"稳定排序防闪烁"的 (key,i) 序逐项一致),PrepareRender 对四种
//     引擎 kind 为恒等(上游 SpriteRenderable/UISprite/TargetLine/MarkerTile
//     的 PrepareRender 全部 `return this`);
//   - 上游 Draw 的 overlay 段 LINQ GroupBy(GetType()) HACK("Keep old grouping
//     behaviour")→ 计数分段:一遍按 kind 计数 + 记录 kind 首遇序,二遍稳定
//     散进 FrameArena 分段 —— 段间序 = kind 首遇序,段内序 = 收集序,与
//     GroupBy 的两阶稳定序逐项一致,零分配。
// 形态适配:
//   - Game.Renderer 静态 → WorldRenderer 注入的 Renderer 引用(上游为进程
//     级单例,等价);
//   - TargetLine 的 IEnumerable<WPos>/LINQ → span(调用方保证生存期;上游
//     委托枚举同为惰性引用);
//   - MarkerTile 的 map.Grid.Ramps 查询 → WorldRenderer 的 TerrainMapSurface
//     注入面(Map 随 Phase 5);
//   - WithZOffset/OffsetBy/AsDecoration 在 UI/TargetLine/MarkerTile 上游本就
//     返回 this(C# 引用相等)—— POD 按值拷贝,可观测行为一致。
// The renderable family. Upstream models this as interfaces plus immutable
// classes whose every With* call news a heap object; per OPT-A7
// (docs/OPTIMIZATION_TRACKER.md) the C++ side carries the four engine
// concrete renderables as a **RenderItem POD (with a kind discriminator)** —
// With* returns by-value copies, zero allocation — while custom (mod-side)
// renderables keep the IRenderable/IFinalizedRenderable virtual interfaces
// (the Phase 5 trait surface) entering the collection flow as non-owning
// pointers in the Custom slot. OPT-A7 mapping (upstream
// WorldRenderer.cs L141-175/319-323): upstream allocates boxed objects into
// renderablesBuffer per frame, sorts with a keys-array/span.Sort pairing,
// then PrepareRender heap-allocates once more (most implementations return
// this); C++ collects into a reused vector<RenderItem> (value semantics),
// sorts on the FrameArena (a uint64 array of key<<32|index + std::sort; the
// embedded index makes the order total and unique, matching upstream's
// "(key, i) stable sort against flickering" item for item), and PrepareRender
// is the identity for the four engine kinds (upstream
// SpriteRenderable/UISprite/TargetLine/MarkerTile all `return this`). The
// overlay stage's LINQ GroupBy(GetType()) HACK ("Keep old grouping
// behaviour") becomes counting segmentation: one pass counts per kind and
// records first-encounter kind order, a second pass stably scatters into
// FrameArena segments — between segments the first-encounter order, within a
// segment the collection order, item-for-item equal to GroupBy's two-level
// stable order, with zero allocation. Shape adaptations: the Game.Renderer
// static → the Renderer reference injected into WorldRenderer (upstream is a
// process-wide singleton, equivalent); TargetLine's IEnumerable<WPos>/LINQ →
// a span (the caller guarantees the lifetime; upstream's delegate enumeration
// is equally a lazy reference); MarkerTile's map.Grid.Ramps lookup → the
// TerrainMapSurface injection surface on WorldRenderer (Map arrives in Phase
// 5); WithZOffset/OffsetBy/AsDecoration return this upstream on
// UI/TargetLine/MarkerTile (C# reference equality) — a POD by-value copy is
// observably identical.
#pragma once
import std;

#include "core/arena.hpp"
#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "core/wangle.hpp"
#include "core/wpos.hpp"
#include "core/wvec.hpp"
#include "gfx/sprite.hpp"

namespace ora::gfx {

class WorldRenderer;
class PaletteReference;
struct IRenderable;
struct IFinalizedRenderable;

/// TintModifiers(Renderable.cs L37-43;[Flags])。
/// TintModifiers (Renderable.cs L37-43; [Flags]).
enum class TintModifiers : std::uint8_t {
  None = 0,
  IgnoreWorldTint = 1,
  ReplaceColor = 2,
};

constexpr TintModifiers operator&(TintModifiers kind_a, TintModifiers kind_b) {
  return static_cast<TintModifiers>(static_cast<std::uint8_t>(kind_a) &
                                    static_cast<std::uint8_t>(kind_b));
}
constexpr TintModifiers operator|(TintModifiers kind_a, TintModifiers kind_b) {
  return static_cast<TintModifiers>(static_cast<std::uint8_t>(kind_a) |
                                    static_cast<std::uint8_t>(kind_b));
}

/// IRenderable(Renderable.cs L18-29):自定义(mod 侧)renderable 的虚接口;
/// 引擎内四种具体 renderable 走 RenderItem POD,不经此接口。
/// IRenderable (Renderable.cs L18-29): the virtual interface of custom
/// (mod-side) renderables; the four engine renderables travel as the
/// RenderItem POD and never pass through this interface.
struct IRenderable {
  virtual WPos Pos() const = 0;
  virtual std::int32_t ZOffset() const = 0;
  virtual bool IsDecoration() const = 0;

  virtual IRenderable* WithZOffset(std::int32_t int4_new_offset) = 0;
  virtual IRenderable* OffsetBy(const WVec& vec_offset) = 0;
  virtual IRenderable* AsDecoration() = 0;

  virtual IFinalizedRenderable* PrepareRender(WorldRenderer& wr) = 0;
  virtual ~IRenderable() = default;
};

/// IFinalizedRenderable(Renderable.cs L55-60)。
/// IFinalizedRenderable (Renderable.cs L55-60).
struct IFinalizedRenderable {
  virtual void Render(WorldRenderer& wr) = 0;
  virtual void RenderDebugGeometry(WorldRenderer& wr) = 0;
  virtual Rectangle ScreenBounds(WorldRenderer& wr) = 0;
  virtual ~IFinalizedRenderable() = default;
};

/// RenderItem 的 kind 判别(= 上游具体 renderable 的 GetType();分段键)。
/// The RenderItem kind discriminator (= upstream's renderable GetType(); the
/// segmentation key).
enum class RenderableKind : std::uint8_t {
  Sprite,
  UISprite,
  TargetLine,
  MarkerTile,
  Custom,  // IRenderable/IFinalizedRenderable 虚接口(mod 侧)| the virtual interfaces (mod side)
  Count,   // 计数哨兵 | the counting sentinel
};

inline constexpr std::int32_t kRenderableKindCount =
    static_cast<std::int32_t>(RenderableKind::Count);

/// 引擎内 renderable 的 POD 载荷(全字段超集,按 kind 解释;平凡可析构 ——
/// FrameArena 域)。指针全部非拥有:Sprite 归 SpriteCache,PaletteReference
/// 归 WorldRenderer,waypoints 归调用方,custom 归其 trait。
/// The POD payload of the engine renderables (a field superset interpreted
/// per kind; trivially destructible — FrameArena domain). Every pointer is
/// non-owning: Sprites belong to the SpriteCache, PaletteReferences to the
/// WorldRenderer, waypoints to the caller, custom to its trait.
struct RenderItem {
  RenderableKind kind = RenderableKind::Sprite;
  TintModifiers kind_tint_mods = TintModifiers::None;
  bool b_is_decoration = false;

  // —— Sprite / UISprite ——
  const Sprite* ptr_sprite = nullptr;
  const PaletteReference* ptr_palette = nullptr;  // 上游可空(RGBA 无色移置 null)| nullable upstream (RGBA without shift becomes null)
  WPos wpos_pos{};                                // Sprite:Pos 属性 = pos + offset;UISprite:effectiveWorldPos | Sprite: the Pos property = pos + offset; UISprite: effectiveWorldPos
  WVec wvec_offset{};                             // 仅 Sprite(上游 UI 恒 Zero)| Sprite only (UI is always Zero)
  std::int32_t int4_z_offset = 0;
  float float_scale = 1.0f;
  float float_alpha = 1.0f;
  core::Vector3 vec_tint{1.0f, 1.0f, 1.0f};
  WAngle wangle_rotation{};
  core::Vector2 vec_screen_pos{};  // 仅 UISprite | UISprite only
  float float_ui_rotation = 0.0f;  // 仅 UISprite(上游 rotation 为 float 弧度)| UISprite only (upstream's float-radian rotation)

  // —— TargetLine ——
  std::span<const WPos> vec_waypoints{};
  core::Color color_color{};
  std::int32_t int4_width = 0;
  std::int32_t int4_marker_size = 0;

  // —— MarkerTile ——
  CPos cpos_cell{};

  // —— Custom(mod 侧虚接口;PrepareRender 后 finalized 生效)——
  // —— Custom (mod-side virtual interfaces; finalized takes effect after
  //      PrepareRender) ——
  IRenderable* ptr_custom = nullptr;
  IFinalizedRenderable* ptr_custom_finalized = nullptr;

  /// 上游 Pos 属性(Sprite = pos + Offset;UI/TargetLine/MarkerTile 的字段
  /// 即 Pos;Custom 走虚接口)。
  /// The upstream Pos property (Sprite = pos + Offset; the stored field is
  /// Pos for UI/TargetLine/MarkerTile; Custom goes through the interface).
  WPos Pos() const {
    return kind == RenderableKind::Sprite ? wpos_pos + wvec_offset : wpos_pos;
  }
};

static_assert(std::is_trivially_destructible_v<RenderItem>,
              "RenderItem 须平凡可析构(FrameArena 域)| must be trivially "
              "destructible (FrameArena domain)");

/// RenderableZPositionComparisonKey(WorldRenderer.cs L25-26:
/// r.Pos.Y + r.Pos.Z + r.ZOffset;int 回绕 -fwrapv,与上游 unchecked long
/// 装载前的 int 加法一致)。空 TargetLine 的 Pos = waypoints.First() 上游抛
/// InvalidOperationException("Sequence contains no elements")—— 等价抛。
/// RenderableZPositionComparisonKey (WorldRenderer.cs L25-26:
/// r.Pos.Y + r.Pos.Z + r.ZOffset; int wraparound under -fwrapv, matching the
/// int additions upstream performs before the unchecked long lift). An empty
/// TargetLine's Pos = waypoints.First() throws InvalidOperationException
/// upstream ("Sequence contains no elements") — thrown equivalently here.
std::int32_t RenderableZSortKey(const RenderItem& item_r);

// ———— SpriteRenderable(SpriteRenderable.cs L18-131;POD 工厂 + With* 按值)————
// ———— SpriteRenderable (SpriteRenderable.cs L18-131; POD factories + by-value With*) ————

/// 构造(SpriteRenderable.cs L27-47 逐句;RGBA 且无色移的调色板置 null 的
/// PERF/HACK 规则照抄)。
/// Construction (SpriteRenderable.cs L27-47 statement by statement; the
/// PERF/HACK rule nulling the palette of shiftless RGBA sprites kept
/// verbatim).
RenderItem MakeSpriteRenderable(const Sprite& sprite_r, WPos wpos_pos, WVec wvec_offset,
                                std::int32_t int4_z_offset, const PaletteReference* ptr_palette,
                                float float_scale, float float_alpha, core::Vector3 vec_tint,
                                TintModifiers kind_tint_mods, bool b_is_decoration,
                                WAngle wangle_rotation = WAngle{});

/// With*(L63-91):按值改一字段返拷贝(上游 new 对象的零分配等价物)。
/// The With* family (L63-91): copies with one field changed (the
/// allocation-free equivalent of upstream's new object).
RenderItem SpriteRenderableWithPalette(const RenderItem& item_r, const PaletteReference* ptr_palette);
RenderItem SpriteRenderableWithZOffset(const RenderItem& item_r, std::int32_t int4_new_offset);
RenderItem SpriteRenderableOffsetBy(const RenderItem& item_r, const WVec& vec_offset);
RenderItem SpriteRenderableAsDecoration(const RenderItem& item_r);
RenderItem SpriteRenderableWithAlpha(const RenderItem& item_r, float float_new_alpha);
RenderItem SpriteRenderableWithTint(const RenderItem& item_r, core::Vector3 vec_new_tint,
                                    TintModifiers kind_new_tint_mods);

/// ScreenPosition(SpriteRenderable.cs L93-97:0.5*scale*Size 的 (int) 截断在
/// x/y,分量级照抄)。
/// ScreenPosition (SpriteRenderable.cs L93-97: the (int) truncations of
/// 0.5*scale*Size land on x/y, component-level verbatim).
core::Vector3 SpriteRenderableScreenPosition(const RenderItem& item_r, const WorldRenderer& wr);

/// ScreenBounds(L126-130)。
/// ScreenBounds (L126-130).
Rectangle SpriteRenderableScreenBounds(const RenderItem& item_r, const WorldRenderer& wr);

// ———— UISpriteRenderable(UISpriteRenderable.cs L17-79)————
// ———— UISpriteRenderable (UISpriteRenderable.cs L17-79) ————

RenderItem MakeUISpriteRenderable(const Sprite& sprite_r, WPos wpos_effective_world_pos,
                                  core::Vector2 vec_screen_pos, std::int32_t int4_z_offset,
                                  const PaletteReference* ptr_palette, float float_scale = 1.0f,
                                  float float_alpha = 1.0f, float float_rotation = 0.0f);
Rectangle UISpriteRenderableScreenBounds(const RenderItem& item_r);

// ———— TargetLineRenderable(TargetLineRenderable.cs L19-77)————
// ———— TargetLineRenderable (TargetLineRenderable.cs L19-77) ————

RenderItem MakeTargetLineRenderable(std::span<const WPos> vec_waypoints, core::Color color_c,
                                    std::int32_t int4_width, std::int32_t int4_marker_size);
/// OffsetBy(TargetLineRenderable.cs L40-45):逐点平移;上游惰性 Select,这里
/// 物化进调用方存储(vec_out 至少 waypoints.size(),返回的 item 引用之)。
/// OffsetBy (TargetLineRenderable.cs L40-45): the point-wise shift; upstream
/// Selects lazily, here materialized into the caller's storage (vec_out holds
/// at least waypoints.size(); the returned item references it).
RenderItem TargetLineRenderableOffsetBy(const RenderItem& item_r, const WVec& vec_offset,
                                        std::span<WPos> vec_out);

// ———— MarkerTileRenderable(MarkerTileRenderable.cs L17-55)————
// ———— MarkerTileRenderable (MarkerTileRenderable.cs L17-55) ————

RenderItem MakeMarkerTileRenderable(CPos cpos_pos, core::Color color_c);

// ———— 收集流的纯逻辑件(OPT-A7;FrameArena 承载)————
// ———— The pure-logic pieces of the collection flow (OPT-A7; FrameArena-carried) ————

/// 排序(WorldRenderer.cs L162-168 逐语义):键 = (int64(zkey) << 32) | i,
/// i = 收集序 —— 键全序唯一,故 std::sort 与上游 keys.Sort(内嵌 i 的稳定
/// 序)逐项一致;返回 FrameArena 上的排序副本(调用方保证生存期到帧末)。
/// Sorting (WorldRenderer.cs L162-168 verbatim semantics): key =
/// (int64(zkey) << 32) | i with i the collection index — keys are total and
/// unique, so std::sort matches upstream's keys.Sort (the stable order with
/// the embedded i) item for item; returns the sorted copy on the FrameArena
/// (the caller keeps it alive until frame end).
std::span<const RenderItem> SortRenderablesByZ(std::span<const RenderItem> vec_items, FrameArena& arena);

/// kind 计数分段的结果:段连续存放,kind 首遇序即段序。
/// The result of the per-kind counting segmentation: segments stored
/// contiguously, in first-encounter kind order.
struct KindSegments {
  std::span<const RenderItem> vec_segmented;  // FrameArena 上 | on the FrameArena
  std::array<std::int32_t, kRenderableKindCount> arr_counts{};   // 每 kind 项数 | per-kind counts
  std::array<std::int32_t, kRenderableKindCount> arr_starts{};   // 每 kind 段起点(-1 = 未出现)| per-kind segment start (-1 = absent)
  std::int32_t int4_kind_order_count = 0;                        // 首遇 kind 数 | the count of first-encountered kinds
  RenderableKind arr_kind_order[kRenderableKindCount];           // 首遇序 | first-encounter order
};

/// 计数分段(上游 overlay 的 GroupBy(GetType()) 等价;两遍:计数 + 首遇序,
/// 然后稳定散布)。段内序 = 收集序。
/// Counting segmentation (the equivalent of upstream's overlay
/// GroupBy(GetType()); two passes: counting + first-encounter order, then a
/// stable scatter). Within a segment the order is the collection order.
KindSegments SegmentByKind(std::span<const RenderItem> vec_items, FrameArena& arena);

/// 单项绘制分发(kind switch;Custom 走 finalized 虚接口)。
/// The single-item draw dispatch (a kind switch; Custom goes through the
/// finalized virtual interface).
void RenderRenderable(const RenderItem& item_r, WorldRenderer& wr);

/// 单项调试几何分发(SpriteRenderable.cs L115-124 / UISpriteRenderable.cs
/// L64-72;TargetLine/MarkerTile 上游为空实现)。
/// The single-item debug-geometry dispatch (SpriteRenderable.cs L115-124 /
/// UISpriteRenderable.cs L64-72; TargetLine/MarkerTile are empty upstream).
void RenderRenderableDebugGeometry(const RenderItem& item_r, WorldRenderer& wr);

/// 单项屏幕包围盒(上游 ScreenBounds;TargetLine/MarkerTile = Empty)。
/// The single-item screen bounds (upstream ScreenBounds; TargetLine/
/// MarkerTile = Empty).
Rectangle RenderableScreenBounds(const RenderItem& item_r, WorldRenderer& wr);

}  // namespace ora::gfx
