// UPSTREAM: OpenRA.Mods.Common/Traits/World/CreateMapPlayers.cs @b6fc03f
//          (实现部分)+ 本批 mods trait 的注册表装配
//          The implementation half of CreateMapPlayers.cs + this batch's
//          mods-trait registry assembly.
import std;

#include "mods/create_map_players.hpp"

#include "game/actor_info.hpp"
#include "map/map.hpp"
#include "map/map_players.hpp"
#include "mods/health.hpp"
#include "mods/pathfinding/locomotor.hpp"
#include "mods/pathfinding/path_finder.hpp"
#include "mods/armament.hpp"
#include "mods/body_orientation.hpp"
#include "mods/mobile.hpp"
#include "mods/unit_order_generator.hpp"
#include "net/session.hpp"
#include "sim/actor.hpp"
#include "sim/actor_map.hpp"
#include "sim/control_groups.hpp"
#include "sim/order_generator.hpp"
#include "sim/player.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— CreateMapPlayers.CreatePlayers(L87-140)————
void CreateMapPlayers::CreatePlayers(World& w, MersenneTwister& player_random) {
  const map::MapPlayers map_players{w.Map().PlayerDefinitions(),
                                    core::Color::FromArgbRaw(0xFF4B4B4B)};
  const auto& players = map_players.Players;

  std::vector<Player*> world_players;
  bool world_owner_found = false;

  // Create the unplayable map players -- neutral, shellmap, scripted, etc.
  // (上游注释)| (upstream comment)
  for (const auto& [name, pr] : players) {
    if (pr.Playable)
      continue;
    auto* player = w.AdoptPlayer(
        std::make_unique<Player>(w, nullptr, pr, player_random));
    world_players.push_back(player);

    if (pr.OwnsWorld) {
      world_owner_found = true;
      w.SetWorldOwner(player);
    }
  }

  if (!world_owner_found)
    throw std::runtime_error(std::format(
        "Map {} does not define a player actor owning the world.",
        w.Map().Title()));

  Player* local_player = nullptr;

  // Create the regular playable players.(上游注释)| (upstream comment)
  for (const auto& [slot_key, slot] : w.LobbyInfo().vec_slots) {
    const net::SessionClient* client = w.LobbyInfo().ClientInSlot(slot_key);
    if (client == nullptr)
      continue;

    // players[kv.Value.PlayerReference]:缺失键 = KeyNotFoundException 等价
    // players[kv.Value.PlayerReference]: a missing key is the
    // KeyNotFoundException equivalent.
    const map::PlayerReference* pr = map_players.Find(slot.PlayerReference);
    if (pr == nullptr)
      throw std::runtime_error(
          "The given key was not present in the dictionary.");
    auto* player = w.AdoptPlayer(
        std::make_unique<Player>(w, client, *pr, player_random));
    world_players.push_back(player);

    if (client->Index == w.LocalClientId())
      local_player = player;
  }

  // Create a player that is allied with everyone for shared observer
  // shroud.(上游注释)| (upstream comment)
  {
    map::PlayerReference everyone;
    everyone.Name = "Everyone";
    everyone.NonCombatant = true;
    everyone.Spectating = true;
    everyone.Faction = "Random";
    for (Player* p : world_players)
      if (!p->NonCombatant() && p->Playable())
        everyone.Allies.push_back(p->InternalName());
    world_players.push_back(w.AdoptPlayer(
        std::make_unique<Player>(w, nullptr, everyone, player_random)));
  }

  w.SetPlayers(world_players, local_player);

  for (Player* p : w.Players())
    for (Player* q : w.Players())
      SetupPlayerMasks(p, q);
}

// ———— SetupPlayerMasks(L142-174)————
void CreateMapPlayers::SetupPlayerMasks(Player* p, Player* q) {
  World& w = p->GetWorld();
  if (!p->Spectating())
    w.AllPlayersMask = w.AllPlayersMask.Union(p->PlayerMask);

  const std::vector<std::string>& allies = p->PlayerReference().Allies;
  const bool allied = p == q || std::find(allies.begin(), allies.end(),
                                          q->InternalName()) != allies.end();
  if (allied) {
    p->AlliedPlayersMask = p->AlliedPlayersMask.Union(q->PlayerMask);
    return;
  }

  const std::vector<std::string>& enemies = p->PlayerReference().Enemies;
  if (std::find(enemies.begin(), enemies.end(), q->InternalName()) !=
      enemies.end()) {
    p->EnemyPlayersMask = p->EnemyPlayersMask.Union(q->PlayerMask);
    return;
  }

  // HACK: Map players share a ClientID with the host, so would otherwise
  // take the host's team stance instead of being neutral(上游注释)
  if (p->PlayerReference().Playable && q->PlayerReference().Playable) {
    // Stances set via lobby teams(上游注释)| (upstream comment)
    // GetClientForPlayer(L176-179):缺客户端 = null 面
    // GetClientForPlayer (L176-179): a missing client is the null face.
    const net::SessionClient* pc =
        w.LobbyInfo().ClientWithIndex(p->ClientIndex());
    const net::SessionClient* qc =
        w.LobbyInfo().ClientWithIndex(q->ClientIndex());
    if (pc != nullptr && qc != nullptr) {
      if (pc->Team != 0 && pc->Team == qc->Team)
        p->AlliedPlayersMask = p->AlliedPlayersMask.Union(q->PlayerMask);
      else
        p->EnemyPlayersMask = p->EnemyPlayersMask.Union(q->PlayerMask);
    }
  }
}

// ———— mods trait 注册表装配(与 sim::RegisterWorldTraits 同形)————
void RegisterCommonTraits() {
  static const bool b_registered = [] {
    sim::TraitRegistry& registry = sim::TraitRegistry::Instance();

    // FactionInfo.Create(init) → new Faction()(Faction.cs 的空运行时类)
    // FactionInfo.Create(init) → new Faction() (Faction.cs's empty runtime
    // class).
    registry.Register(
        "FactionInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Faction>();
        });

    // HealthInfo.Create(init) → new Health(init, this);RulesetLoaded 的
    // 无 HitShape 抛随工厂面(COVERAGE 时点偏离)
    // HealthInfo.Create(init) → new Health(init, this); the RulesetLoaded
    // no-HitShape throw rides the factory face (the COVERAGE timing
    // deviation).
    registry.Register(
        "HealthInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          // 上游查具体 Info 类(HasTraitInfo<HitShapeInfo>() —— 其接口集
          // 不含 IHitShapeInfo),按记录全名匹配
          // Upstream queries the concrete Info class
          // (HasTraitInfo<HitShapeInfo>() — its interface set carries no
          // IHitShapeInfo), matched here by record full name.
          bool has_hit_shape = false;
          for (const auto& rec_trait :
               init.Self().Info()->TraitsInConstructOrder())
            if (rec_trait->record_desc().str_full_name ==
                std::string_view{
                    "OpenRA.Mods.Common.Traits.HitShapeInfo"}) {
              has_hit_shape = true;
              break;
            }
          if (!has_hit_shape)
            throw yaml::YamlException(
                "Actors with Health need at least one HitShape trait!");
          const HealthInfoData data = HealthInfoData::Parse(rec_info);
          return arena.Create<Health>(init, data);
        });

    // ActorMapInfo.Create(init) → new ActorMap(init.World, this)
    registry.Register(
        "ActorMapInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const std::int64_t bin_size =
              sim::RecordFieldInt(rec_info, "BinSize").value_or(10);
          return arena.Create<sim::ActorMap>(init.Self().world(),
                                             static_cast<int>(bin_size));
        });

    // ControlGroupsInfo.Create(init) → new ControlGroups(init.World, this)
    registry.Register(
        "ControlGroupsInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          std::vector<std::string> groups{"1", "2", "3", "4", "5",
                                          "6", "7", "8", "9", "0"};
          // 上游 ImmutableArray 默认;无自定义加载面(字段不可经 yaml 覆盖)
          // Upstream's ImmutableArray default; no custom load face (the
          // field is not yaml-overridable).
          (void)rec_info;
          return arena.Create<sim::ControlGroups>(init.Self().world(),
                                                  std::move(groups));
        });

    // CreateMapPlayersInfo.Create(init) → new CreateMapPlayers()
    registry.Register(
        "CreateMapPlayersInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<CreateMapPlayers>();
        });

    // LocomotorInfo.Create(init) → new Locomotor(init.Self, this);字段面
    // 经值袋(LoadSpeeds 已由 loaders.cpp 的生成加载器物化)
    // LocomotorInfo.Create(init) → new Locomotor(init.Self, this); the
    // field face via the bag (LoadSpeeds is materialized by loaders.cpp's
    // generated loader).
    registry.Register(
        "LocomotorInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          pathfinding::LocomotorInfo info = pathfinding::ParseLocomotorInfo(rec_info);
          return arena.Create<pathfinding::Locomotor>(init.Self(),
                                                     std::move(info));
        });

    // PathFinderInfo.Create(init) → new PathFinder(init.Self, this)
    registry.Register(
        "PathFinderInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const std::int64_t weight = sim::RecordFieldInt(
              rec_info, "HeuristicWeightPercentage").value_or(125);
          return arena.Create<pathfinding::PathFinder>(
              init.Self(), static_cast<int>(weight));
        });

    // DefaultOrderGenerator 名字分派(Manifest.DefaultOrderGenerator 键;
    // World ctor 消费)
    // The DefaultOrderGenerator name dispatch (the
    // Manifest.DefaultOrderGenerator key; consumed by the World ctor).
    sim::RegisterOrderGenerator(
        "UnitOrderGenerator",
        [](World& world) -> std::unique_ptr<sim::IOrderGenerator> {
          return std::make_unique<UnitOrderGenerator>(world);
        });

    // ———— 第三批:Mobile/BodyOrientation/Armament ————

    // MobileInfo.Create(init) → new Mobile(init, this);LocomotorInfo 解析
    // = RulesetLoaded 的工厂时点面(D 系登记)
    // MobileInfo.Create(init) → new Mobile(init, this); the LocomotorInfo
    // resolution = the factory-time face of RulesetLoaded (the D-series
    // registration).
    registry.Register(
        "MobileInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          MobileInfoData data = MobileInfoData::Parse(rec_info);
          data.locomotor_info =
              ResolveMobileLocomotorInfo(init.Self().world(),
                                         data.str_locomotor);
          return arena.Create<Mobile>(init, std::move(data));
        });

    // BodyOrientationInfo.Create(init) → new BodyOrientation(init, this)
    registry.Register(
        "BodyOrientationInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const BodyOrientationInfoData data =
              BodyOrientationInfoData::Parse(rec_info);
          return arena.Create<BodyOrientation>(init, data);
        });

    // ArmamentInfo.Create(init) → new Armament(init.Self, this);武器解析
    // 与校验 = RulesetLoaded 的工厂时点面(异常文本逐字)
    // ArmamentInfo.Create(init) → new Armament(init.Self, this); the
    // weapon resolution + validation = the factory-time face of
    // RulesetLoaded (exception texts verbatim).
    registry.Register(
        "ArmamentInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const ArmamentInfoData data =
              ArmamentInfoData::Parse(rec_info, init.Self().world());
          return arena.Create<Armament>(init, std::move(data));
        });

    return true;
  }();
  (void)b_registered;
}

// ———— 第三批 [VerifySync] 哈希注册(gen/sync_gen.cpp 成员表:
//      Mobile {Facing,FromCell,ToCell,CenterPosition} /
//      BodyOrientation {QuantizedFacings};组合协议 = 0 XOR 成员哈希)————
// ———— The batch-3 [VerifySync] hash registrations (the gen/sync_gen.cpp
//      member tables: Mobile {Facing,FromCell,ToCell,CenterPosition} /
//      BodyOrientation {QuantizedFacings}; the combination protocol = 0
//      XOR the member hashes) ————

namespace {

int MobileSyncHash(const sim::ISync* s) {
  const auto* mobile = static_cast<const Mobile*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(
          sim::sync::CombineSyncHash(
              sim::sync::CombineSyncHash(
                  0, sim::sync::HashWAngle(mobile->Facing())),
              sim::sync::HashCPos(mobile->FromCell())),
          sim::sync::HashCPos(mobile->ToCell())),
      sim::sync::HashWPos(mobile->CenterPosition()));
}

int BodyOrientationSyncHash(const sim::ISync* s) {
  const auto* body_orientation = static_cast<const BodyOrientation*>(s);
  return sim::sync::CombineSyncHash(
      0, body_orientation->QuantizedFacings());
}

const bool b_registered_batch3_sync_hash = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Mobile",
                                &MobileSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.BodyOrientation",
      &BodyOrientationSyncHash);
  return true;
}();

}  // namespace

}  // namespace ora::mods
