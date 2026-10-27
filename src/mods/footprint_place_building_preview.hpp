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

struct FootprintPlaceBuildingPreviewInfoData {
  std::string str_palette{"terrain"};
  float fp4_footprint_alpha = 1.0f;
  float fp4_line_build_footprint_alpha = 1.0f;

  static FootprintPlaceBuildingPreviewInfoData Parse(
      const meta::RecordObject& rec_info);
};

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

inline std::vector<gfx::IRenderable*> BuildingPlaceBuildingDecorationAnnotations(
    const BuildingInfoData&) {
  return {};
}

std::unique_ptr<IPlaceBuildingPreview> CreateFootprintPlaceBuildingPreview(
    gfx::WorldRenderer& wr, const game::ActorInfo& ai,
    const FootprintPlaceBuildingPreviewInfoData& info);

}
