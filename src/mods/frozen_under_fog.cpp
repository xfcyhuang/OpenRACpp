// UPSTREAM: OpenRA.Mods.Common/Traits/Modifiers/FrozenUnderFog.cs +
//          HiddenUnderShroud.cs + ShroudExts.cs L19-32(实现部分)
//          The implementation half.
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

/// ShroudExts.cs L19-26:AnyExplored((CPos, SubCell)[])
/// ShroudExts.cs L19-26: AnyExplored((CPos, SubCell)[]).
bool ShroudAnyExplored(const sim::Shroud& shroud,
                       const std::vector<std::pair<CPos, SubCell>>& cells) {
  for (const auto& [cell, sub] : cells)
    if (shroud.IsExplored(cell))
      return true;
  return false;
}

/// ShroudExts.cs L28-35:AnyExplored(PPos[])
/// ShroudExts.cs L28-35: AnyExplored(PPos[]).
bool ShroudAnyExplored(const sim::Shroud& shroud,
                       const std::vector<PPos>& vec_puvs) {
  for (const PPos puv : vec_puvs)
    if (shroud.IsExplored(puv))
      return true;
  return false;
}

/// World.Players() 的 IndexOf(Player)(PlayerDictionary 的索引面)
/// World.Players()' IndexOf(Player) (PlayerDictionary's index face).
int PlayerIndexOf(const sim::World& world, const sim::Player* player) {
  const std::vector<sim::Player*>& vec_players = world.Players();
  for (std::size_t i = 0; i < vec_players.size(); i++)
    if (vec_players[i] == player)
      return static_cast<int>(i);
  return -1;
}

/// ActorInfo 的 BuildingInfo 查询(上游 TraitInfoOrDefault<
/// BuildingInfo>;按记录全名匹配 —— HealthInfo 工厂同形)
/// The ActorInfo BuildingInfo query (upstream's TraitInfoOrDefault<
/// BuildingInfo>; matched by record full name — HealthInfo's factory
/// shape).
const meta::RecordObject* FindBuildingInfo(const game::ActorInfo& ai) {
  for (const auto& rec_trait : ai.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.Buildings.BuildingInfo"})
      return rec_trait;
  return nullptr;
}

}  // namespace

// ———— FrozenUnderFogInfo(L21-31)————
// ———— FrozenUnderFogInfo (L21-31) ————

FrozenUnderFogInfoData FrozenUnderFogInfoData::Parse(
    const meta::RecordObject& rec_info) {
  FrozenUnderFogInfoData data;
  if (const auto v = RecInt(rec_info, "AlwaysVisibleRelationships"))
    data.always_visible_relationships =
        static_cast<sim::PlayerRelationship>(
            static_cast<std::int32_t>(*v));
  return data;
}

// ———— FrozenUnderFog(L35-188)————
// ———— FrozenUnderFog (L35-188) ————

FrozenUnderFog::FrozenUnderFog(sim::ActorInitializer& init,
                               FrozenUnderFogInfoData info)
    : info_{std::move(info)} {
  // L44-55
  sim::Actor& self = init.Self();
  sim::World& world = self.world();
  map::Map& map = world.Map();

  // Explore map-placed actors if the "Explore Map" option is enabled
  // (上游注释)。exploredMap 读 = Shroud::Created 同式(玩家 Shroud 的
  // lobby 合成结果)
  // Explore map-placed actors if the "Explore Map" option is enabled
  // (upstream's comment). The exploredMap read takes Shroud::Created's
  // same shape (the player Shroud's lobby-synthesized result).
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
  // L57-80
  sim::World& world = self.world();
  vec_frozen_states_.clear();
  for (sim::Player* player : world.Players()) {
    sim::FrozenActor* frozen_actor = world.Arena().Create<sim::FrozenActor>(
        self, *this, vec_footprint_, *player, b_starts_revealed_);
    player->GetFrozenActorLayer()->Add(frozen_actor);
    vec_frozen_states_.push_back(
        FrozenState{frozen_actor, !frozen_actor->Visible()});
  }

  // Set the initial visibility state. This relies on
  // actor.GetTargetablePositions(), which is also setup up in Created.
  // Since we can't be sure whether our method will run after theirs,
  // defer by a frame.(上游注释)
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
  // L101-102
  VisibilityHash |= 1 << (player_index % 32);
  frozen_actor.RefreshState();
}

void FrozenUnderFog::OnVisibilityChanged(sim::FrozenActor& frozen) {
  // L82-99
  // Ignore callbacks during initial setup(上游注释)
  if (!b_created_)
    return;

  // Update state visibility to match the frozen actor to ensure
  // consistency(上游注释)
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
  // L106-109
  // If fog is disabled visibility is determined by shroud(上游注释)
  if (!by_player.GetShroud()->FogEnabled())
    return ShroudAnyExplored(*by_player.GetShroud(), vec_footprint_);

  return vec_frozen_states_[PlayerIndexOf(by_player.GetWorld(),
                                          &by_player)]
      .b_is_visible;
}

bool FrozenUnderFog::IsVisible(sim::Actor& self, sim::Player* by_player) {
  // L114-121
  if (by_player == nullptr)
    return true;

  const sim::PlayerRelationship relationship =
      self.Owner()->RelationshipWith(by_player);
  return sim::HasRelationship(info_.always_visible_relationships,
                              relationship) ||
         IsVisibleInner(*by_player);
}

void FrozenUnderFog::TickRender(sim::Actor& self) {
  // L104-112(结构面;renderables/bounds/mouseBounds 的渲染物化随渲染批
  // —— 此处保留 NeedRenderables 的消费与 ScreenMap 冻结面,物化空)
  // L104-112 (the structural face; the renderables/bounds/mouseBounds
  // materialization rides the render batch — the NeedRenderables
  // consumption and the ScreenMap frozen face are kept, the
  // materialization stays empty).
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
                                    sim::Player& /*old_owner*/,
                                    sim::Player& /*new_owner*/) {
  // L134-141
  // Force a state update for the old owner so the tooltip etc doesn't
  // show them as the owner(上游注释)
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
  // L143-148
  // Invalidate the frozen actor (which exists if this actor was captured
  // from an enemy) for the current owner(上游注释)
  const int owner_index = PlayerIndexOf(self.world(), self.Owner());
  if (owner_index < 0 ||
      owner_index >= static_cast<int>(vec_frozen_states_.size()))
    return;
  vec_frozen_states_[owner_index].frozen_actor->Invalidate();
}

// ———— HiddenUnderShroud(L39-72)————
// ———— HiddenUnderShroud (L39-72) ————

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
  // L46-56
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
  // L58-66
  if (by_player == nullptr)
    return true;

  const sim::PlayerRelationship relationship =
      self.Owner()->RelationshipWith(by_player);
  return sim::HasRelationship(info_.always_visible_relationships,
                              relationship) ||
         IsVisibleInner(self, *by_player);
}

}  // namespace ora::mods
