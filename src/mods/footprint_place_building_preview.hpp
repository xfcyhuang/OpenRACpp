// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/FootprintPlaceBuildingPreview.cs
//          @1f594e320 L21-135 全文(引擎 SpriteRenderable → 自有 custom
//          renderable 承载:IOrderGenerator 的 IRenderable* 出参域)
//          The whole of FootprintPlaceBuildingPreview.cs L21-135 (the
//          engine SpriteRenderable → an own custom renderable: the
//          IRenderable* out domain of IOrderGenerator).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/wpos.hpp"
#include "gfx/renderable.hpp"
#include "gfx/sprite.hpp"
#include "mods/building.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/type_dictionary.hpp"

namespace ora::gfx {
class WorldRenderer;
class PaletteReference;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

enum class PlaceBuildingCellType : std::uint8_t {
  None = 0,
  Valid = 1,
  Invalid = 2,
  LineBuild = 4,
};

constexpr PlaceBuildingCellType operator|(PlaceBuildingCellType a,
                                          PlaceBuildingCellType b) {
  return static_cast<PlaceBuildingCellType>(static_cast<std::uint8_t>(a) |
                                            static_cast<std::uint8_t>(b));
}

constexpr PlaceBuildingCellType operator&(PlaceBuildingCellType a,
                                          PlaceBuildingCellType b) {
  return static_cast<PlaceBuildingCellType>(static_cast<std::uint8_t>(a) &
                                            static_cast<std::uint8_t>(b));
}

constexpr bool IncludesCellType(PlaceBuildingCellType v, PlaceBuildingCellType flag) {
  return (static_cast<std::uint8_t>(v) & static_cast<std::uint8_t>(flag)) != 0;
}

/// FootprintPlaceBuildingPreview(L46;空运行时类)
/// FootprintPlaceBuildingPreview (L46; the empty runtime class).
class FootprintPlaceBuildingPreviewRuntime final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_FootprintPlaceBuildingPreview;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<FootprintPlaceBuildingPreviewRuntime>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

/// IPlaceBuildingPreviewGeneratorInfo(Orders/PlaceBuildingOrderGenerator.cs
/// L27-31;记录接口面 —— 实现者 FootprintPlaceBuildingPreviewInfo)
/// IPlaceBuildingPreviewGeneratorInfo (PlaceBuildingOrderGenerator.cs
/// L27-31; the record interface — implementer FootprintPlaceBuilding-
/// PreviewInfo).
struct FootprintPlaceBuildingPreviewInfoData {
  std::string str_palette{"terrain"};  // L27(TileSet.TerrainPaletteInternalName)
  float fp4_footprint_alpha = 1.0f;    // L30
  float fp4_line_build_footprint_alpha = 1.0f;  // L33

  static FootprintPlaceBuildingPreviewInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// IPlaceBuildingPreview(L33-39)
/// IPlaceBuildingPreview (L33-39).
class IPlaceBuildingPreview {
 public:
  virtual ~IPlaceBuildingPreview() = default;
  virtual int2 TopLeftScreenOffset() const = 0;
  virtual void Tick() = 0;
  virtual std::vector<gfx::IRenderable*> Render(
      gfx::WorldRenderer& wr, CPos top_left,
      const std::vector<std::pair<CPos, PlaceBuildingCellType>>& footprint) = 0;
  virtual std::vector<gfx::IRenderable*> RenderAnnotations(
      gfx::WorldRenderer& wr, CPos top_left) = 0;
};

/// 单格 footprint 瓦(FootprintPlaceBuildingPreviewPreview 的
/// RenderFootprint yield 项;上游 SpriteRenderable(z=-511, IgnoreWorldTint)
/// 的 custom 承载)
/// One footprint tile (a RenderFootprint yield item of
/// FootprintPlaceBuildingPreviewPreview; the custom carrier of upstream's
/// SpriteRenderable(z=-511, IgnoreWorldTint)).
class FootprintCellRenderable final : public gfx::IRenderable,
                                      public gfx::IFinalizedRenderable {
 public:
  FootprintCellRenderable() = default;

  void Configure(const gfx::Sprite& sprite_tile, CPos cell, WPos pos,
                 WVec offset, const gfx::PaletteReference* ptr_palette,
                 float fp4_alpha);

  WPos Pos() const override { return wpos_pos_ + wvec_offset_; }
  std::int32_t ZOffset() const override { return -511; }
  bool IsDecoration() const override { return true; }
  gfx::IRenderable* WithZOffset(std::int32_t) override { return this; }
  gfx::IRenderable* OffsetBy(const WVec& vec_offset) override {
    wpos_pos_ = wpos_pos_ + vec_offset;
    return this;
  }
  gfx::IRenderable* AsDecoration() override { return this; }
  gfx::IFinalizedRenderable* PrepareRender(gfx::WorldRenderer&) override {
    return this;
  }

  void Render(gfx::WorldRenderer& wr) override;
  void RenderDebugGeometry(gfx::WorldRenderer&) override {}
  Rectangle ScreenBounds(gfx::WorldRenderer&) override { return Rectangle{}; }

  const gfx::Sprite& Tile() const { return sprite_tile_; }
  float Alpha() const { return fp4_alpha_; }
  CPos Cell() const { return cpos_cell_; }

 private:
  gfx::Sprite sprite_tile_;
  CPos cpos_cell_{};
  WPos wpos_pos_{};
  WVec wvec_offset_{};
  const gfx::PaletteReference* ptr_palette_ = nullptr;
  float fp4_alpha_ = 1.0f;
};

/// FootprintPlaceBuildingPreviewPreview(L48-135)
/// FootprintPlaceBuildingPreviewPreview (L48-135).
class FootprintPlaceBuildingPreviewPreview final : public IPlaceBuildingPreview {
 public:
  FootprintPlaceBuildingPreviewPreview(
      gfx::WorldRenderer& wr, const game::ActorInfo& ai,
      const FootprintPlaceBuildingPreviewInfoData& info);

  int2 TopLeftScreenOffset() const override {
    return int2_top_left_screen_offset_;
  }
  void Tick() override {}

  std::vector<gfx::IRenderable*> Render(
      gfx::WorldRenderer& wr, CPos top_left,
      const std::vector<std::pair<CPos, PlaceBuildingCellType>>& footprint)
      override;
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer& wr,
                                                   CPos top_left) override;

 private:
  const game::ActorInfo* ptr_actor_info_ = nullptr;
  WVec wvec_center_offset_{};
  FootprintPlaceBuildingPreviewInfoData info_;
  int2 int2_top_left_screen_offset_{};
  gfx::Sprite sprite_valid_tile_;
  gfx::Sprite sprite_blocked_tile_;
  float fp4_valid_alpha_ = 1.0f;
  float fp4_blocked_alpha_ = 1.0f;
  std::vector<FootprintCellRenderable> vec_cell_renderables_;
  std::vector<gfx::IRenderable*> vec_renderable_ptrs_;
};

/// 上游 BuildingInfo 的 IPlaceBuildingDecorationInfo.RenderAnnotations
/// (Building.cs L257-263:RequiresBaseProvider 时 BaseProvider 的
/// RangeCircleRenderables;BaseProvider trait 未移植 = 空集的等价域)
/// Upstream BuildingInfo's IPlaceBuildingDecorationInfo.RenderAnnotations
/// (Building.cs L257-263: BaseProvider's RangeCircleRenderables when
/// RequiresBaseProvider; the BaseProvider trait unported = the empty-set
/// equivalent domain).
inline std::vector<gfx::IRenderable*> BuildingPlaceBuildingDecorationAnnotations(
    const BuildingInfoData&) {
  return {};
}

/// FootprintPlaceBuildingPreviewInfo 的工厂面(CreatePreview L35-43)
/// FootprintPlaceBuildingPreviewInfo's factory face (CreatePreview L35-43).
std::unique_ptr<IPlaceBuildingPreview> CreateFootprintPlaceBuildingPreview(
    gfx::WorldRenderer& wr, const game::ActorInfo& ai,
    const FootprintPlaceBuildingPreviewInfoData& info);

}  // namespace ora::mods
