// UPSTREAM: OpenRA.Mods.Common/Traits/Air/Aircraft.cs + AmmoPool.cs +
//          Rearmable.cs + Repairable.cs + RepairsUnits.cs +
//          Air/FallsToEarth.cs + Air/AttackAircraft.cs + AttackBomber.cs +
//          SupportPowers/AirstrikePower.cs(注册装配)
#include "meta/generic_record.hpp"
#include "mods/air_support.hpp"
#include "mods/aircraft.hpp"
#include "mods/airstrike_power.hpp"
#include "mods/attack_aircraft.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch14() {
  auto& registry = sim::TraitRegistry::Instance();
  using ActorInitializer = sim::ActorInitializer;
  using sim::TraitBase;

  registry.Register(
      "AircraftInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<Aircraft>(init,
                                      AircraftInfoData::Parse(rec_info));
      });

  registry.Register(
      "AmmoPoolInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<AmmoPool>(AmmoPoolInfoData::Parse(rec_info));
      });

  registry.Register(
      "RearmableInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<Rearmable>(RearmableInfoData::Parse(rec_info));
      });

  registry.Register(
      "RepairsUnitsInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<RepairsUnits>(
            RepairsUnitsInfoData::Parse(rec_info));
      });

  registry.Register(
      "RepairableInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<Repairable>(
            init, RepairableInfoData::Parse(rec_info));
      });

  registry.Register(
      "FallsToEarthInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<FallsToEarth>(
            init, FallsToEarthInfoData::Parse(rec_info));
      });

  registry.Register(
      "AttackAircraftInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<AttackAircraft>(
            init, AttackAircraftInfoData::Parse(rec_info));
      });

  registry.Register(
      "AttackBomberInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<AttackBomber>(
            init, AttackBomberInfoData::Parse(rec_info));
      });

  registry.Register(
      "AirstrikePowerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<AirstrikePower>(
            init, AirstrikePowerInfoData::Parse(rec_info));
      });
}

}  // namespace ora::mods
