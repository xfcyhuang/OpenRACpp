// UPSTREAM: OpenRA.Game/Player.cs @b6fc03f(实现部分)+ Faction.cs 的解析面
//          The implementation half of Player.cs + the Faction.cs parse face.
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
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::sim {

namespace {

/// Exts.Random<T>(Exts.cs L170-187;只读集合 + r.Next(count);空集合
/// ArgumentException 文本逐字)
/// Exts.Random<T> (Exts.cs L170-187): the read-only collection + r.Next
/// (count); the empty-collection ArgumentException text verbatim.
template <class T>
T RandomOf(std::span<const T> ts, MersenneTwister& r) {
  if (ts.empty())
    throw std::invalid_argument(
        "Collection must not be empty. (Parameter 'ts')");
  return ts[static_cast<std::size_t>(r.Next(
      static_cast<std::int32_t>(ts.size())))];
}

/// 阵营记录表(world actor 的 TraitInfos<FactionInfo> 解析值)
/// The faction-record table (the parsed TraitInfos<FactionInfo> of the
/// world actor).
std::vector<FactionInfoData> FactionInfosOf(World& world,
                                            std::string_view system_actor) {
  std::vector<FactionInfoData> out;
  const game::ActorInfo* world_info =
      world.Map().Rules().FindActor(std::string{system_actor});
  if (world_info == nullptr)
    return out;
  for (const meta::RecordObject* rec : world_info->TraitInfosByInterface(
           "OpenRA.Traits.ITraitInfoInterface")) {
    // FactionInfo 类型过滤(上游 TraitInfos<FactionInfo> 的泛型过滤 = 记录
    // 短名匹配)
    // The FactionInfo type filter (upstream's generic filter = the record
    // short-name match).
    if (rec->record_desc().str_name != "FactionInfo")
      continue;
    out.push_back(FactionInfoData::Parse(*rec));
  }
  return out;
}

/// ResolveFaction 的 world 重载(Player.cs L139-143) | the world overload
/// of ResolveFaction (Player.cs L139-143).
FactionInfoData ResolveFactionWithWorld(World& world,
                                        const std::string& str_faction_name,
                                        MersenneTwister& player_random,
                                        bool require_selectable) {
  const std::vector<FactionInfoData> faction_infos = FactionInfosOf(
      world, world.Type() == WorldType::Editor ? "editorworld" : "world");
  return Player::ResolveFaction(str_faction_name, faction_infos,
                                player_random, require_selectable);
}

/// ResolveDisplayFaction(L145-150) | ResolveDisplayFaction (L145-150).
FactionInfoData ResolveDisplayFaction(World& world,
                                      const std::string& str_faction_name) {
  const std::vector<FactionInfoData> factions = FactionInfosOf(
      world, world.Type() == WorldType::Editor ? "editorworld" : "world");

  for (const FactionInfoData& f : factions)
    if (f.InternalName == str_faction_name)
      return f;
  // factions.First():空表 InvalidOperationException 等价
  // factions.First(): the empty-table InvalidOperationException equivalent.
  if (factions.empty())
    throw std::runtime_error("Sequence contains no elements");
  return factions.front();
}

}  // namespace

// ———— FactionInfoData(值袋槽位 → 解析值)————
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

  // RandomFactionMembers:FrozenSet<string> → 插入序 vector(GenericValue
  // 的 vector<GenericValue> 载荷)
  // RandomFactionMembers: the FrozenSet<string> → an insertion-ordered
  // vector (the vector<GenericValue> payload).
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

// ———— ResolveFaction(L116-137)————
FactionInfoData Player::ResolveFaction(const std::string& str_faction_name,
                                       std::span<const FactionInfoData> faction_infos,
                                       MersenneTwister& player_random,
                                       bool require_selectable) {
  // selectableFactions 过滤 + FirstOrDefault(名匹配)?? Random(缺省)
  // The selectableFactions filter + FirstOrDefault(name)?? Random(fallback).
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

  // Don't loop infinite(上游注释)| (upstream comment).
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

// ———— ctor(L152-228)————
Player::Player(World& world, const net::SessionClient* client,
               const map::PlayerReference& pr, MersenneTwister& player_random)
    : world_{&world}, pr_(pr) {
  const game::Ruleset& rules = world.Map().Rules();
  const game::ActorInfo* player_info =
      rules.FindActor(world.Type() == WorldType::Editor ? "editorplayer"
                                                        : "player");
  if (player_info != nullptr)
    for (const meta::RecordObject* rec : player_info->TraitInfosByInterface(
             "OpenRA.Traits.IBotInfo"))
      vec_bot_infos_.push_back(rec);

  // Real player or host-created bot(上游注释)| (upstream comment).
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
    // Map player(上游注释)| (upstream comment).
    // ClientIndex = host 管理员客户端(上游 TODO 保留)| the host-admin
    // client (the upstream TODO kept).
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
    // new LongBitSet<PlayerBitMask>(InternalName)(L198-199)
    const std::string* name = &str_internal_name_;
    PlayerMask = PlayerMaskSet{std::span<const std::string>{name, 1}};
  }

  // Set this property before running any Created callbacks on the player
  // actor(上游注释)| (upstream comment).
  b_is_bot_ = !str_bot_type_.empty();

  // Special case handling is required for the Player actor(上游注释:
  // 先赋未初始化 actor,再手动 Initialize —— PlayerActor 指针先于
  // Created 回调可用)
  // Special case handling is required for the Player actor (upstream
  // comment: assign the uninitialized actor first, Initialize by hand —
  // the PlayerActor pointer is available before the Created callbacks).
  const std::string player_actor_type =
      world.Type() == WorldType::Editor ? "editorplayer" : "player";
  TypeDictionary init_dict;
  OwnerInit owner_init{this};
  init_dict.Add(&owner_init);
  p_player_actor_ = world.CreateActor(false, player_actor_type, init_dict);
  p_player_actor_->Initialize(true);

  // Shroud(L213):Trait<Shroud>() 的解析面随 Shroud 批(部分覆盖装配面;
  // 消费面本批为空 —— 头注)
  // Shroud (L213): the Trait<Shroud>() face lands with the Shroud batch
  // (the partial-coverage assembly face; no consumers this batch — the
  // header note).

  // Enable the bot logic on the host(L216-224) | Enable the bot logic on
  // the host (L216-224).
  if (b_is_bot_ && world.IsHost()) {
    const std::vector<IBot*> bots =
        p_player_actor_->TraitsImplementing<IBot>();
    const auto it = std::find_if(bots.begin(), bots.end(),
                                 [&](const IBot* b) {
                                   return b->Info()->Type() == str_bot_type_;
                                 });
    if (it == bots.end())
      ;  // Log.Write("debug", ...)(日志面随平台批 | with the platform batch)
    else
      (*it)->Activate(this);
  }

  vec_unlock_render_player_ =
      p_player_actor_->TraitsImplementing<IUnlocksRenderPlayer>();
  vec_notify_disconnected_ =
      p_player_actor_->TraitsImplementing<INotifyPlayerDisconnected>();
}

// ———— UnlockedRenderPlayer(L95-104)————
bool Player::UnlockedRenderPlayer() const {
  for (const auto* x : vec_unlock_render_player_)
    if (x->RenderPlayerUnlocked())
      return true;
  return win_state_ != WinState::Undefined && !b_in_mission_map_;
}

// ———— ToString(L230-233)————
std::string Player::ToString() {
  return std::format("{} ({})", ResolvedPlayerName(), int4_client_index_);
}

// ———— ResolvePlayerName(L235-247)————
std::string Player::ResolvePlayerName() {
  if (b_is_bot_) {
    // botInfo.Name 与枚举名经 Fluent(Phase 6)—— 原文 + 序号直拼
    // (FluentProvider 缺位为 COVERAGE 登记偏离;枚举序 = 同型 bot 序号)
    // botInfo.Name and the enumeration go through Fluent (Phase 6) — raw
    // name + index concatenated (the missing FluentProvider is the
    // registered COVERAGE deviation; the index = the ordinal among
    // same-type bots).
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
    // botsOfSameType/First(b.Type == BotType):找不到 = First() 等价抛
    // botsOfSameType/First(b.Type == BotType): a miss is the First()
    // equivalent throw.
    throw std::runtime_error("Sequence contains no elements");
  }

  return str_player_name_;
}

// ———— ResolvedPlayerName(L107-114;??= 缓存)————
const std::string& Player::ResolvedPlayerName() {
  if (str_resolved_player_name_.empty())
    str_resolved_player_name_ = ResolvePlayerName();
  return str_resolved_player_name_;
}

// ———— RelationshipWith(L249-265)————
PlayerRelationship Player::RelationshipWith(const Player* other) const {
  if (this == other)
    return PlayerRelationship::Ally;

  // Observers are considered allies to active combatants(上游注释)
  // Observers are considered allies to active combatants (upstream comment).
  if (other == nullptr || other->Spectating())
    return b_non_combatant_ ? PlayerRelationship::Neutral
                            : PlayerRelationship::Ally;

  if (AlliedPlayersMask.Overlaps(other->PlayerMask))
    return PlayerRelationship::Ally;

  if (EnemyPlayersMask.Overlaps(other->PlayerMask))
    return PlayerRelationship::Enemy;

  return PlayerRelationship::Neutral;
}

// ———— PlayerDisconnected(L302-306)————
void Player::PlayerDisconnected(Player& p) {
  for (auto* np : vec_notify_disconnected_)
    np->PlayerDisconnected(*p_player_actor_, p);
}

}  // namespace ora::sim
