#include "meta/generic_record.hpp"
#include "mods/concrete_support_powers.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch13() {
  auto& registry = sim::TraitRegistry::Instance();
  using ActorInitializer = sim::ActorInitializer;
  using sim::TraitBase;

  registry.Register(
      "NukePowerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<NukePower>(
            init, NukePowerInfoData::Parse(rec_info, init.Self().world()));
      });

  registry.Register(
      "SpawnActorPowerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<SpawnActorPower>(
            init, SpawnActorPowerInfoData::Parse(rec_info));
      });

  registry.Register(
      "ProduceActorPowerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<ProduceActorPower>(
            init, ProduceActorPowerInfoData::Parse(rec_info));
      });

  registry.Register(
      "GrantExternalConditionPowerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<GrantExternalConditionPower>(
            init, GrantExternalConditionPowerInfoData::Parse(rec_info));
      });
}

}
