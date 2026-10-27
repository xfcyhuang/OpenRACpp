import std;

#include "mods/actor_preview_faces.hpp"
#include "mods/create_map_players.hpp"

#include "mods/hotkey_logics.hpp"
#include "mods/load_widget_at_game_start.hpp"
#include "mods/local_profile_logics.hpp"
#include "mods/viewport_controller.hpp"
#include "mods/production_palette_widget.hpp"
#include "mods/radar_widget.hpp"
#include "mods/support_powers_widget.hpp"
#include "mods/support_power_timer_widget.hpp"
#include "mods/observer_widgets.hpp"
#include "mods/video_player_widget.hpp"
#include "mods/ingame_hud_logics.hpp"
#include "mods/production_tabs_widget.hpp"
#include "mods/production_type_button_widget.hpp"
#include "mods/world_interaction_controller.hpp"
#include "mods/trait_registry_batch17.cpp"
#include "mods/ingame_menu_logic.hpp"
#include "mods/main_menu_logic.hpp"
#include "mods/asset_browser_logic.hpp"
#include "mods/browser_logics.hpp"
#include "mods/connection_logics.hpp"
#include "mods/lobby_logics.hpp"
#include "mods/game_info_logics.hpp"
#include "mods/lobby_utils.hpp"
#include "mods/server_creation_logic.hpp"
#include "mods/server_list_logic.hpp"
#include "mods/settings_logics.hpp"
#include "mods/menu_logics.hpp"

#include "meta/field_loader.hpp"

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
#include "mods/conditions.hpp"
#include "mods/external_condition.hpp"
#include "mods/cloak.hpp"
#include "mods/experience.hpp"
#include "mods/capture.hpp"
#include "mods/selectable.hpp"
#include "mods/spawn_map_actors.hpp"
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

void CreateMapPlayers::CreatePlayers(World& w, MersenneTwister& player_random) {
  const map::MapPlayers map_players{w.Map().PlayerDefinitions(),
                                    core::Color::FromArgbRaw(0xFF4B4B4B)};
  const auto& players = map_players.Players;

  std::vector<Player*> world_players;
  bool world_owner_found = false;

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

  for (const auto& [slot_key, slot] : w.LobbyInfo().vec_slots) {
    const net::SessionClient* client = w.LobbyInfo().ClientInSlot(slot_key);
    if (client == nullptr)
      continue;

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

  if (p->PlayerReference().Playable && q->PlayerReference().Playable) {

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

void InstallBitSetAllocatorBridges() {
  auto install = []<class TagT>(const char* ptr_tag) {
    core::BitSetAllocator<TagT>::InstallBridge(
        [str_tag = std::string{ptr_tag}](
            std::span<const std::string> vec_values) {
          return meta::BitsOf(str_tag, vec_values);
        },
        [str_tag = std::string{ptr_tag}](std::uint64_t uint8_bits) {
          return meta::StringsOfBits(str_tag, uint8_bits);
        },
        [str_tag = std::string{ptr_tag}](std::uint64_t uint8_bits,
                                         std::string_view str_value) {
          return meta::BitSetContainsString(str_tag, uint8_bits,
                                            str_value);
        },
        [str_tag = std::string{ptr_tag}](
            std::span<const std::string> vec_values) {
          return meta::BitSetBitsOfNoAlloc(str_tag, vec_values);
        });
  };
  install.operator()<sim::TargetableType>("OpenRA.Traits.TargetableType");
  install.operator()<sim::DamageType>("OpenRA.Traits.DamageType");
  install.operator()<sim::CrushClass>("OpenRA.Traits.CrushClass");
  install.operator()<sim::DockType>(
      "OpenRA.Mods.Common.Traits.DockType");
  install.operator()<sim::CaptureType>(
      "OpenRA.Mods.Common.Traits.CaptureType");
  install.operator()<ArmorType>(
      "OpenRA.Mods.Common.Traits.ArmorType");
  install.operator()<DetectionType>(
      "OpenRA.Mods.Common.Traits.DetectionType");
}

void RegisterCommonTraits() {
  InstallBitSetAllocatorBridges();
  RegisterActorPreviewFaces();
  RegisterWorldInteractionControllerWidget();
  RegisterViewportControllerWidget();
  RegisterProductionPaletteWidget();
  RegisterProductionTabsWidget();
  RegisterProductionTypeButtonWidget();
  RegisterRadarWidget();
  RegisterSupportPowersWidget();
  RegisterSupportPowerTimerWidget();
  RegisterObserverWidgets();
  RegisterVideoPlayerWidget();
  RegisterIngameHudLogics();
  static const bool b_registered = [] {
    sim::TraitRegistry& registry = sim::TraitRegistry::Instance();

    registry.Register(
        "FactionInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Faction>();
        });

    registry.Register(
        "HealthInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {

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

    registry.Register(
        "ActorMapInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const std::int64_t bin_size =
              sim::RecordFieldInt(rec_info, "BinSize").value_or(10);
          return arena.Create<sim::ActorMap>(init.Self().world(),
                                             static_cast<int>(bin_size));
        });

    registry.Register(
        "ControlGroupsInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          std::vector<std::string> groups{"1", "2", "3", "4", "5",
                                          "6", "7", "8", "9", "0"};

          (void)rec_info;
          return arena.Create<sim::ControlGroups>(init.Self().world(),
                                                  std::move(groups));
        });

    registry.Register(
        "CreateMapPlayersInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<CreateMapPlayers>();
        });

    registry.Register(
        "LocomotorInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          pathfinding::LocomotorInfo info = pathfinding::ParseLocomotorInfo(rec_info);
          return arena.Create<pathfinding::Locomotor>(init.Self(),
                                                     std::move(info));
        });

    registry.Register(
        "PathFinderInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const std::int64_t weight = sim::RecordFieldInt(
              rec_info, "HeuristicWeightPercentage").value_or(125);
          return arena.Create<pathfinding::PathFinder>(
              init.Self(), static_cast<int>(weight));
        });

    sim::RegisterOrderGenerator(
        "UnitOrderGenerator",
        [](World& world) -> std::unique_ptr<sim::IOrderGenerator> {
          return std::make_unique<UnitOrderGenerator>(world);
        });

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

    registry.Register(
        "BodyOrientationInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const BodyOrientationInfoData data =
              BodyOrientationInfoData::Parse(rec_info);
          return arena.Create<BodyOrientation>(init, data);
        });

    registry.Register(
        "ArmamentInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const ArmamentInfoData data =
              ArmamentInfoData::Parse(rec_info, init.Self().world());
          return arena.Create<Armament>(init, std::move(data));
        });

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

    registry.Register(
        "HitShapeInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<HitShape>(
              init, HitShapeInfoData::Parse(rec_info));
        });

    registry.Register(
        "ArmorInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Armor>(ArmorInfoData::Parse(rec_info));
        });

    registry.Register(
        "RevealsShroudInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<RevealsShroud>(
              RevealsShroudInfoData::Parse(rec_info));
        });

    registry.Register(
        "AttackFrontalInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AttackFrontal>(
              init, AttackBaseInfoData::Parse(rec_info));
        });

    registry.Register(
        "AutoTargetInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AutoTarget>(
              init, AutoTargetInfoData::Parse(rec_info));
        });

    registry.Register(
        "AutoTargetPriorityInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AutoTargetPriority>(
              AutoTargetPriorityInfoData::Parse(rec_info));
        });

    registry.Register(
        "TargetableInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Targetable>(
              TargetableInfoData::Parse(rec_info));
        });

    registry.Register(
        "BlocksProjectilesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<BlocksProjectiles>(
              BlocksProjectilesInfoData::Parse(rec_info));
        });

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

    registry.Register(
        "FrozenUnderFogInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<FrozenUnderFog>(
              init, FrozenUnderFogInfoData::Parse(rec_info));
        });

    registry.Register(
        "HiddenUnderShroudInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<HiddenUnderShroud>(
              HiddenUnderShroudInfoData::Parse(rec_info));
        });

    registry.Register(
        "SmudgeLayerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<SmudgeLayer>(
              init.Self(), SmudgeLayerInfoData::Parse(rec_info));
        });

    registry.Register(
        "BuildingInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Building>(
              init, BuildingInfoData::Parse(rec_info));
        });

    registry.Register(
        "BuildableInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Buildable>();
        });

    registry.Register(
        "ValuedInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Valued>();
        });

    registry.Register(
        "ExitInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Exit>(ExitInfoData::Parse(rec_info));
        });

    registry.Register(
        "ReservableInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Reservable>();
        });

    registry.Register(
        "RallyPointInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<RallyPoint>(
              init.Self(), RallyPointInfoData::Parse(rec_info));
        });

    registry.Register(
        "ProvidesPrerequisiteInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ProvidesPrerequisite>(
              init, ProvidesPrerequisiteInfoData::Parse(rec_info));
        });

    registry.Register(
        "TechTreeInfo",
        [](const meta::RecordObject&, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<TechTree>(init);
        });

    registry.Register(
        "DeveloperModeInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<DeveloperMode>();
        });

    registry.Register(
        "ProductionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Production>(
              init, ProductionInfoData::Parse(rec_info));
        });

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

    registry.Register(
        "PlayerResourcesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<PlayerResources>(
              init, PlayerResourcesInfoData::Parse(rec_info));
        });

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

    registry.Register(
        "BuildingInfluenceInfo",
        [](const meta::RecordObject&, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<BuildingInfluence>(init.Self().world());
        });

    registry.Register(
        "ClassicFacingBodyOrientationInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<BodyOrientation>(
              init, BodyOrientationInfoData::Parse(rec_info));
        });

    registry.Register(
        "AttackFollowInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AttackFollow>(
              init, AttackFollowInfoData::Parse(rec_info));
        });

    registry.Register(
        "AttackTurretedInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AttackTurreted>(
              init, AttackTurretedInfoData::Parse(rec_info));
        });

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

    registry.Register(
        "PowerManagerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<PowerManager>(
              init, PowerManagerInfoData::Parse(rec_info));
        });

    registry.Register(
        "PowerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Power>(init, PowerInfoData::Parse(rec_info));
        });

    registry.Register(
        "AffectedByPowerOutageInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<AffectedByPowerOutage>(
              init, AffectedByPowerOutageInfoData::Parse(rec_info));
        });

    registry.Register(
        "ResourceLayerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ResourceLayer>(
              init.Self(), ResourceLayerInfoData::Parse(rec_info));
        });

    registry.Register(
        "ResourceClaimLayerInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ResourceClaimLayer>();
        });

    registry.Register(
        "DockClientManagerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<DockClientManager>(
              init, DockClientManagerInfoData::Parse(rec_info));
        });

    registry.Register(
        "DockHostInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<DockHost>(init,
                                        DockHostInfoData::Parse(rec_info));
        });

    registry.Register(
        "StoresResourcesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<StoresResources>(
              init, StoresResourcesInfoData::Parse(rec_info));
        });

    registry.Register(
        "StoresPlayerResourcesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<StoresPlayerResources>(
              init, StoresPlayerResourcesInfoData::Parse(rec_info));
        });

    registry.Register(
        "RefineryInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Refinery>(
              init, RefineryInfoData::Parse(rec_info));
        });

    registry.Register(
        "HarvesterInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          HarvesterInfoData data = HarvesterInfoData::Parse(rec_info);
          if (init.Self().Info() != nullptr)
            data.ValidateResources(*init.Self().Info());
          return arena.Create<Harvester>(init, data);
        });

    RegisterCommonTraitsBatch7();

    RegisterCommonTraitsBatch8();

    RegisterCommonTraitsBatch9();
    RegisterCommonTraitsBatch10();
    RegisterCommonTraitsBatch11();
    RegisterCommonTraitsBatch12();
    RegisterCommonTraitsBatch12Objectives();
    RegisterCommonTraitsBatch12Seeds();
    RegisterCommonTraitsBatch13();
    RegisterCommonTraitsBatch14();
    RegisterCommonTraitsBatch15();
    RegisterCommonTraitsBatch16();
    RegisterCommonTraitsBatch17();
    RegisterCommonTraitsBatch18();
    RegisterLoadWidgetAtGameStart();
    RegisterHotkeyLogics();
    RegisterLocalProfileLogics();
    RegisterMenuLogics();
    RegisterIngameMenuLogic();
    RegisterMainMenuLogic();
    RegisterBrowserLogics();
    RegisterConnectionLogics();
    RegisterServerListLogics();
    RegisterGameInfoLogics();
    RegisterServerCreationLogic();
    RegisterAssetBrowserLogic();
    RegisterLobbyLogics();
    RegisterSettingsLogics();

    return true;
  }();
  (void)b_registered;
}

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

  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.ClassicProductionQueue",
      &ProductionQueueSyncHash);
  return true;
}();

}

}

}
