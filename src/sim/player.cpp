import std;

#include "sim/player.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "net/session.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/shroud.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::sim {

namespace {

template <class T>
T RandomOf(std::span<const T> ts, MersenneTwister& r) {
  if (ts.empty())
    throw std::invalid_argument(
        "Collection must not be empty. (Parameter 'ts')");
  return ts[static_cast<std::size_t>(r.Next(
      static_cast<std::int32_t>(ts.size())))];
}

std::vector<FactionInfoData> FactionInfosOf(World& world,
                                            std::string_view system_actor) {
  std::vector<FactionInfoData> out;
  const game::ActorInfo* world_info =
      world.Map().Rules().FindActor(std::string{system_actor});
  if (world_info == nullptr)
    return out;
  for (const meta::RecordObject* rec : world_info->TraitInfosByInterface(
           "OpenRA.Traits.ITraitInfoInterface")) {

    if (rec->record_desc().str_name != "FactionInfo")
      continue;
    out.push_back(FactionInfoData::Parse(*rec));
  }
  return out;
}

FactionInfoData ResolveFactionWithWorld(World& world,
                                        const std::string& str_faction_name,
                                        MersenneTwister& player_random,
                                        bool require_selectable) {
  const std::vector<FactionInfoData> faction_infos = FactionInfosOf(
      world, world.Type() == WorldType::Editor ? "editorworld" : "world");
  return Player::ResolveFaction(str_faction_name, faction_infos,
                                player_random, require_selectable);
}

FactionInfoData ResolveDisplayFaction(World& world,
                                      const std::string& str_faction_name) {
  const std::vector<FactionInfoData> factions = FactionInfosOf(
      world, world.Type() == WorldType::Editor ? "editorworld" : "world");

  for (const FactionInfoData& f : factions)
    if (f.InternalName == str_faction_name)
      return f;

  if (factions.empty())
    throw std::runtime_error("Sequence contains no elements");
  return factions.front();
}

}

FactionInfoData FactionInfoData::Parse(const meta::RecordObject& rec_info) {
  FactionInfoData data;
  if (auto v = RecordFieldString(rec_info, "Name"))
    data.Name = std::string{*v};
  if (auto v = RecordFieldString(rec_info, "InternalName"))
    data.InternalName = std::string{*v};
  if (auto v = RecordFieldString(rec_info, "Side"))
    data.Side = std::string{*v};
  if (auto v = RecordFieldString(rec_info, "Description"))
    data.Description = std::string{*v};
  if (auto v = RecordFieldInt(rec_info, "Selectable"))
    data.Selectable = *v != 0;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      if (fields[i]->str_name != "RandomFactionMembers")
        continue;
      const meta::GenericValue& v = generated->Slot(i);
      if (auto* list = std::get_if<std::vector<meta::GenericValue>>(&v.val)) {
        data.RandomFactionMembers.reserve(list->size());
        for (const meta::GenericValue& item : *list)
          if (auto* s = std::get_if<std::string>(&item.val))
            data.RandomFactionMembers.push_back(*s);
      }
    }
  }
  return data;
}

FactionInfoData Player::ResolveFaction(const std::string& str_faction_name,
                                       std::span<const FactionInfoData> faction_infos,
                                       MersenneTwister& player_random,
                                       bool require_selectable) {

  std::vector<const FactionInfoData*> selectable;
  selectable.reserve(faction_infos.size());
  for (const FactionInfoData& f : faction_infos)
    if (!require_selectable || f.Selectable)
      selectable.push_back(&f);

  const FactionInfoData* selected = nullptr;
  for (const FactionInfoData* f : selectable)
    if (f->InternalName == str_faction_name) {
      selected = f;
      break;
    }
  if (selected == nullptr)
    selected = RandomOf<const FactionInfoData*>(
        std::span{selectable.data(), selectable.size()}, player_random);

  for (int i = 0; i <= 10 && !selected->RandomFactionMembers.empty(); i++) {
    const std::string faction =
        RandomOf<std::string>(selected->RandomFactionMembers, player_random);
    selected = nullptr;
    for (const FactionInfoData* f : selectable)
      if (f->InternalName == faction) {
        selected = f;
        break;
      }

    if (selected == nullptr)
      throw yaml::YamlException(std::format("Unknown faction: {}", faction));
  }

  return *selected;
}

Player::Player(World& world, const net::SessionClient* client,
               const map::PlayerReference& pr, MersenneTwister& player_random)
    : world_{&world}, pr_(pr) {

  str_internal_name_ = pr.Name;

  const game::Ruleset& rules = world.Map().Rules();
  const game::ActorInfo* player_info =
      rules.FindActor(world.Type() == WorldType::Editor ? "editorplayer"
                                                        : "player");
  if (player_info != nullptr)
    for (const meta::RecordObject* rec : player_info->TraitInfosByInterface(
             "OpenRA.Traits.IBotInfo"))
      vec_bot_infos_.push_back(rec);

  if (client != nullptr) {
    int4_client_index_ = client->Index;
    color_ = client->Color;
    str_player_name_ = client->Name;

    str_bot_type_ = client->Bot;
    faction_ = ResolveFactionWithWorld(world, client->Faction, player_random,
                                       !pr.LockFaction);
    display_faction_ = ResolveDisplayFaction(world, client->Faction);

    auto* assign_spawn_points =
        world.WorldActor() != nullptr
            ? world.WorldActor()->TraitOrDefault<IAssignSpawnPoints>()
            : nullptr;
    cpos_home_location_ =
        assign_spawn_points != nullptr
            ? assign_spawn_points->AssignHomeLocation(world,
                                                      *const_cast<net::SessionClient*>(client),
                                                      player_random)
            : pr.HomeLocation;
    int4_spawn_point_ = assign_spawn_points != nullptr
                            ? assign_spawn_points->SpawnPointForPlayer(this)
                            : client->SpawnPoint;
    int4_display_spawn_point_ = client->SpawnPoint;

    int4_handicap_ = client->Handicap;
  } else {

    const net::SessionClient* admin = nullptr;
    for (const net::SessionClient& c : world.LobbyInfo().vec_clients)
      if (c.IsAdmin) {
        admin = &c;
        break;
      }
    int4_client_index_ = admin != nullptr ? admin->Index : 0;
    color_ = pr.Color;
    str_player_name_ = pr.Name;
    b_non_combatant_ = pr.NonCombatant;
    b_playable_ = pr.Playable;
    b_spectating_ = pr.Spectating;
    str_bot_type_ = pr.Bot;
    faction_ = ResolveFactionWithWorld(world, pr.Faction, player_random,
                                       false);
    display_faction_ = ResolveDisplayFaction(world, pr.Faction);
    cpos_home_location_ = pr.HomeLocation;
    int4_spawn_point_ = int4_display_spawn_point_ = 0;
    int4_handicap_ = pr.Handicap;
  }

  if (!b_spectating_) {

    const std::string* name = &str_internal_name_;
    PlayerMask = PlayerMaskSet{std::span<const std::string>{name, 1}};
  }

  b_is_bot_ = !str_bot_type_.empty();

  const std::string player_actor_type =
      world.Type() == WorldType::Editor ? "editorplayer" : "player";
  TypeDictionary init_dict;
  OwnerInit owner_init{this};
  init_dict.Add(&owner_init);

  p_player_actor_ = world.Arena().Create<sim::Actor>(
      world, player_actor_type, init_dict);
  p_player_actor_->Initialize(true);

  p_shroud_ = p_player_actor_->TraitOrDefault<Shroud>();
  p_frozen_actor_layer_ =
      p_player_actor_->TraitOrDefault<FrozenActorLayer>();

  if (b_is_bot_ && world.IsHost()) {
    const std::vector<IBot*> bots =
        p_player_actor_->TraitsImplementing<IBot>();
    const auto it = std::find_if(bots.begin(), bots.end(),
                                 [&](const IBot* b) {
                                   return b->Info()->Type() == str_bot_type_;
                                 });
    if (it == bots.end())
      ;
    else
      (*it)->Activate(this);
  }

  vec_unlock_render_player_ =
      p_player_actor_->TraitsImplementing<IUnlocksRenderPlayer>();
  vec_notify_disconnected_ =
      p_player_actor_->TraitsImplementing<INotifyPlayerDisconnected>();
}

bool Player::UnlockedRenderPlayer() const {
  for (const auto* x : vec_unlock_render_player_)
    if (x->RenderPlayerUnlocked())
      return true;
  return win_state_ != WinState::Undefined && !b_in_mission_map_;
}

std::string Player::ToString() {
  return std::format("{} ({})", ResolvedPlayerName(), int4_client_index_);
}

std::string Player::ResolvePlayerName() {
  if (b_is_bot_) {

    for (const meta::RecordObject* rec : vec_bot_infos_) {
      const auto type = RecordFieldString(*rec, "Type");
      if (type && std::string{*type} == str_bot_type_) {
        const auto name = RecordFieldString(*rec, "Name");
        int number = 1;
        for (const Player* p : world_->Players())
          if (p != this && p->BotType() == str_bot_type_)
            ++number;
        return std::format("{} {}", name.value_or(""), number);
      }
    }

    throw std::runtime_error("Sequence contains no elements");
  }

  return str_player_name_;
}

const std::string& Player::ResolvedPlayerName() {
  if (str_resolved_player_name_.empty())
    str_resolved_player_name_ = ResolvePlayerName();
  return str_resolved_player_name_;
}

PlayerRelationship Player::RelationshipWith(const Player* other) const {
  if (this == other)
    return PlayerRelationship::Ally;

  if (other == nullptr || other->Spectating())
    return b_non_combatant_ ? PlayerRelationship::Neutral
                            : PlayerRelationship::Ally;

  if (AlliedPlayersMask.Overlaps(other->PlayerMask))
    return PlayerRelationship::Ally;

  if (EnemyPlayersMask.Overlaps(other->PlayerMask))
    return PlayerRelationship::Enemy;

  return PlayerRelationship::Neutral;
}

void Player::PlayerDisconnected(Player& p) {
  for (auto* np : vec_notify_disconnected_)
    np->PlayerDisconnected(*p_player_actor_, p);
}

Shroud* Player::GetShroud() const {
  if (p_shroud_ == nullptr && p_player_actor_ != nullptr)
    p_shroud_ = p_player_actor_->TraitOrDefault<Shroud>();
  return p_shroud_;
}

FrozenActorLayer* Player::GetFrozenActorLayer() const {
  if (p_frozen_actor_layer_ == nullptr && p_player_actor_ != nullptr)
    p_frozen_actor_layer_ =
        p_player_actor_->TraitOrDefault<FrozenActorLayer>();
  return p_frozen_actor_layer_;
}

}
