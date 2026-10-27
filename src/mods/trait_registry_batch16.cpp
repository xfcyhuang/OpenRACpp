#include "meta/generic_record.hpp"
#include "mods/carryall.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch16() {
  auto& registry = sim::TraitRegistry::Instance();

  registry.Register(
      "CarryableInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<Carryable>(CarryableInfoData::Parse(rec_info));
      });

  registry.Register(
      "CarryallInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<Carryall>(init,
                                      CarryallInfoData::Parse(rec_info));
      });

  registry.Register(
      "AutoCarryableInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<AutoCarryable>(
            AutoCarryableInfoData::Parse(rec_info));
      });

  registry.Register(
      "AutoCarryallInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<AutoCarryall>(
            init, AutoCarryallInfoData::Parse(rec_info));
      });

  registry.Register(
      "CarryableHarvesterInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<CarryableHarvester>(
            CarryableHarvesterInfoData::Parse(rec_info));
      });
}

}
