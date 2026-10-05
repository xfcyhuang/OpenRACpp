// UPSTREAM: OpenRA.Game/Graphics/Renderable.cs @b6fc03f L18-60 +
//           OpenRA.Game/Graphics/SpriteRenderable.cs @b6fc03f L18-131 +
//           OpenRA.Game/Graphics/UISpriteRenderable.cs @b6fc03f L17-79 +
//           OpenRA.Game/Graphics/TargetLineRenderable.cs @b6fc03f L19-77 +
//           OpenRA.Game/Graphics/MarkerTileRenderable.cs @b6fc03f L17-55 +
//           OpenRA.Game/Graphics/WorldRenderer.cs @b6fc03f L25-26/141-175/319-323
// 实现(头文件携带完整 UPSTREAM 锚点与 OPT-A7 论证)。
// Implementations (the headers carry the full UPSTREAM anchors and the
// OPT-A7 arguments).
#include "gfx/renderable.hpp"

#include <cassert>

#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/world_renderer.hpp"

namespace ora::gfx {

namespace {

/// int2 → Vector3 的 (x, y, 0) 形态(上游 ToVector3 的 z=0 分支)。
/// The (x, y, 0) form of int2 → Vector3 (upstream's ToVector3 with z = 0).
core::Vector3 ToVector3(int2 v) {
  return core::Vector3{static_cast<float>(v.X), static_cast<float>(v.Y), 0.0f};
}

/// 上游 Color.Red(FFFF0000)。
/// Upstream's Color.Red (FFFF0000).
const core::Color kColorRed = core::Color::FromArgb(255, 255, 0, 0);

}  // namespace

// ———— 排序键(WorldRenderer.cs L25-26;空 TargetLine 的 First() 等价抛)————
// ———— The sort key (WorldRenderer.cs L25-26; the empty-TargetLine First()
//      equivalent throw) ————

std::int32_t RenderableZSortKey(const RenderItem& item_r) {
  if (item_r.kind == RenderableKind::Custom)
    return item_r.ptr_custom->Pos().Y + item_r.ptr_custom->Pos().Z + item_r.ptr_custom->ZOffset();
  if (item_r.kind == RenderableKind::TargetLine && item_r.vec_waypoints.empty())
    throw std::runtime_error("Sequence contains no elements");
  return item_r.Pos().Y + item_r.Pos().Z + item_r.int4_z_offset;
}

// ———— SpriteRenderable(SpriteRenderable.cs L18-131)————
// ———— SpriteRenderable (SpriteRenderable.cs L18-131) ————

RenderItem MakeSpriteRenderable(const Sprite& sprite_r, WPos wpos_pos, WVec wvec_offset,
                                std::int32_t int4_z_offset, const PaletteReference* ptr_palette,
                                float float_scale, float float_alpha, core::Vector3 vec_tint,
                                TintModifiers kind_tint_mods, bool b_is_decoration, WAngle wangle_rotation) {
  RenderItem item_r;
  item_r.kind = RenderableKind::Sprite;
  item_r.ptr_sprite = &sprite_r;
  item_r.wpos_pos = wpos_pos;
  item_r.wvec_offset = wvec_offset;
  item_r.int4_z_offset = int4_z_offset;
  item_r.ptr_palette = ptr_palette;
  item_r.float_scale = float_scale;
  item_r.float_alpha = float_alpha;
  item_r.vec_tint = vec_tint;
  item_r.kind_tint_mods = kind_tint_mods;
  item_r.b_is_decoration = b_is_decoration;
  item_r.wangle_rotation = wangle_rotation;

  // PERF: Remove useless palette assignments for RGBA sprites
  // HACK: This is working around the fact that palettes are defined on traits rather than on sequences
  // and can be removed once this has been fixed (SpriteRenderable.cs L42-46)
  if (sprite_r.kind_channel == TextureChannel::RGBA &&
      !(ptr_palette != nullptr && ptr_palette->HasColorShift()))
    item_r.ptr_palette = nullptr;

  return item_r;
}

RenderItem SpriteRenderableWithPalette(const RenderItem& item_r, const PaletteReference* ptr_palette) {
  RenderItem item_out = item_r;
  item_out.ptr_palette = ptr_palette;
  // 上游 WithPalette 直接 new(不再过 RGBA 置 null 规则;SpriteRenderable.cs L63-66)
  // Upstream's WithPalette news directly (the RGBA-null rule does not rerun;
  // SpriteRenderable.cs L63-66).
  return item_out;
}

RenderItem SpriteRenderableWithZOffset(const RenderItem& item_r, std::int32_t int4_new_offset) {
  RenderItem item_out = item_r;
  item_out.int4_z_offset = int4_new_offset;
  return item_out;
}

RenderItem SpriteRenderableOffsetBy(const RenderItem& item_r, const WVec& vec_offset) {
  RenderItem item_out = item_r;
  item_out.wpos_pos = item_r.wpos_pos + vec_offset;
  return item_out;
}

RenderItem SpriteRenderableAsDecoration(const RenderItem& item_r) {
  RenderItem item_out = item_r;
  item_out.b_is_decoration = true;
  return item_out;
}

RenderItem SpriteRenderableWithAlpha(const RenderItem& item_r, float float_new_alpha) {
  RenderItem item_out = item_r;
  item_out.float_alpha = float_new_alpha;
  return item_out;
}

RenderItem SpriteRenderableWithTint(const RenderItem& item_r, core::Vector3 vec_new_tint,
                                    TintModifiers kind_new_tint_mods) {
  RenderItem item_out = item_r;
  item_out.vec_tint = vec_new_tint;
  item_out.kind_tint_mods = kind_new_tint_mods;
  return item_out;
}

core::Vector3 SpriteRenderableScreenPosition(const RenderItem& item_r, const WorldRenderer& wr) {
  // SpriteRenderable.cs L93-97:注意 pos 是构造字段(未加 Offset),Offset 经
  // ScreenPxOffset 单独叠加;(int) 截断仅落在 x/y。
  // SpriteRenderable.cs L93-97: note pos is the construction field (Offset
  // not yet added) — Offset lands separately via ScreenPxOffset; the (int)
  // truncations hit x/y only.
  const core::Vector3 vec_s = 0.5f * item_r.float_scale * item_r.ptr_sprite->vec_size;
  return wr.Screen3DPxPosition(item_r.wpos_pos) + ToVector3(wr.ScreenPxOffset(item_r.wvec_offset)) -
         core::Vector3{static_cast<float>(static_cast<std::int32_t>(vec_s.X)),
                       static_cast<float>(static_cast<std::int32_t>(vec_s.Y)), vec_s.Z};
}

Rectangle SpriteRenderableScreenBounds(const RenderItem& item_r, const WorldRenderer& wr) {
  const core::Vector3 vec_screen_offset = SpriteRenderableScreenPosition(item_r, wr) + item_r.ptr_sprite->vec_offset;
  return BoundingRectangle(vec_screen_offset, item_r.ptr_sprite->vec_size,
                           item_r.wangle_rotation.RendererRadians());
}

// ———— UISpriteRenderable(UISpriteRenderable.cs L17-79)————
// ———— UISpriteRenderable (UISpriteRenderable.cs L17-79) ————

RenderItem MakeUISpriteRenderable(const Sprite& sprite_r, WPos wpos_effective_world_pos,
                                  core::Vector2 vec_screen_pos, std::int32_t int4_z_offset,
                                  const PaletteReference* ptr_palette, float float_scale, float float_alpha,
                                  float float_rotation) {
  RenderItem item_r;
  item_r.kind = RenderableKind::UISprite;
  item_r.ptr_sprite = &sprite_r;
  item_r.wpos_pos = wpos_effective_world_pos;  // Pos 属性直存 | the Pos property stored directly
  item_r.vec_screen_pos = vec_screen_pos;
  item_r.int4_z_offset = int4_z_offset;
  item_r.ptr_palette = ptr_palette;
  item_r.float_scale = float_scale;
  item_r.float_alpha = float_alpha;
  item_r.float_ui_rotation = float_rotation;

  // PERF: Remove useless palette assignments for RGBA sprites
  // HACK: This is working around the limitation that palettes are defined on traits rather than on sequences,
  // and can be removed once this has been fixed (UISpriteRenderable.cs L37-41)
  if (sprite_r.kind_channel == TextureChannel::RGBA &&
      !(ptr_palette != nullptr && ptr_palette->HasColorShift()))
    item_r.ptr_palette = nullptr;

  // 上游 IsDecoration 恒 true / Offset 恒 Zero / With* 返回 this —— POD 按值
  // 拷贝即等价(见 renderable.hpp 头注)。
  // Upstream's IsDecoration is always true, Offset always Zero, and the With*
  // family returns this — the POD by-value copy is equivalent (see the
  // renderable.hpp header note).
  item_r.b_is_decoration = true;
  return item_r;
}

Rectangle UISpriteRenderableScreenBounds(const RenderItem& item_r) {
  const core::Vector3 vec_offset =
      core::Vector3{item_r.vec_screen_pos.X, item_r.vec_screen_pos.Y, 0.0f} + item_r.ptr_sprite->vec_offset;
  return BoundingRectangle(vec_offset, item_r.ptr_sprite->vec_size, item_r.float_ui_rotation);
}

// ———— TargetLineRenderable(TargetLineRenderable.cs L19-77)————
// ———— TargetLineRenderable (TargetLineRenderable.cs L19-77) ————

RenderItem MakeTargetLineRenderable(std::span<const WPos> vec_waypoints, core::Color color_c,
                                    std::int32_t int4_width, std::int32_t int4_marker_size) {
  RenderItem item_r;
  item_r.kind = RenderableKind::TargetLine;
  item_r.vec_waypoints = vec_waypoints;
  item_r.color_color = color_c;
  item_r.int4_width = int4_width;
  item_r.int4_marker_size = int4_marker_size;
  item_r.int4_z_offset = 0;  // 上游 ZOffset 恒 0,WithZOffset 返回 this | ZOffset is always 0 upstream; WithZOffset returns this
  item_r.b_is_decoration = true;
  return item_r;
}

RenderItem TargetLineRenderableOffsetBy(const RenderItem& item_r, const WVec& vec_offset,
                                        std::span<WPos> vec_out) {
  // 上游 OffsetBy 惰性 Select(w => w + offset)逐点平移(TargetLineRenderable.cs
  // L40-45);C++ 物化进调用方存储,waypoints 生存期随帧内调用方。
  // Upstream's OffsetBy lazily Selects (w => w + offset) point-wise
  // (TargetLineRenderable.cs L40-45); C++ materializes into the caller's
  // storage, with the waypoint lifetime owned by the in-frame caller.
  assert(vec_out.size() >= item_r.vec_waypoints.size());
  RenderItem item_out = item_r;
  for (std::size_t i = 0; i < item_r.vec_waypoints.size(); ++i)
    vec_out[i] = item_r.vec_waypoints[i] + vec_offset;
  item_out.vec_waypoints = {vec_out.data(), item_r.vec_waypoints.size()};
  return item_out;
}

// ———— MarkerTileRenderable(MarkerTileRenderable.cs L17-55)————
// ———— MarkerTileRenderable (MarkerTileRenderable.cs L17-55) ————

RenderItem MakeMarkerTileRenderable(CPos cpos_pos, core::Color color_c) {
  RenderItem item_r;
  item_r.kind = RenderableKind::MarkerTile;
  item_r.cpos_cell = cpos_pos;
  item_r.color_color = color_c;
  item_r.int4_z_offset = 0;  // WithZOffset/OffsetBy/AsDecoration 上游返回 this | these return this upstream
  item_r.b_is_decoration = true;
  return item_r;
}

// ———— 排序 + 分段(OPT-A7;WorldRenderer.cs L162-168/319-323)————
// ———— Sorting + segmentation (OPT-A7; WorldRenderer.cs L162-168/319-323) ————

std::span<const RenderItem> SortRenderablesByZ(std::span<const RenderItem> vec_items, FrameArena& arena) {
  struct KeyIdx {
    std::int64_t int8_key;
    std::uint32_t uint4_index;
  };

  const std::size_t size_count = vec_items.size();
  KeyIdx* arr_keys = static_cast<KeyIdx*>(arena.Allocate(sizeof(KeyIdx) * size_count, alignof(KeyIdx)));
  for (std::size_t i = 0; i < size_count; ++i)
    arr_keys[i] = KeyIdx{(static_cast<std::int64_t>(RenderableZSortKey(vec_items[i])) << 32) +
                             static_cast<std::int32_t>(i),
                         static_cast<std::uint32_t>(i)};

  // 键嵌入收集序 ⇒ 全序唯一 ⇒ std::sort 与上游 keys.Sort 的稳定结果逐项一致
  // (WorldRenderer.cs L162-168;"+ i" 上游为 int 加法,这里窄化安全:i < 2^31)。
  // The keys embed the collection index ⇒ total and unique ⇒ std::sort
  // matches upstream's keys.Sort stable result item for item
  // (WorldRenderer.cs L162-168; upstream's "+ i" is an int addition — the
  // narrowing is safe: i < 2^31).
  std::sort(arr_keys, arr_keys + size_count, [](const KeyIdx& a, const KeyIdx& b) {
    return a.int8_key != b.int8_key ? a.int8_key < b.int8_key : a.uint4_index < b.uint4_index;
  });

  RenderItem* arr_out = static_cast<RenderItem*>(arena.Allocate(sizeof(RenderItem) * size_count, alignof(RenderItem)));
  for (std::size_t i = 0; i < size_count; ++i)
    arr_out[i] = vec_items[arr_keys[i].uint4_index];
  return {arr_out, size_count};
}

KindSegments SegmentByKind(std::span<const RenderItem> vec_items, FrameArena& arena) {
  KindSegments segments;
  segments.arr_starts.fill(-1);

  // 第一遍:每 kind 计数 + 首遇序(GroupBy 的组序)。
  // Pass one: per-kind counts + first-encounter order (GroupBy's group
  // order).
  for (const RenderItem& item_r : vec_items) {
    const auto kind = static_cast<std::int32_t>(item_r.kind);
    if (kind < kRenderableKindCount && segments.arr_counts[kind] == 0)
      segments.arr_kind_order[segments.int4_kind_order_count++] = item_r.kind;
    if (kind < kRenderableKindCount)
      ++segments.arr_counts[kind];
  }

  // 段起点 = 首遇序前缀和(未出现的 kind 不占段)。
  // Segment starts = prefix sums over the first-encounter order (absent
  // kinds take no segment).
  std::array<std::int32_t, kRenderableKindCount> arr_cursor{};
  std::int32_t int4_offset = 0;
  for (std::int32_t i = 0; i < segments.int4_kind_order_count; ++i) {
    const auto kind = static_cast<std::int32_t>(segments.arr_kind_order[i]);
    segments.arr_starts[kind] = int4_offset;
    arr_cursor[kind] = int4_offset;
    int4_offset += segments.arr_counts[kind];
  }

  // 第二遍:稳定散布(段内序 = 收集序)。
  // Pass two: the stable scatter (within a segment, the collection order).
  RenderItem* arr_out =
      static_cast<RenderItem*>(arena.Allocate(sizeof(RenderItem) * vec_items.size(), alignof(RenderItem)));
  for (const RenderItem& item_r : vec_items) {
    const auto kind = static_cast<std::int32_t>(item_r.kind);
    if (kind >= kRenderableKindCount)
      continue;  // 未知 kind(前向兼容哨兵)| an unknown kind (forward-compat sentinel)
    arr_out[arr_cursor[kind]++] = item_r;
  }

  segments.vec_segmented = {arr_out, vec_items.size()};
  return segments;
}

// ———— 绘制分发(WorldRenderer.cs L293-294/320-323/345-359 的单项循环体)————
// ———— The draw dispatch (the per-item bodies of WorldRenderer.cs
//      L293-294/320-323/345-359) ————

void RenderRenderable(const RenderItem& item_r, WorldRenderer& wr) {
  switch (item_r.kind) {
    case RenderableKind::Sprite: {
      // SpriteRenderable.Render(L100-113 逐句)。
      // SpriteRenderable.Render (L100-113 statement by statement).
      SpriteRenderer& renderer_sprite = wr.RendererPtr()->WorldSpriteRenderer();
      core::Vector3 vec_t = item_r.float_alpha * item_r.vec_tint;
      const ITerrainLighting* ptr_lighting = wr.TerrainLighting();
      if (ptr_lighting != nullptr && (item_r.kind_tint_mods & TintModifiers::IgnoreWorldTint) == TintModifiers::None)
        vec_t = vec_t * ptr_lighting->TintAt(item_r.wpos_pos);

      // Shader interprets negative alpha as a flag to use the tint colour directly instead of multiplying the sprite colour
      float float_a = item_r.float_alpha;
      if ((item_r.kind_tint_mods & TintModifiers::ReplaceColor) != TintModifiers::None)
        float_a *= -1.0f;

      renderer_sprite.DrawSprite(*item_r.ptr_sprite, item_r.ptr_palette, SpriteRenderableScreenPosition(item_r, wr),
                                 item_r.float_scale, vec_t, float_a, item_r.wangle_rotation.RendererRadians());
      break;
    }

    case RenderableKind::UISprite: {
      // UISpriteRenderable.Render(L59-62;Game.Renderer.SpriteRenderer =
      // UI 侧 RgbaSpriteRenderer)。
      // UISpriteRenderable.Render (L59-62; Game.Renderer.SpriteRenderer =
      // the UI-side RgbaSpriteRenderer).
      wr.RendererPtr()->UIRgbaSpriteRenderer().DrawSprite(
          *item_r.ptr_sprite, core::Vector3{item_r.vec_screen_pos.X, item_r.vec_screen_pos.Y, 0.0f},
          item_r.float_scale, core::Vector3{1.0f, 1.0f, 1.0f}, item_r.float_alpha, item_r.float_ui_rotation);
      break;
    }

    case RenderableKind::TargetLine: {
      // TargetLineRenderable.Render(L50-65)+ DrawTargetMarker(L67-73)。
      // TargetLineRenderable.Render (L50-65) + DrawTargetMarker (L67-73).
      if (item_r.vec_waypoints.empty())
        break;

      IViewportSurface& viewport = *wr.Viewport();
      RgbaColorRenderer& renderer_color = wr.RendererPtr()->UIRgbaColorRenderer();

      const auto ToScreen = [&](const WPos& wpos_waypoint) {
        return ToVector3(viewport.WorldToViewPx(wr.Screen3DPosition(wpos_waypoint)));
      };

      core::Vector3 vec_first = ToScreen(item_r.vec_waypoints.front());
      core::Vector3 vec_a = vec_first;
      for (std::size_t i = 1; i < item_r.vec_waypoints.size(); ++i) {
        const core::Vector3 vec_b = ToScreen(item_r.vec_waypoints[i]);
        renderer_color.DrawLine(vec_a, vec_b, static_cast<float>(item_r.int4_width), item_r.color_color);
        const core::Vector3 vec_offset{static_cast<float>(item_r.int4_marker_size),
                                       static_cast<float>(item_r.int4_marker_size), 0.0f};
        renderer_color.FillRect(vec_b - vec_offset, vec_b + vec_offset, item_r.color_color);
        vec_a = vec_b;
      }

      const core::Vector3 vec_offset{1.0f, 1.0f, 0.0f};  // 默认 size = 1 | the default size = 1
      renderer_color.FillRect(vec_first - vec_offset, vec_first + vec_offset, item_r.color_color);
      break;
    }

    case RenderableKind::MarkerTile: {
      // MarkerTileRenderable.Render(L42-51;map 查询经 TerrainMapSurface)。
      // MarkerTileRenderable.Render (L42-51; the map queries go through
      // TerrainMapSurface).
      const TerrainMapSurface& surface = wr.TerrainSurface();
      const std::int32_t int4_ramp_offset = surface.fn_ramp_center_height_offset(item_r.cpos_cell);
      const WPos wpos_corner_base =
          surface.fn_center_of_cell(item_r.cpos_cell) - WVec{0, 0, int4_ramp_offset};

      core::Vector3 arr_screen[4];
      const std::span<const WVec> vec_corners = surface.fn_ramp_corners(item_r.cpos_cell);
      for (std::size_t i = 0; i < 4 && i < vec_corners.size(); ++i)
        arr_screen[i] = ToVector3(wr.Viewport()->WorldToViewPx(wr.Screen3DPosition(wpos_corner_base + vec_corners[i])));

      if (vec_corners.size() >= 4)  // 上游恒 4 角 | always 4 corners upstream
        wr.RendererPtr()->UIRgbaColorRenderer().FillRect(arr_screen[0], arr_screen[1], arr_screen[2], arr_screen[3],
                                                         item_r.color_color);
      break;
    }

    case RenderableKind::Custom:
      assert(item_r.ptr_custom_finalized != nullptr);  // PrepareRenderables 已 Prepare | prepared by PrepareRenderables
      if (item_r.ptr_custom_finalized != nullptr)
        item_r.ptr_custom_finalized->Render(wr);
      break;

    default:
      break;
  }
}

void RenderRenderableDebugGeometry(const RenderItem& item_r, WorldRenderer& wr) {
  switch (item_r.kind) {
    case RenderableKind::Sprite: {
      // SpriteRenderable.RenderDebugGeometry(L115-124)。
      // SpriteRenderable.RenderDebugGeometry (L115-124).
      const core::Vector3 vec_pos = SpriteRenderableScreenPosition(item_r, wr) + item_r.ptr_sprite->vec_offset;
      const core::Vector3 vec_tl = ToVector3(wr.Viewport()->WorldToViewPx(vec_pos));
      const core::Vector3 vec_br = ToVector3(wr.Viewport()->WorldToViewPx(vec_pos + item_r.ptr_sprite->vec_size));
      if (item_r.wangle_rotation.Angle == 0) {
        wr.RendererPtr()->UIRgbaColorRenderer().DrawRect(vec_tl, vec_br, 1.0f, kColorRed);
      } else {
        core::Vector3 arr_quad[4];
        RotateQuadInto(arr_quad, vec_tl, vec_br - vec_tl, item_r.wangle_rotation.RendererRadians());
        wr.RendererPtr()->UIRgbaColorRenderer().DrawPolygon(arr_quad, 1.0f, kColorRed);
      }
      break;
    }

    case RenderableKind::UISprite: {
      // UISpriteRenderable.RenderDebugGeometry(L64-72)。
      // UISpriteRenderable.RenderDebugGeometry (L64-72).
      const core::Vector3 vec_offset = core::Vector3{
          item_r.vec_screen_pos.X + item_r.ptr_sprite->vec_offset.X,
          item_r.vec_screen_pos.Y + item_r.ptr_sprite->vec_offset.Y, 0.0f};
      const core::Vector3 vec_size{item_r.ptr_sprite->vec_size.X, item_r.ptr_sprite->vec_size.Y, 0.0f};
      if (item_r.float_ui_rotation == 0.0f) {
        wr.RendererPtr()->UIRgbaColorRenderer().DrawRect(vec_offset, vec_offset + vec_size, 1.0f, kColorRed);
      } else {
        core::Vector3 arr_quad[4];
        RotateQuadInto(arr_quad, vec_offset, vec_size, item_r.float_ui_rotation);
        wr.RendererPtr()->UIRgbaColorRenderer().DrawPolygon(arr_quad, 1.0f, kColorRed);
      }
      break;
    }

    default:
      break;  // TargetLine/MarkerTile 上游空实现 | empty upstream
  }
}

Rectangle RenderableScreenBounds(const RenderItem& item_r, WorldRenderer& wr) {
  switch (item_r.kind) {
    case RenderableKind::Sprite:
      return SpriteRenderableScreenBounds(item_r, wr);
    case RenderableKind::UISprite:
      return UISpriteRenderableScreenBounds(item_r);
    case RenderableKind::Custom:
      assert(item_r.ptr_custom_finalized != nullptr);
      return item_r.ptr_custom_finalized != nullptr ? item_r.ptr_custom_finalized->ScreenBounds(wr)
                                                    : Rectangle::Empty();
    default:
      return Rectangle::Empty();  // TargetLine/MarkerTile 上游 Empty | Empty upstream
  }
}

}  // namespace ora::gfx
