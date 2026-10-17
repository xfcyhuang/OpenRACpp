// UPSTREAM: OpenRA.Mods.Common/Traits 第十一批 mods trait 注册表装配
//          (RadarPings/PlayerRadarTerrain/AppearsOnRadar/ProvidesRadar/
//          RadarColorFromTerrain/SupportPowerManager/Tooltip/
//          PlayerStatistics/UpdatesPlayerStatistics;Info 名 → 工厂 →
//          WorldArena)
//          The batch-11 registry assembly (Info name → factory →
//          WorldArena).
#include "meta/generic_record.hpp"
#include "mods/player_statistics.hpp"
#include "mods/radar.hpp"
#include "mods/support_powers.hpp"
#include "mods/tooltip.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch11() {
  auto& registry = sim::TraitRegistry::Instance();
  using sim::TraitBase;
  using ActorInitializer = sim::ActorInitializer;
  using ora::WorldArena;

  registry.Register(
      "RadarPingsInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<RadarPings>(RadarPingsInfoData::Parse(rec_info));
      });

  registry.Register(
      "PlayerRadarTerrainInfo",
      [](const meta::RecordObject&, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<PlayerRadarTerrain>(init.Self());
      });

  registry.Register(
      "AppearsOnRadarInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<AppearsOnRadar>(
            init, AppearsOnRadarInfoData::Parse(rec_info));
      });

  registry.Register(
      "ProvidesRadarInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<ProvidesRadar>(
            init, ProvidesRadarInfoData::Parse(rec_info));
      });

  registry.Register(
      "RadarColorFromTerrainInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<RadarColorFromTerrain>(
            init.Self(), RadarColorFromTerrainInfoData::Parse(rec_info));
      });

  registry.Register(
      "SupportPowerManagerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<SupportPowerManager>(init);
      });

  registry.Register(
      "TooltipInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<Tooltip>(init, TooltipInfoData::Parse(rec_info));
      });

  registry.Register(
      "PlayerStatisticsInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<PlayerStatistics>(init);
      });

  registry.Register(
      "UpdatesPlayerStatisticsInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<UpdatesPlayerStatistics>(
            init, UpdatesPlayerStatisticsInfoData::Parse(rec_info));
      });
}

}  // namespace ora::mods
