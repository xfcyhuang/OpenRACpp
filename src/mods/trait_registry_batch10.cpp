#include "meta/generic_record.hpp"
#include "mods/bulk_production_queue.hpp"
#include "mods/footprint_place_building_preview.hpp"
#include "mods/line_build.hpp"
#include "mods/place_building.hpp"
#include "mods/plug.hpp"
#include "mods/primary_building.hpp"
#include "mods/production_icon_overlay.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

namespace {

std::string InstanceNameOf(const sim::ActorInitializer& init,
                           const meta::RecordObject& rec_info) {
  if (init.Self().Info() == nullptr)
    return {};
  return std::string{init.Self().Info()->InstanceNameOf(&rec_info)};
}

}

void RegisterCommonTraitsBatch10() {
  auto& registry = sim::TraitRegistry::Instance();
  using sim::TraitBase;
  using ActorInitializer = sim::ActorInitializer;
  using ora::WorldArena;

  registry.Register(
      "PlaceBuildingInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<PlaceBuilding>(
            init, PlaceBuildingInfoData::Parse(rec_info));
      });

  registry.Register(
      "PlaceBuildingVariantsInfo",
      [](const meta::RecordObject&, ActorInitializer&,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<PlaceBuildingVariants>();
      });

  registry.Register(
      "ReplacementInfo",
      [](const meta::RecordObject&, ActorInitializer&,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<Replacement>();
      });

  registry.Register(
      "ReplaceableInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<Replaceable>(
            init, ReplaceableInfoData::Parse(rec_info));
      });

  registry.Register(
      "LineBuildInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<LineBuild>(init, LineBuildInfoData::Parse(rec_info));
      });

  registry.Register(
      "LineBuildNodeInfo",
      [](const meta::RecordObject&, ActorInitializer&,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<LineBuildNode>();
      });

  registry.Register(
      "PlugInfo",
      [](const meta::RecordObject&, ActorInitializer&,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<Plug>();
      });

  registry.Register(
      "PluggableInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<Pluggable>(
            init, PluggableInfoData::Parse(rec_info),
            InstanceNameOf(init, rec_info));
      });

  registry.Register(
      "PrimaryBuildingInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<PrimaryBuilding>(
            init, PrimaryBuildingInfoData::Parse(rec_info));
      });

  registry.Register(
      "BulkProductionQueueInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<BulkProductionQueue>(
            init, BulkProductionQueueInfoData::Parse(rec_info));
      });

  registry.Register(
      "ProductionIconOverlayManagerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<ProductionIconOverlayManager>(
            init, ProductionIconOverlayManagerInfoData::Parse(rec_info));
      });

  registry.Register(
      "WithProductionIconOverlayInfo",
      [](const meta::RecordObject&, ActorInitializer&,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<WithProductionIconOverlay>();
      });

  registry.Register(
      "FootprintPlaceBuildingPreviewInfo",
      [](const meta::RecordObject&, ActorInitializer&,
         WorldArena& arena) -> TraitBase* {
        return arena.Create<FootprintPlaceBuildingPreviewRuntime>();
      });
}

}
