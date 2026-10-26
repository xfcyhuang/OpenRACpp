// UPSTREAM: OpenRA.Mods.Common/Traits/CustomSellValue.cs + Sellable.cs +
//          RepairableNear.cs + RepairableBuilding.cs @1f594e320
//          (注册装配)
#include "meta/generic_record.hpp"
#include "mods/repairable_building.hpp"
#include "mods/repairable_near.hpp"
#include "mods/sellable.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch17() {
  auto& registry = sim::TraitRegistry::Instance();

  registry.Register(
      "CustomSellValueInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer&,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<CustomSellValue>(
            CustomSellValueInfoData::Parse(rec_info));
      });

  registry.Register(
      "SellableInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<Sellable>(init.Self(),
                                      SellableInfoData::Parse(rec_info));
      });

  registry.Register(
      "RepairableNearInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<RepairableNear>(init.Self(),
                                            RepairableNearInfoData::Parse(rec_info));
      });

  registry.Register(
      "RepairableBuildingInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<RepairableBuilding>(
            init.Self(), RepairableBuildingInfoData::Parse(rec_info));
      });
}

}  // namespace ora::mods
