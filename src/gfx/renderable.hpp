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

struct IFinalizedRenderable {
  virtual void Render(WorldRenderer& wr) = 0;
  virtual void RenderDebugGeometry(WorldRenderer& wr) = 0;
  virtual Rectangle ScreenBounds(WorldRenderer& wr) = 0;
  virtual ~IFinalizedRenderable() = default;
};

enum class RenderableKind : std::uint8_t {
  Sprite,
  UISprite,
  TargetLine,
  MarkerTile,
  Custom,
  Count,
};

inline constexpr std::int32_t kRenderableKindCount =
    static_cast<std::int32_t>(RenderableKind::Count);

struct RenderItem {
  RenderableKind kind = RenderableKind::Sprite;
  TintModifiers kind_tint_mods = TintModifiers::None;
  bool b_is_decoration = false;

  const Sprite* ptr_sprite = nullptr;
  const PaletteReference* ptr_palette = nullptr;
  WPos wpos_pos{};
  WVec wvec_offset{};
  std::int32_t int4_z_offset = 0;
  float float_scale = 1.0f;
  float float_alpha = 1.0f;
  core::Vector3 vec_tint{1.0f, 1.0f, 1.0f};
  WAngle wangle_rotation{};
  core::Vector2 vec_screen_pos{};
  float float_ui_rotation = 0.0f;

  std::span<const WPos> vec_waypoints{};
  core::Color color_color{};
  std::int32_t int4_width = 0;
  std::int32_t int4_marker_size = 0;

  CPos cpos_cell{};

  IRenderable* ptr_custom = nullptr;
  IFinalizedRenderable* ptr_custom_finalized = nullptr;

  WPos Pos() const {
    return kind == RenderableKind::Sprite ? wpos_pos + wvec_offset : wpos_pos;
  }
};

static_assert(std::is_trivially_destructible_v<RenderItem>,
              "RenderItem 须平凡可析构(FrameArena 域)| must be trivially "
              "destructible (FrameArena domain)");

std::int32_t RenderableZSortKey(const RenderItem& item_r);

RenderItem MakeSpriteRenderable(const Sprite& sprite_r, WPos wpos_pos, WVec wvec_offset,
                                std::int32_t int4_z_offset, const PaletteReference* ptr_palette,
                                float float_scale, float float_alpha, core::Vector3 vec_tint,
                                TintModifiers kind_tint_mods, bool b_is_decoration,
                                WAngle wangle_rotation = WAngle{});

RenderItem SpriteRenderableWithPalette(const RenderItem& item_r, const PaletteReference* ptr_palette);
RenderItem SpriteRenderableWithZOffset(const RenderItem& item_r, std::int32_t int4_new_offset);
RenderItem SpriteRenderableOffsetBy(const RenderItem& item_r, const WVec& vec_offset);
RenderItem SpriteRenderableAsDecoration(const RenderItem& item_r);
RenderItem SpriteRenderableWithAlpha(const RenderItem& item_r, float float_new_alpha);
RenderItem SpriteRenderableWithTint(const RenderItem& item_r, core::Vector3 vec_new_tint,
                                    TintModifiers kind_new_tint_mods);

core::Vector3 SpriteRenderableScreenPosition(const RenderItem& item_r, const WorldRenderer& wr);

Rectangle SpriteRenderableScreenBounds(const RenderItem& item_r, const WorldRenderer& wr);

RenderItem MakeUISpriteRenderable(const Sprite& sprite_r, WPos wpos_effective_world_pos,
                                  core::Vector2 vec_screen_pos, std::int32_t int4_z_offset,
                                  const PaletteReference* ptr_palette, float float_scale = 1.0f,
                                  float float_alpha = 1.0f, float float_rotation = 0.0f);
Rectangle UISpriteRenderableScreenBounds(const RenderItem& item_r);

RenderItem MakeTargetLineRenderable(std::span<const WPos> vec_waypoints, core::Color color_c,
                                    std::int32_t int4_width, std::int32_t int4_marker_size);

RenderItem TargetLineRenderableOffsetBy(const RenderItem& item_r, const WVec& vec_offset,
                                        std::span<WPos> vec_out);

RenderItem MakeMarkerTileRenderable(CPos cpos_pos, core::Color color_c);

std::span<const RenderItem> SortRenderablesByZ(std::span<const RenderItem> vec_items, FrameArena& arena);

struct KindSegments {
  std::span<const RenderItem> vec_segmented;
  std::array<std::int32_t, kRenderableKindCount> arr_counts{};
  std::array<std::int32_t, kRenderableKindCount> arr_starts{};
  std::int32_t int4_kind_order_count = 0;
  RenderableKind arr_kind_order[kRenderableKindCount];
};

KindSegments SegmentByKind(std::span<const RenderItem> vec_items, FrameArena& arena);

void RenderRenderable(const RenderItem& item_r, WorldRenderer& wr);

void RenderRenderableDebugGeometry(const RenderItem& item_r, WorldRenderer& wr);

Rectangle RenderableScreenBounds(const RenderItem& item_r, WorldRenderer& wr);

}
