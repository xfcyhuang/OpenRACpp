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
#include "mods/affects_shroud.hpp"
#include "mods/armament.hpp"
#include "mods/armor.hpp"
#include "mods/attack_base.hpp"
#include "mods/auto_target.hpp"
#include "mods/blocks_projectiles.hpp"
#include "mods/building.hpp"
#include "mods/building_influence.hpp"
#include "mods/frozen_under_fog.hpp"
#include "mods/hit_shape.hpp"
#include "mods/missile_projectiles.hpp"
#include "mods/player_resources.hpp"
#include "mods/production.hpp"
#include "mods/production_support.hpp"
#include "mods/smudge_layer.hpp"
#include "sim/shroud.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "mods/targetable.hpp"
#include "mods/warheads.hpp"
#include "mods/projectiles.hpp"
#include "mods/body_orientation.hpp"
#include "mods/mobile.hpp"
#include "mods/unit_order_generator.hpp"
#include "mods/turreted.hpp"
#include "mods/attack_follow.hpp"
#include "mods/power.hpp"
#include "mods/resource_layer.hpp"
#include "mods/dock_client.hpp"
#include "mods/dock_host.hpp"
#include "mods/stores_resources.hpp"
#include "mods/harvester.hpp"
#include "mods/refinery.hpp"
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

    // ———— 第四批:Shroud/FrozenActorLayer/HitShape/Armor/RevealsShroud/
    //      AttackFrontal/AutoTarget(±Priority)/BlocksProjectiles ————

    // ShroudInfo.Create(init) → new Shroud(init.Self, this)
    registry.Register(
        "ShroudInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          sim::ShroudInfoData data;
          if (sim::RecordFieldInt(rec_info, "FogCheckboxEnabled"))
            data.b_fog_checkbox_enabled =
                *sim::RecordFieldInt(rec_info, "FogCheckboxEnabled") != 0;
          if (sim::RecordFieldInt(rec_info, "ExploredMapCheckboxEnabled"))
            data.b_explored_map_checkbox_enabled =
                *sim::RecordFieldInt(rec_info,
                                     "ExploredMapCheckboxEnabled") != 0;
          return arena.Create<sim::Shroud>(init.Self(), data);
        });

    // FrozenActorLayerInfo.Create(init) → new FrozenActorLayer(init.Self,
    // this)(Requires<ShroudInfo> 的构造序由 ActorInfo 拓扑序保证)
    registry.Register(
        "FrozenActorLayerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          sim::FrozenActorLayerInfoData data;
          if (const auto v =
                  sim::RecordFieldInt(rec_info, "BinSize"))
            data.int4_bin_size = static_cast<int>(*v);
          return arena.Create<sim::FrozenActorLayer>(init.Self(), data);
        });

    // HitShapeInfo.Create(init) → new HitShape(init, this)(Type 的
    // LoadShape 嵌套记录在此物化;Initialize 随 ParseHitShape 调用)
    registry.Register(
        "HitShapeInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<HitShape>(
              init, HitShapeInfoData::Parse(rec_info));
        });

    // ArmorInfo.Create(init) → new Armor(this)
    registry.Register(
        "ArmorInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Armor>(ArmorInfoData::Parse(rec_info));
        });

    // RevealsShroudInfo.Create(init) → new RevealsShroud(this)
    registry.Register(
        "RevealsShroudInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<RevealsShroud>(
              RevealsShroudInfoData::Parse(rec_info));
        });

    // AttackFrontalInfo.Create(init) → new AttackFrontal(init.Self, this)
    // (Requires<IFacingInfo> 的构造序由 ActorInfo 拓扑序保证)
    registry.Register(
        "AttackFrontalInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AttackFrontal>(
              init, AttackBaseInfoData::Parse(rec_info));
        });

    // AutoTargetInfo.Create(init) → new AutoTarget(init, this)
    registry.Register(
        "AutoTargetInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AutoTarget>(
              init, AutoTargetInfoData::Parse(rec_info));
        });

    // AutoTargetPriorityInfo.Create(init) → new AutoTargetPriority(this)
    registry.Register(
        "AutoTargetPriorityInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AutoTargetPriority>(
              AutoTargetPriorityInfoData::Parse(rec_info));
        });

    // TargetableInfo.Create(init) → new Targetable(this)
    registry.Register(
        "TargetableInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Targetable>(
              TargetableInfoData::Parse(rec_info));
        });

    // BlocksProjectilesInfo.Create(init) → new BlocksProjectiles(this)
    registry.Register(
        "BlocksProjectilesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<BlocksProjectiles>(
              BlocksProjectilesInfoData::Parse(rec_info));
        });

    // ———— 第四批:弹丸/战头注册表(ProjectileRegistry/WarheadRegistry)————

    sim::ProjectileRegistry::Instance().Register(
        "BulletInfo",
        [](const meta::RecordObject& rec_info) -> sim::IProjectileInfo* {
          return new BulletInfo(BulletInfoData::Parse(rec_info));
        });

    sim::ProjectileRegistry::Instance().Register(
        "InstantHitInfo",
        [](const meta::RecordObject& rec_info) -> sim::IProjectileInfo* {
          return new InstantHitInfo(InstantHitInfoData::Parse(rec_info));
        });

    WarheadRegistry::Instance().Register(
        "SpreadDamageWarhead",
        [](const meta::RecordObject& rec_info) -> IWarhead* {
          return SpreadDamageWarhead::Parse(rec_info).release();
        });

    // ———— 第五批:弹丸/战头三件(注册表)————

    sim::ProjectileRegistry::Instance().Register(
        "MissileInfo",
        [](const meta::RecordObject& rec_info) -> sim::IProjectileInfo* {
          return new MissileInfo(MissileInfoData::Parse(rec_info));
        });

    sim::ProjectileRegistry::Instance().Register(
        "GravityBombInfo",
        [](const meta::RecordObject& rec_info) -> sim::IProjectileInfo* {
          return new GravityBombInfo(GravityBombInfoData::Parse(rec_info));
        });

    sim::ProjectileRegistry::Instance().Register(
        "TeslaZapInfo",
        [](const meta::RecordObject& rec_info) -> sim::IProjectileInfo* {
          return new TeslaZapInfo(TeslaZapInfoData::Parse(rec_info));
        });

    WarheadRegistry::Instance().Register(
        "TargetDamageWarhead",
        [](const meta::RecordObject& rec_info) -> IWarhead* {
          return TargetDamageWarhead::Parse(rec_info).release();
        });

    WarheadRegistry::Instance().Register(
        "CreateEffectWarhead",
        [](const meta::RecordObject& rec_info) -> IWarhead* {
          return CreateEffectWarhead::Parse(rec_info).release();
        });

    WarheadRegistry::Instance().Register(
        "LeaveSmudgeWarhead",
        [](const meta::RecordObject& rec_info) -> IWarhead* {
          return LeaveSmudgeWarhead::Parse(rec_info).release();
        });

    // ———— 第五批:迷雾修饰两件 + 建筑/生产链(注册表)————

    // FrozenUnderFogInfo.Create(init) → new FrozenUnderFog(init, this)
    registry.Register(
        "FrozenUnderFogInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<FrozenUnderFog>(
              init, FrozenUnderFogInfoData::Parse(rec_info));
        });

    // HiddenUnderShroudInfo.Create(init) → new HiddenUnderShroud(this)
    registry.Register(
        "HiddenUnderShroudInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<HiddenUnderShroud>(
              HiddenUnderShroudInfoData::Parse(rec_info));
        });

    // SmudgeLayerInfo.Create(init) → new SmudgeLayer(init.Self, this)
    registry.Register(
        "SmudgeLayerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<SmudgeLayer>(
              init.Self(), SmudgeLayerInfoData::Parse(rec_info));
        });

    // BuildingInfo.Create(init) → new Building(init, this)
    registry.Register(
        "BuildingInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Building>(
              init, BuildingInfoData::Parse(rec_info));
        });

    // BuildableInfo → TraitInfo<Buildable>(空运行时类)
    registry.Register(
        "BuildableInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Buildable>();
        });

    // ValuedInfo → TraitInfo<Valued>(空运行时类)
    registry.Register(
        "ValuedInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Valued>();
        });

    // ExitInfo.Create(init) → new Exit(this)
    registry.Register(
        "ExitInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Exit>(ExitInfoData::Parse(rec_info));
        });

    // ReservableInfo → TraitInfo<Reservable>
    registry.Register(
        "ReservableInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Reservable>();
        });

    // RallyPointInfo.Create(init) → new RallyPoint(init.Self, this)
    registry.Register(
        "RallyPointInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<RallyPoint>(
              init.Self(), RallyPointInfoData::Parse(rec_info));
        });

    // ProvidesPrerequisiteInfo.Create(init) → new ProvidesPrerequisite(
    // init, this)
    registry.Register(
        "ProvidesPrerequisiteInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ProvidesPrerequisite>(
              init, ProvidesPrerequisiteInfoData::Parse(rec_info));
        });

    // TechTreeInfo.Create(init) → new TechTree(init)
    registry.Register(
        "TechTreeInfo",
        [](const meta::RecordObject&, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<TechTree>(init);
        });

    // DeveloperModeInfo.Create(init) → new DeveloperMode(this)(cheat
    // 字段随 lobby/命令批接线)
    registry.Register(
        "DeveloperModeInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<DeveloperMode>();
        });

    // ProductionInfo.Create(init) → new Production(init, this)
    registry.Register(
        "ProductionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Production>(
              init, ProductionInfoData::Parse(rec_info));
        });

    // ProductionQueueInfo.Create(init) → new ProductionQueue(init,
    // this);RulesetLoaded 的 LowPowerModifier 校验随工厂时点(异常
    // 文本逐字)
    registry.Register(
        "ProductionQueueInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          ProductionQueueInfoData data =
              ProductionQueueInfoData::Parse(rec_info);
          if (data.int4_low_power_modifier <= 0)
            throw yaml::YamlException(
                "Production queue must have LowPowerModifier of at "
                "least 1.");
          return arena.Create<ProductionQueue>(init, std::move(data));
        });

    // PlayerResourcesInfo.Create(init) → new PlayerResources(init.Self,
    // this)
    registry.Register(
        "PlayerResourcesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<PlayerResources>(
              init, PlayerResourcesInfoData::Parse(rec_info));
        });

    // ClassicProductionQueueInfo.Create(init) → new
    // ClassicProductionQueue(init, this)(ra 的共享队列;SpeedUp/
    // BuildTimeSpeedReduction 的加速面随批 —— 缺省 [100] 无加速等价)
    registry.Register(
        "ClassicProductionQueueInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          ProductionQueueInfoData data =
              ProductionQueueInfoData::Parse(rec_info);
          if (data.int4_low_power_modifier <= 0)
            throw yaml::YamlException(
                "Production queue must have LowPowerModifier of at "
                "least 1.");
          return arena.Create<ClassicProductionQueue>(
              init, std::move(data));
        });

    // BuildingInfluenceInfo.Create(init) → new BuildingInfluence(
    // init.World)
    registry.Register(
        "BuildingInfluenceInfo",
        [](const meta::RecordObject&, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<BuildingInfluence>(init.Self().world());
        });

    // ———— 第六批增补:ClassicFacingBodyOrientation(Mods.Cnc 的
    // BodyOrientation 子类;QuantizeFacing 覆写在 facings == 32 时走
    // ClassicIndexFacing/SpriteFacings 表 —— 该域随渲染批的序列表面
    // 接线,其余域与基类同式;COVERAGE 登记)————
    // The batch-6 addition: ClassicFacingBodyOrientation (the Mods.Cnc
    // subclass of BodyOrientation; its QuantizeFacing override goes
    // through the ClassicIndexFacing/SpriteFacings table at facings == 32
    // — that domain rides the render batch's sequence face, every other
    // domain matches the base formula; registered in COVERAGE).
    registry.Register(
        "ClassicFacingBodyOrientationInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<BodyOrientation>(
              init, BodyOrientationInfoData::Parse(rec_info));
        });

    // ———— 第六批:AttackFollow 族/Turreted ————

    // AttackFollowInfo.Create(init) → new AttackFollow(init.Self, this)
    registry.Register(
        "AttackFollowInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AttackFollow>(
              init, AttackFollowInfoData::Parse(rec_info));
        });

    // AttackTurretedInfo.Create(init) → new AttackTurreted(init.Self,
    // this)
    registry.Register(
        "AttackTurretedInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AttackTurreted>(
              init, AttackTurretedInfoData::Parse(rec_info));
        });

    // TurretedInfo.Create(init) → new Turreted(init, this)(init 的按名
    // 匹配面携带声明实例名)
    registry.Register(
        "TurretedInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          TurretedInfoData data = TurretedInfoData::Parse(rec_info);
          std::string str_instance_name;
          if (init.Self().Info() != nullptr)
            str_instance_name =
                std::string{init.Self().Info()->InstanceNameOf(&rec_info)};
          return arena.Create<Turreted>(init, data,
                                        std::move(str_instance_name));
        });

    // ———— 第六批:电源链 ————

    // PowerManagerInfo.Create(init) → new PowerManager(init.Self, this)
    registry.Register(
        "PowerManagerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<PowerManager>(
              init, PowerManagerInfoData::Parse(rec_info));
        });

    // PowerInfo.Create(init) → new Power(init.Self, this)
    registry.Register(
        "PowerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Power>(init, PowerInfoData::Parse(rec_info));
        });

    // AffectedByPowerOutageInfo.Create(init) → new
    // AffectedByPowerOutage(init.Self, this)
    registry.Register(
        "AffectedByPowerOutageInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AffectedByPowerOutage>(
              init, AffectedByPowerOutageInfoData::Parse(rec_info));
        });

    // ———— 第六批:资源链(world actor + 单位/厂)————

    // ResourceLayerInfo.Create(init) → new ResourceLayer(init.Self, this)
    registry.Register(
        "ResourceLayerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ResourceLayer>(
              init.Self(), ResourceLayerInfoData::Parse(rec_info));
        });

    // ResourceClaimLayerInfo.Create(init) → new ResourceClaimLayer()
    registry.Register(
        "ResourceClaimLayerInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ResourceClaimLayer>();
        });

    // DockClientManagerInfo.Create(init) → new DockClientManager(
    // init.Self, this)
    registry.Register(
        "DockClientManagerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<DockClientManager>(
              init, DockClientManagerInfoData::Parse(rec_info));
        });

    // DockHostInfo.Create(init) → new DockHost(init.Self, this)
    registry.Register(
        "DockHostInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<DockHost>(init,
                                        DockHostInfoData::Parse(rec_info));
        });

    // StoresResourcesInfo.Create(init) → new StoresResources(init.Self,
    // this)
    registry.Register(
        "StoresResourcesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<StoresResources>(
              init, StoresResourcesInfoData::Parse(rec_info));
        });

    // StoresPlayerResourcesInfo.Create(init) → new
    // StoresPlayerResources(init.Self, this)
    registry.Register(
        "StoresPlayerResourcesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<StoresPlayerResources>(
              init, StoresPlayerResourcesInfoData::Parse(rec_info));
        });

    // RefineryInfo.Create(init) → new Refinery(init.Self, this)
    // (Requires<WithSpriteBodyInfo> 的约束面:WithSpriteBody 未移植 →
    // 注册侧跳过 —— COVERAGE 登记)
    // (the Requires<WithSpriteBodyInfo> constraint face: WithSpriteBody is
    // unported → the registration-side skip — registered in COVERAGE).
    registry.Register(
        "RefineryInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Refinery>(
              init, RefineryInfoData::Parse(rec_info));
        });

    // HarvesterInfo.Create(init) → new Harvester(init.Self, this)
    // (IRulesetLoaded 的 Resources 校验 = 工厂时点)
    // (the IRulesetLoaded Resources validation = factory time).
    registry.Register(
        "HarvesterInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          HarvesterInfoData data = HarvesterInfoData::Parse(rec_info);
          if (init.Self().Info() != nullptr)
            data.ValidateResources(*init.Self().Info());
          return arena.Create<Harvester>(init, data);
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

// ———— 第五批 [VerifySync] 哈希注册(gen/sync_gen.cpp 成员表:
//      FrozenUnderFog {VisibilityHash} / Building {TopLeft} /
//      PlayerResources {Cash,Resources,ResourceCapacity} /
//      ProductionQueue {Enabled,IsValidFaction})————
// ———— The batch-5 [VerifySync] hash registrations (the gen/sync_gen.cpp
//      member tables: FrozenUnderFog {VisibilityHash} / Building
//      {TopLeft} / PlayerResources {Cash,Resources,ResourceCapacity} /
//      ProductionQueue {Enabled,IsValidFaction}) ————

namespace {

int FrozenUnderFogSyncHash(const sim::ISync* s) {
  const auto* frozen_under_fog = static_cast<const FrozenUnderFog*>(s);
  return sim::sync::CombineSyncHash(
      0, frozen_under_fog->VisibilityHash);
}

int BuildingSyncHash(const sim::ISync* s) {
  const auto* building = static_cast<const Building*>(s);
  return sim::sync::CombineSyncHash(
      0, sim::sync::HashCPos(building->TopLeft()));
}

int PlayerResourcesSyncHash(const sim::ISync* s) {
  const auto* player_resources = static_cast<const PlayerResources*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(
          sim::sync::CombineSyncHash(0, player_resources->Cash),
          player_resources->Resources),
      player_resources->ResourceCapacity);
}

int ProductionQueueSyncHash(const sim::ISync* s) {
  const auto* queue = static_cast<const ProductionQueue*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(0, queue->Enabled() ? 1 : 0),
      queue->IsValidFaction() ? 1 : 0);
}

const bool b_registered_batch5_sync_hash = [] {
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.FrozenUnderFog",
      &FrozenUnderFogSyncHash);
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Building",
                                &BuildingSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.PlayerResources",
      &PlayerResourcesSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.ProductionQueue",
      &ProductionQueueSyncHash);
  return true;
}();

}  // namespace

}  // namespace

}  // namespace ora::mods
