// UPSTREAM: OpenRA.Mods.Common/Traits/Render/RenderSprites.cs @b6fc03f(第八批
//          mods trait 的注册表装配:RenderSprites/WithSpriteBody/
//          WithFacingSpriteBody/WithInfantryBody/WithSpriteTurret/
//          WithMakeAnimation/WithMakeOverlay + ProximityCapturable 族;
//          与 RegisterCommonTraits 同形 —— Info 名 → 工厂 → WorldArena)
//          The batch-8 mods-trait registry assembly: RenderSprites/
//          WithSpriteBody/WithFacingSpriteBody/WithInfantryBody/
//          WithSpriteTurret/WithMakeAnimation/WithMakeOverlay + the
//          ProximityCapturable family; the same shape as
//          RegisterCommonTraits — the Info name → factory → WorldArena.

#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "mods/proximity_capturable.hpp"
#include "mods/render_sprites.hpp"
#include "mods/with_infantry_body.hpp"
#include "mods/with_make_animation.hpp"
#include "mods/with_sprite_body.hpp"
#include "mods/with_sprite_turret.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

void RegisterCommonTraitsBatch8() {
  auto& registry = sim::TraitRegistry::Instance();
  using sim::TraitBase;
  using ActorInitializer = sim::ActorInitializer;
  using ora::WorldArena;

  registry.Register(
      "RenderSpritesInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        RenderSpritesInfoData data = RenderSpritesInfoData::Parse(rec_info);
        return arena.Create<RenderSprites>(init, std::move(data));
      });

  registry.Register(
      "RenderSpritesEditorOnlyInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        RenderSpritesInfoData data = RenderSpritesInfoData::Parse(rec_info);
        return arena.Create<RenderSpritesEditorOnly>(init, std::move(data));
      });

  registry.Register(
      "WithSpriteBodyInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithSpriteBodyInfoData data = WithSpriteBodyInfoData::Parse(rec_info);
        return arena.Create<WithSpriteBody>(init, std::move(data));
      });

  registry.Register(
      "WithFacingSpriteBodyInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithSpriteBodyInfoData data = WithSpriteBodyInfoData::Parse(rec_info);
        return arena.Create<WithFacingSpriteBody>(init, std::move(data));
      });

  registry.Register(
      "WithInfantryBodyInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithInfantryBodyInfoData data =
            WithInfantryBodyInfoData::Parse(rec_info);
        return arena.Create<WithInfantryBody>(init, std::move(data));
      });

  registry.Register(
      "WithSpriteTurretInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithSpriteTurretInfoData data =
            WithSpriteTurretInfoData::Parse(rec_info);
        return arena.Create<WithSpriteTurret>(init.Self(), std::move(data));
      });

  registry.Register(
      "WithMakeAnimationInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WithMakeAnimationInfoData data =
            WithMakeAnimationInfoData::Parse(rec_info);
        return arena.Create<WithMakeAnimation>(init, std::move(data));
      });

  registry.Register(
      "WithMakeOverlayInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        const auto str_sequence =
            sim::RecordFieldString(rec_info, "Sequence");
        const auto str_palette = sim::RecordFieldString(rec_info, "Palette");
        const auto v_is_player =
            sim::RecordFieldInt(rec_info, "IsPlayerPalette");
        return arena.Create<WithMakeOverlay>(
            init.Self(), str_sequence ? std::string{*str_sequence} : std::string{},
            str_palette ? std::string{*str_palette} : std::string{},
            v_is_player.has_value() && *v_is_player != 0);
      });

  registry.Register(
      "ProximityCaptorInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer&,
         WorldArena& arena) -> TraitBase* {
        ProximityCaptorInfoData data = ProximityCaptorInfoData::Parse(rec_info);
        return arena.Create<ProximityCaptor>(std::move(data));
      });

  registry.Register(
      "ProximityCapturableInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        ProximityCapturableBaseInfoData data_base =
            ProximityCapturableBaseInfoData::Parse(rec_info);
        WDist dist_range = WDist::FromCells(5);  // ProximityCapturableInfo L21
        if (const auto* gv = sim::RecordFieldValue(rec_info, "Range"))
          if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
            if (tuple->uint1_count >= 1)
              dist_range = WDist{static_cast<int>(tuple->arr_ints[0])};
        return arena.Create<ProximityCapturable>(init, std::move(data_base),
                                                 dist_range);
      });
}

}  // namespace ora::mods
