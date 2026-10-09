// UPSTREAM: OpenRA.Mods.Common/Traits/Render/SelectionDecorations.cs @b6fc03f
//          第九批 mods trait 注册表装配(SelectionDecorations/
//          WithDecoration/WithDeathAnimation/WithDamageOverlay/
//          ProductionBar + pips 装饰;Info 名 → 工厂 → WorldArena)。
//          The batch-9 registry assembly (Info name → factory → WorldArena).

#include "meta/generic_record.hpp"
#include "mods/production_bar.hpp"
#include "mods/selection_decorations.hpp"
#include "mods/with_damage_overlay.hpp"
#include "mods/with_death_animation.hpp"
#include "mods/with_decoration.hpp"
#include "mods/with_decoration_pips.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch9() {
  auto& registry = sim::TraitRegistry::Instance();
  using sim::TraitBase;
  using ActorInitializer = sim::ActorInitializer;
  using ora::WorldArena;

  registry.Register(
      "SelectionDecorationsInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        SelectionDecorationsBaseInfoData data =
            SelectionDecorationsBaseInfoData::Parse(rec_info);
        return arena.Create<SelectionDecorations>(init, std::move(data));
      });

  registry.Register(
      "WithDecorationInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithDecorationInfoData data = WithDecorationInfoData::Parse(rec_info);
        return arena.Create<WithDecoration>(init, std::move(data));
      });

  registry.Register(
      "WithDeathAnimationInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithDeathAnimationInfoData data =
            WithDeathAnimationInfoData::Parse(rec_info);
        return arena.Create<WithDeathAnimation>(init, std::move(data));
      });

  registry.Register(
      "WithDamageOverlayInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithDamageOverlayInfoData data =
            WithDamageOverlayInfoData::Parse(rec_info);
        return arena.Create<WithDamageOverlay>(init, std::move(data));
      });

  registry.Register(
      "ProductionBarInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        ProductionBarInfoData data = ProductionBarInfoData::Parse(rec_info);
        return arena.Create<ProductionBar>(init, std::move(data));
      });

  registry.Register(
      "WithSpriteControlGroupDecorationInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithSpriteControlGroupDecorationInfoData data =
            WithSpriteControlGroupDecorationInfoData::Parse(rec_info);
        return arena.Create<WithSpriteControlGroupDecoration>(init,
                                                              std::move(data));
      });

  registry.Register(
      "WithResourceStoragePipsDecorationInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithResourceStoragePipsDecorationInfoData data =
            WithResourceStoragePipsDecorationInfoData::Parse(rec_info);
        return arena.Create<WithResourceStoragePipsDecoration>(init,
                                                               std::move(data));
      });

  registry.Register(
      "WithStoresResourcesPipsDecorationInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithStoresResourcesPipsDecorationInfoData data =
            WithStoresResourcesPipsDecorationInfoData::Parse(rec_info);
        return arena.Create<WithStoresResourcesPipsDecoration>(init,
                                                               std::move(data));
      });
}

}  // namespace ora::mods
