#include "mods/radar.hpp"

#include "game/ruleset.hpp"
#include "gfx/gfx_util.hpp"
#include "map/map.hpp"
#include "mods/actor_exts.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

RadarPingsInfoData RadarPingsInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RadarPingsInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "FromRadius"))
    data.int4_from_radius = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "ToRadius"))
    data.int4_to_radius = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "ShrinkSpeed"))
    data.int4_shrink_speed = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "RotationSpeed"))
    if (const auto* f = std::get_if<float>(&gv->val))
      data.fp4_rotation_speed = *f;
  return data;
}

std::array<core::Vector2, 3> RadarPing::Points(int2 int2_center) const {
  const core::Vector2 vec_center{
      static_cast<float>(int2_center.X), static_cast<float>(int2_center.Y)};
  constexpr double kTwoPiOver3 = 2.0 * 3.14159265358979323846 / 3.0;
  const float fp4_radius = static_cast<float>(int4_radius);
  const core::Vector2 d0 = gfx::FromAngle(fp4_angle);
  const core::Vector2 d1 =
      gfx::FromAngle(static_cast<float>(fp4_angle + kTwoPiOver3));
  const core::Vector2 d2 =
      gfx::FromAngle(static_cast<float>(fp4_angle + 2 * kTwoPiOver3));
  return {
      core::Vector2{vec_center.X + fp4_radius * d0.X,
                    vec_center.Y + fp4_radius * d0.Y},
      core::Vector2{vec_center.X + fp4_radius * d1.X,
                    vec_center.Y + fp4_radius * d1.Y},
      core::Vector2{vec_center.X + fp4_radius * d2.X,
                    vec_center.Y + fp4_radius * d2.Y},
  };
}

void RadarPings::Tick(Actor&) {
  std::erase_if(vec_pings_, [](const auto& p) { return !p->Tick(); });
}

RadarPing& RadarPings::Add(std::function<bool()> fn_is_visible,
                           WPos position, core::Color color, int duration) {
  auto ping = std::make_unique<RadarPing>(
      std::move(fn_is_visible), position, color, duration,
      info_.int4_from_radius, info_.int4_to_radius, info_.int4_shrink_speed,
      info_.fp4_rotation_speed);

  if (ping->fn_is_visible && ping->fn_is_visible())
    wpos_last_ping_position_ = ping->wpos_position;

  vec_pings_.push_back(std::move(ping));
  return *vec_pings_.back();
}

PlayerRadarTerrainInfoData PlayerRadarTerrainInfoData::Parse(
    const meta::RecordObject&) {
  return {};
}

PlayerRadarTerrain::PlayerRadarTerrain(Actor& self)
    : ptr_world_(&self.world()),
      ptr_shroud_(self.TraitOrDefault<sim::Shroud>()) {
  ptr_shroud_->AddShroudChangedCallback(
      [this](PPos puv) { UpdateShroudCell(puv); });
}

void PlayerRadarTerrain::UpdateShroudCell(PPos puv) {
  for (const MPos& uv : ptr_world_->Map().Unproject(puv))
    UpdateTerrainCell(uv);
}

void PlayerRadarTerrain::UpdateTerrainCell(MPos uv) {
  if (ptr_shroud_->IsVisible(uv))
    UpdateTerrainCellColor(uv);
}

void PlayerRadarTerrain::UpdateTerrainCellColor(MPos uv) {
  layer_terrain_color_->Set(uv,
                            GetColor(ptr_world_->Map(),
                                     vec_radar_terrain_layers_, uv));

  for (const auto& fn : vec_cell_terrain_color_changed_)
    if (fn)
      fn(uv);
}

void PlayerRadarTerrain::WorldLoaded(sim::World& w, gfx::WorldRenderer*) {
  for (auto* rtl : w.WorldActor()->TraitsImplementing<sim::IRadarTerrainLayer>())
    vec_radar_terrain_layers_.push_back(rtl);
  layer_terrain_color_ =
      std::make_unique<map::CellLayer<std::pair<std::uint32_t, std::uint32_t>>>(
          w.Map());

  w.AddFrameEndTask([this](sim::World&) {
    for (const MPos& uv : ptr_world_->Map().AllCells().MapCoords())
      UpdateTerrainCellColor(uv);

    n_map_tiles_token_ =
        ptr_world_->Map().TilesRef().SubscribeCellEntryChanged(
            [this](CPos cell) {
              UpdateTerrainCell(cell.ToMPos(ptr_world_->Map().Grid().Type));
            });
    for (auto* rtl : vec_radar_terrain_layers_)
      vec_rtl_tokens_.push_back(rtl->SubscribeCellEntryChanged(
          [this](CPos cell) {
            UpdateTerrainCell(cell.ToMPos(ptr_world_->Map().Grid().Type));
          }));

    b_is_initialized_ = true;
  });
}

std::pair<std::uint32_t, std::uint32_t> PlayerRadarTerrain::GetColor(
    const map::Map& map,
    std::span<sim::IRadarTerrainLayer* const> vec_radar_terrain_layers,
    MPos uv) {
  std::optional<std::pair<core::Color, core::Color>> pair_colors;
  for (auto* rtl : vec_radar_terrain_layers)
    if (rtl->TryGetTerrainColorPair(uv, pair_colors))
      return {pair_colors->first.ToArgb(), pair_colors->second.ToArgb()};

  const auto pair_terrain = map.GetTerrainColorPair(uv);
  return {pair_terrain.first.ToArgb(), pair_terrain.second.ToArgb()};
}

AppearsOnRadarInfoData AppearsOnRadarInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AppearsOnRadarInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "UseLocation"))
    if (const auto* b = std::get_if<bool>(&gv->val))
      data.b_use_location = *b;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "ValidRelationships"))
    if (const auto* n = std::get_if<std::int64_t>(&gv->val))
      data.kind_valid_relationships =
          static_cast<sim::PlayerRelationship>(*n);
  return data;
}

AppearsOnRadar::AppearsOnRadar(const ActorInitializer& init,
                               AppearsOnRadarInfoData info)
    : sim::ConditionalTraitCore<AppearsOnRadar>(info.conditional),
      info_{std::move(info)} {}

void AppearsOnRadar::PopulateRadarSignatureCells(
    Actor& self, std::vector<std::pair<CPos, core::Color>>& vec_dest) {
  const sim::Player* ptr_viewer =
      self.world().RenderPlayer() != nullptr
          ? self.world().RenderPlayer()
          : self.world().LocalPlayer();
  if (IsTraitDisabled() ||
      (ptr_viewer != nullptr &&
       !sim::HasRelationship(
           info_.kind_valid_relationships,
           self.Owner()->RelationshipWith(ptr_viewer))))
    return;

  core::Color color = mods::OwnerColor(self);
  if (ptr_modifier_ != nullptr)
    color = ptr_modifier_->RadarColorOverride(self, color);

  if (info_.b_use_location) {
    vec_dest.emplace_back(self.Location(), color);
    return;
  }

  for (const auto& [cell, sub_cell] : self.OccupiesSpace()->OccupiedCells())
    vec_dest.emplace_back(cell, color);
}

ProvidesRadarInfoData ProvidesRadarInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProvidesRadarInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

RadarColorFromTerrainInfoData RadarColorFromTerrainInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RadarColorFromTerrainInfoData data;
  if (const auto v = sim::RecordFieldString(rec_info, "Terrain"))
    data.str_terrain = std::string{*v};
  return data;
}

core::Color RadarColorFromTerrainInfoData::GetColorFromTerrain(
    sim::World& world) const {
  const auto& terrain_info = world.Map().Rules().TerrainInfo();
  return terrain_info
      .TerrainTypes()[terrain_info.GetTerrainIndex(str_terrain)]
      .Color;
}

}
