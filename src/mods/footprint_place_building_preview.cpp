// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/FootprintPlaceBuildingPreview.cs
//          @e180e3225(实现部分)
//          The implementation half.
#include "mods/footprint_place_building_preview.hpp"

#include "game/actor_info.hpp"
#include "mods/building_utils.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

FootprintPlaceBuildingPreviewInfoData FootprintPlaceBuildingPreviewInfoData::Parse(
    const meta::RecordObject& rec_info) {
  FootprintPlaceBuildingPreviewInfoData data;
  if (const auto v = sim::RecordFieldString(rec_info, "Palette"))
    if (!v->empty())
      data.str_palette = std::string{*v};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "FootprintAlpha"))
    if (const auto* f = std::get_if<float>(&gv->val))
      data.fp4_footprint_alpha = *f;
  if (const auto* gv =
          sim::RecordFieldValue(rec_info, "LineBuildFootprintAlpha"))
    if (const auto* f = std::get_if<float>(&gv->val))
      data.fp4_line_build_footprint_alpha = *f;
  return data;
}

void FootprintCellRenderable::Configure(
    const gfx::Sprite& sprite_tile, CPos cell, WPos pos, WVec offset,
    const gfx::PaletteReference* ptr_palette, float fp4_alpha) {
  sprite_tile_ = sprite_tile;
  cpos_cell_ = cell;
  wpos_pos_ = pos;
  wvec_offset_ = offset;
  ptr_palette_ = ptr_palette;
  fp4_alpha_ = fp4_alpha;
}

void FootprintCellRenderable::Render(gfx::WorldRenderer& wr) {
  gfx::Renderer* ptr_renderer = wr.RendererPtr();
  if (ptr_renderer == nullptr)
    return;

  const core::Vector3 vec_location{
      static_cast<float>(wpos_pos_.X + wvec_offset_.X),
      static_cast<float>(wpos_pos_.Y + wvec_offset_.Y),
      static_cast<float>(wpos_pos_.Z + wvec_offset_.Z)};
  ptr_renderer->WorldSpriteRenderer().DrawSprite(
      sprite_tile_, ptr_palette_, vec_location, 1.0f,
      core::Vector3{1.0f, 1.0f, 1.0f}, fp4_alpha_);
}

FootprintPlaceBuildingPreviewPreview::FootprintPlaceBuildingPreviewPreview(
    gfx::WorldRenderer& wr, const game::ActorInfo& ai,
    const FootprintPlaceBuildingPreviewInfoData& info)
    : ptr_actor_info_{&ai}, info_{info} {
  sim::World& world = wr.World();

  if (const meta::RecordObject* rec_building =
          FindTraitInfoRecord(ai, "BuildingInfo")) {
    const BuildingInfoData bi = BuildingInfoData::Parse(*rec_building);
    wvec_center_offset_ = bi.CenterOffset(world);
  }
  int2_top_left_screen_offset_ = -wr.ScreenPxOffset(wvec_center_offset_);

  const std::string str_tileset = world.Map().Tileset();
  std::string str_lower;
  str_lower.reserve(str_tileset.size());
  for (const char ch : str_tileset)
    str_lower.push_back(static_cast<char>(
        ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch));

  gfx::SequenceSet* sequences = world.Map().Sequences();
  if (sequences != nullptr) {
    gfx::ISpriteSequence* valid_sequence = nullptr;
    if (sequences->HasSequence("overlay", "build-valid-" + str_lower))
      valid_sequence =
          &sequences->GetSequence("overlay", "build-valid-" + str_lower);
    else
      valid_sequence = &sequences->GetSequence("overlay", "build-valid");

    sprite_valid_tile_ = valid_sequence->GetSprite(0);
    fp4_valid_alpha_ = valid_sequence->GetAlpha(0);

    gfx::ISpriteSequence& blocked_sequence =
        sequences->GetSequence("overlay", "build-invalid");
    sprite_blocked_tile_ = blocked_sequence.GetSprite(0);
    fp4_blocked_alpha_ = blocked_sequence.GetAlpha(0);
  }
}

std::vector<gfx::IRenderable*> FootprintPlaceBuildingPreviewPreview::Render(
    gfx::WorldRenderer& wr, CPos top_left,
    const std::vector<std::pair<CPos, PlaceBuildingCellType>>& footprint) {
  const gfx::PaletteReference* ptr_palette = wr.Palette(info_.str_palette);
  const WPos pos_top_left = wr.World().Map().CenterOfCell(top_left);

  vec_cell_renderables_.clear();
  vec_renderable_ptrs_.clear();
  for (const auto& [cell, kind_type] : footprint) {
    if (kind_type == PlaceBuildingCellType::None)
      continue;

    const bool b_invalid = IncludesCellType(kind_type,
                                            PlaceBuildingCellType::Invalid);
    const gfx::Sprite& tile =
        b_invalid ? sprite_blocked_tile_ : sprite_valid_tile_;
    const float fp4_sequence_alpha =
        b_invalid ? fp4_blocked_alpha_ : fp4_valid_alpha_;
    const WPos pos = wr.World().Map().CenterOfCell(cell);
    const WVec offset{0, 0, pos_top_left.Z - pos.Z};
    const float fp4_trait_alpha =
        IncludesCellType(kind_type, PlaceBuildingCellType::LineBuild)
            ? info_.fp4_line_build_footprint_alpha
            : info_.fp4_footprint_alpha;

    vec_cell_renderables_.emplace_back();
    vec_cell_renderables_.back().Configure(tile, cell, pos, offset,
                                           ptr_palette,
                                           fp4_sequence_alpha *
                                               fp4_trait_alpha);
    vec_renderable_ptrs_.push_back(&vec_cell_renderables_.back());
  }

  return vec_renderable_ptrs_;
}

std::vector<gfx::IRenderable*>
FootprintPlaceBuildingPreviewPreview::RenderAnnotations(gfx::WorldRenderer&,
                                                        CPos) {
  return {};
}

std::unique_ptr<IPlaceBuildingPreview> CreateFootprintPlaceBuildingPreview(
    gfx::WorldRenderer& wr, const game::ActorInfo& ai,
    const FootprintPlaceBuildingPreviewInfoData& info) {
  return std::make_unique<FootprintPlaceBuildingPreviewPreview>(wr, ai, info);
}

}  // namespace ora::mods
