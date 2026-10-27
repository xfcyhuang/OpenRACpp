#include "gfx/renderable.hpp"

#include <cassert>

#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/world_renderer.hpp"

namespace ora::gfx {

namespace {

core::Vector3 ToVector3(int2 v) {
  return core::Vector3{static_cast<float>(v.X), static_cast<float>(v.Y), 0.0f};
}

const core::Color kColorRed = core::Color::FromArgb(255, 255, 0, 0);

}

std::int32_t RenderableZSortKey(const RenderItem& item_r) {
  if (item_r.kind == RenderableKind::Custom)
    return item_r.ptr_custom->Pos().Y + item_r.ptr_custom->Pos().Z + item_r.ptr_custom->ZOffset();
  if (item_r.kind == RenderableKind::TargetLine && item_r.vec_waypoints.empty())
    throw std::runtime_error("Sequence contains no elements");
  return item_r.Pos().Y + item_r.Pos().Z + item_r.int4_z_offset;
}

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

  if (sprite_r.kind_channel == TextureChannel::RGBA &&
      !(ptr_palette != nullptr && ptr_palette->HasColorShift()))
    item_r.ptr_palette = nullptr;

  return item_r;
}

RenderItem SpriteRenderableWithPalette(const RenderItem& item_r, const PaletteReference* ptr_palette) {
  RenderItem item_out = item_r;
  item_out.ptr_palette = ptr_palette;

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

RenderItem MakeUISpriteRenderable(const Sprite& sprite_r, WPos wpos_effective_world_pos,
                                  core::Vector2 vec_screen_pos, std::int32_t int4_z_offset,
                                  const PaletteReference* ptr_palette, float float_scale, float float_alpha,
                                  float float_rotation) {
  RenderItem item_r;
  item_r.kind = RenderableKind::UISprite;
  item_r.ptr_sprite = &sprite_r;
  item_r.wpos_pos = wpos_effective_world_pos;
  item_r.vec_screen_pos = vec_screen_pos;
  item_r.int4_z_offset = int4_z_offset;
  item_r.ptr_palette = ptr_palette;
  item_r.float_scale = float_scale;
  item_r.float_alpha = float_alpha;
  item_r.float_ui_rotation = float_rotation;

  if (sprite_r.kind_channel == TextureChannel::RGBA &&
      !(ptr_palette != nullptr && ptr_palette->HasColorShift()))
    item_r.ptr_palette = nullptr;

  item_r.b_is_decoration = true;
  return item_r;
}

Rectangle UISpriteRenderableScreenBounds(const RenderItem& item_r) {
  const core::Vector3 vec_offset =
      core::Vector3{item_r.vec_screen_pos.X, item_r.vec_screen_pos.Y, 0.0f} + item_r.ptr_sprite->vec_offset;
  return BoundingRectangle(vec_offset, item_r.ptr_sprite->vec_size, item_r.float_ui_rotation);
}

RenderItem MakeTargetLineRenderable(std::span<const WPos> vec_waypoints, core::Color color_c,
                                    std::int32_t int4_width, std::int32_t int4_marker_size) {
  RenderItem item_r;
  item_r.kind = RenderableKind::TargetLine;
  item_r.vec_waypoints = vec_waypoints;
  item_r.color_color = color_c;
  item_r.int4_width = int4_width;
  item_r.int4_marker_size = int4_marker_size;
  item_r.int4_z_offset = 0;
  item_r.b_is_decoration = true;
  return item_r;
}

RenderItem TargetLineRenderableOffsetBy(const RenderItem& item_r, const WVec& vec_offset,
                                        std::span<WPos> vec_out) {

  assert(vec_out.size() >= item_r.vec_waypoints.size());
  RenderItem item_out = item_r;
  for (std::size_t i = 0; i < item_r.vec_waypoints.size(); ++i)
    vec_out[i] = item_r.vec_waypoints[i] + vec_offset;
  item_out.vec_waypoints = {vec_out.data(), item_r.vec_waypoints.size()};
  return item_out;
}

RenderItem MakeMarkerTileRenderable(CPos cpos_pos, core::Color color_c) {
  RenderItem item_r;
  item_r.kind = RenderableKind::MarkerTile;
  item_r.cpos_cell = cpos_pos;
  item_r.color_color = color_c;
  item_r.int4_z_offset = 0;
  item_r.b_is_decoration = true;
  return item_r;
}

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

  for (const RenderItem& item_r : vec_items) {
    const auto kind = static_cast<std::int32_t>(item_r.kind);
    if (kind < kRenderableKindCount && segments.arr_counts[kind] == 0)
      segments.arr_kind_order[segments.int4_kind_order_count++] = item_r.kind;
    if (kind < kRenderableKindCount)
      ++segments.arr_counts[kind];
  }

  std::array<std::int32_t, kRenderableKindCount> arr_cursor{};
  std::int32_t int4_offset = 0;
  for (std::int32_t i = 0; i < segments.int4_kind_order_count; ++i) {
    const auto kind = static_cast<std::int32_t>(segments.arr_kind_order[i]);
    segments.arr_starts[kind] = int4_offset;
    arr_cursor[kind] = int4_offset;
    int4_offset += segments.arr_counts[kind];
  }

  RenderItem* arr_out =
      static_cast<RenderItem*>(arena.Allocate(sizeof(RenderItem) * vec_items.size(), alignof(RenderItem)));
  for (const RenderItem& item_r : vec_items) {
    const auto kind = static_cast<std::int32_t>(item_r.kind);
    if (kind >= kRenderableKindCount)
      continue;
    arr_out[arr_cursor[kind]++] = item_r;
  }

  segments.vec_segmented = {arr_out, vec_items.size()};
  return segments;
}

void RenderRenderable(const RenderItem& item_r, WorldRenderer& wr) {
  switch (item_r.kind) {
    case RenderableKind::Sprite: {

      SpriteRenderer& renderer_sprite = wr.RendererPtr()->WorldSpriteRenderer();
      core::Vector3 vec_t = item_r.float_alpha * item_r.vec_tint;
      const ITerrainLighting* ptr_lighting = wr.TerrainLighting();
      if (ptr_lighting != nullptr && (item_r.kind_tint_mods & TintModifiers::IgnoreWorldTint) == TintModifiers::None)
        vec_t = vec_t * ptr_lighting->TintAt(item_r.wpos_pos);

      float float_a = item_r.float_alpha;
      if ((item_r.kind_tint_mods & TintModifiers::ReplaceColor) != TintModifiers::None)
        float_a *= -1.0f;

      renderer_sprite.DrawSprite(*item_r.ptr_sprite, item_r.ptr_palette, SpriteRenderableScreenPosition(item_r, wr),
                                 item_r.float_scale, vec_t, float_a, item_r.wangle_rotation.RendererRadians());
      break;
    }

    case RenderableKind::UISprite: {

      wr.RendererPtr()->UIRgbaSpriteRenderer().DrawSprite(
          *item_r.ptr_sprite, core::Vector3{item_r.vec_screen_pos.X, item_r.vec_screen_pos.Y, 0.0f},
          item_r.float_scale, core::Vector3{1.0f, 1.0f, 1.0f}, item_r.float_alpha, item_r.float_ui_rotation);
      break;
    }

    case RenderableKind::TargetLine: {

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

      const core::Vector3 vec_offset{1.0f, 1.0f, 0.0f};
      renderer_color.FillRect(vec_first - vec_offset, vec_first + vec_offset, item_r.color_color);
      break;
    }

    case RenderableKind::MarkerTile: {

      const TerrainMapSurface& surface = wr.TerrainSurface();
      const std::int32_t int4_ramp_offset = surface.fn_ramp_center_height_offset(item_r.cpos_cell);
      const WPos wpos_corner_base =
          surface.fn_center_of_cell(item_r.cpos_cell) - WVec{0, 0, int4_ramp_offset};

      core::Vector3 arr_screen[4];
      const std::span<const WVec> vec_corners = surface.fn_ramp_corners(item_r.cpos_cell);
      for (std::size_t i = 0; i < 4 && i < vec_corners.size(); ++i)
        arr_screen[i] = ToVector3(wr.Viewport()->WorldToViewPx(wr.Screen3DPosition(wpos_corner_base + vec_corners[i])));

      if (vec_corners.size() >= 4)
        wr.RendererPtr()->UIRgbaColorRenderer().FillRect(arr_screen[0], arr_screen[1], arr_screen[2], arr_screen[3],
                                                         item_r.color_color);
      break;
    }

    case RenderableKind::Custom:
      assert(item_r.ptr_custom_finalized != nullptr);
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
      break;
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
      return Rectangle::Empty();
  }
}

}
