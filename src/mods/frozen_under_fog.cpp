#include "mods/frozen_under_fog.hpp"

#include "game/actor_info.hpp"
#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/building.hpp"
#include "sim/actor_init.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/screen_map.hpp"
#include "sim/shroud.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> RecInt(const meta::RecordObject& rec,
                                   std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          return *n;
      }
  }
  return std::nullopt;
}

bool ShroudAnyExplored(const sim::Shroud& shroud,
                       const std::vector<std::pair<CPos, SubCell>>& cells) {
  for (const auto& [cell, sub] : cells)
    if (shroud.IsExplored(cell))
      return true;
  return false;
}

bool ShroudAnyExplored(const sim::Shroud& shroud,
                       const std::vector<PPos>& vec_puvs) {
  for (const PPos puv : vec_puvs)
    if (shroud.IsExplored(puv))
      return true;
  return false;
}

int PlayerIndexOf(const sim::World& world, const sim::Player* player) {
  const std::vector<sim::Player*>& vec_players = world.Players();
  for (std::size_t i = 0; i < vec_players.size(); i++)
    if (vec_players[i] == player)
      return static_cast<int>(i);
  return -1;
}

const meta::RecordObject* FindBuildingInfo(const game::ActorInfo& ai) {
  for (const auto& rec_trait : ai.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.Buildings.BuildingInfo"})
      return rec_trait;
  return nullptr;
}

}

FrozenUnderFogInfoData FrozenUnderFogInfoData::Parse(
    const meta::RecordObject& rec_info) {
  FrozenUnderFogInfoData data;
  if (const auto v = RecInt(rec_info, "AlwaysVisibleRelationships"))
    data.always_visible_relationships =
        static_cast<sim::PlayerRelationship>(
            static_cast<std::int32_t>(*v));
  return data;
}

FrozenUnderFog::FrozenUnderFog(sim::ActorInitializer& init,
                               FrozenUnderFogInfoData info)
    : info_{std::move(info)} {

  sim::Actor& self = init.Self();
  sim::World& world = self.world();
  map::Map& map = world.Map();

  bool explored_map = false;
  if (!world.Players().empty()) {
    if (const sim::Shroud* shroud = world.Players()[0]->GetShroud())
      explored_map = shroud->ExploreMapEnabled();
  }
  b_starts_revealed_ = explored_map &&
                       init.Contains<sim::SpawnedByMapInit>() &&
                       !init.Contains<sim::HiddenUnderFogInit>();

  std::vector<CPos> vec_footprint_cells{self.Location()};
  if (const meta::RecordObject* rec_building = FindBuildingInfo(
          *self.Info())) {
    const BuildingInfoData building_info =
        BuildingInfoData::Parse(*rec_building);
    vec_footprint_cells = building_info.FrozenUnderFogTiles(
        self.Location());
  }

  for (const CPos c : vec_footprint_cells) {
    const MPos uv = c.ToMPos(map.Grid().Type);
    for (const PPos puv : map.ProjectedCellsCovering(uv))
      vec_footprint_.push_back(puv);
  }
}

void FrozenUnderFog::Created(sim::Actor& self) {

  sim::World& world = self.world();
  vec_frozen_states_.clear();
  for (sim::Player* player : world.Players()) {
    sim::FrozenActor* frozen_actor = world.Arena().Create<sim::FrozenActor>(
        self, *this, vec_footprint_, *player, b_starts_revealed_);
    player->GetFrozenActorLayer()->Add(frozen_actor);
    vec_frozen_states_.push_back(
        FrozenState{frozen_actor, !frozen_actor->Visible()});
  }

  world.AddFrameEndTask([this](sim::World& w) {
    (void)w;
    for (std::size_t player_index = 0;
         player_index < vec_frozen_states_.size(); player_index++) {
      FrozenState& state = vec_frozen_states_[player_index];
      sim::FrozenActor* frozen = state.frozen_actor;
      if (b_starts_revealed_ || state.b_is_visible)
        UpdateFrozenActor(*frozen, static_cast<int>(player_index));

      frozen->RefreshHidden();
    }
  });

  b_created_ = true;
}

void FrozenUnderFog::UpdateFrozenActor(sim::FrozenActor& frozen_actor,
                                       int player_index) {

  VisibilityHash |= 1 << (player_index % 32);
  frozen_actor.RefreshState();
}

void FrozenUnderFog::OnVisibilityChanged(sim::FrozenActor& frozen) {

  if (!b_created_)
    return;

  FrozenState& state = vec_frozen_states_[PlayerIndexOf(
      frozen.Viewer()->GetWorld(), frozen.Viewer())];
  const bool is_visible = !frozen.Visible();
  state.b_is_visible = is_visible;

  if (is_visible)
    UpdateFrozenActor(
        frozen, PlayerIndexOf(frozen.Viewer()->GetWorld(), frozen.Viewer()));

  frozen.RefreshHidden();
}

bool FrozenUnderFog::IsVisibleInner(sim::Player& by_player) {

  if (!by_player.GetShroud()->FogEnabled())
    return ShroudAnyExplored(*by_player.GetShroud(), vec_footprint_);

  return vec_frozen_states_[PlayerIndexOf(by_player.GetWorld(),
                                          &by_player)]
      .b_is_visible;
}

bool FrozenUnderFog::IsVisible(sim::Actor& self, sim::Player* by_player) {

  if (by_player == nullptr)
    return true;

  const sim::PlayerRelationship relationship =
      self.Owner()->RelationshipWith(by_player);
  return sim::HasRelationship(info_.always_visible_relationships,
                              relationship) ||
         IsVisibleInner(*by_player);
}

void FrozenUnderFog::TickRender(sim::Actor& self) {

  for (std::size_t player_index = 0;
       player_index < vec_frozen_states_.size(); player_index++) {
    sim::FrozenActor* frozen =
        vec_frozen_states_[player_index].frozen_actor;
    if (!frozen->NeedRenderables())
      continue;

    frozen->SetNeedRenderables(false);
    if (self.world().ScreenMapFace() != nullptr)
      self.world().ScreenMapFace()->AddOrUpdateFrozen(
          self.world().Players()[player_index], frozen);
  }
}

void FrozenUnderFog::OnOwnerChanged(sim::Actor& self,
                                    sim::Player& ,
                                    sim::Player& ) {

  const int old_owner_index = PlayerIndexOf(self.world(), self.Owner());
  if (old_owner_index < 0 ||
      old_owner_index >= static_cast<int>(vec_frozen_states_.size()))
    return;
  sim::FrozenActor* frozen =
      vec_frozen_states_[old_owner_index].frozen_actor;
  UpdateFrozenActor(*frozen, old_owner_index);
  frozen->RefreshHidden();
}

void FrozenUnderFog::Disposing(sim::Actor& self) {

  const int owner_index = PlayerIndexOf(self.world(), self.Owner());
  if (owner_index < 0 ||
      owner_index >= static_cast<int>(vec_frozen_states_.size()))
    return;
  vec_frozen_states_[owner_index].frozen_actor->Invalidate();
}

HiddenUnderShroudInfoData HiddenUnderShroudInfoData::Parse(
    const meta::RecordObject& rec_info) {
  HiddenUnderShroudInfoData data;
  if (const auto v = RecInt(rec_info, "AlwaysVisibleRelationships"))
    data.always_visible_relationships =
        static_cast<sim::PlayerRelationship>(
            static_cast<std::int32_t>(*v));
  if (const auto v = RecInt(rec_info, "Type"))
    data.type = static_cast<VisibilityType>(static_cast<std::int32_t>(*v));
  return data;
}

bool HiddenUnderShroud::IsVisibleInner(sim::Actor& self,
                                       sim::Player& by_player) {

  if (info_.type == VisibilityType::Footprint)
    return ShroudAnyExplored(*by_player.GetShroud(),
                             self.OccupiesSpace()->OccupiedCells());

  WPos pos = self.CenterPosition();
  if (info_.type == VisibilityType::GroundPosition)
    pos = pos - WVec{0, 0, self.world().Map().DistanceAboveTerrain(pos)
                                    .Length};

  return by_player.GetShroud()->IsExplored(pos);
}

bool HiddenUnderShroud::IsVisible(sim::Actor& self,
                                  sim::Player* by_player) {

  if (by_player == nullptr)
    return true;

  const sim::PlayerRelationship relationship =
      self.Owner()->RelationshipWith(by_player);
  return sim::HasRelationship(info_.always_visible_relationships,
                              relationship) ||
         IsVisibleInner(self, *by_player);
}

}
