// UPSTREAM: OpenRA.Mods.Common/Traits/World/CreateMapPlayers.cs @b6fc03f
//          L22-181 + OpenRA.Game/Traits/World/Faction.cs(逐语义重写)
//          Verbatim-semantics rewrites.
//
// 机制对照 / Mechanism mapping:
//  - CreateMapPlayersInfo.CreateServerPlayers(L30-82):服务器 GameInformation
//    面 —— Phase 7(与 ICreatePlayersInfo 接口面一并;COVERAGE 登记)
//    CreateMapPlayersInfo.CreateServerPlayers (L30-82): the server
//    GameInformation face — Phase 7 (together with the ICreatePlayersInfo
//    interface face; in COVERAGE).
//  - SetupPlayerMasks 的 "HACK: Map players share a ClientID"(L159-161)
//    注释语义照抄 | the SetupPlayerMasks HACK-comment semantics kept.
#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ICreatePlayers;
using ora::MersenneTwister;
using sim::Player;
using sim::TraitBase;
using sim::World;

/// Faction(Faction.cs L40:"we're only interested in the Info")
/// Faction (Faction.cs L40: "we're only interested in the Info").
class Faction final : public TraitBase {
 public:
  // 无接口面(MakeTraitUpcasts 单参形态;ORA_TRAIT_INTERFACES 的空可变参
  // 形态不受宏支持 —— 手写展开)
  // No interface face (the single-argument MakeTraitUpcasts form; the
  // macro's empty-variadic form is unsupported — hand-expanded).
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_Faction;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Faction>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

/// CreateMapPlayers(CreateMapPlayers.cs L85) | CreateMapPlayers
/// (CreateMapPlayers.cs L85).
class CreateMapPlayers final : public TraitBase, public ICreatePlayers {
 public:
  ORA_TRAIT_INTERFACES(CreateMapPlayers,
                       OpenRA_Mods_Common_Traits_CreateMapPlayers,
                       ICreatePlayers)

  void CreatePlayers(World& w, MersenneTwister& player_random) override;  // L87-140

 private:
  /// SetupPlayerMasks(L142-174) | SetupPlayerMasks (L142-174).
  static void SetupPlayerMasks(Player* p, Player* q);
};

/// mods 侧 trait 的注册入口(与 sim::RegisterWorldTraits 同形;测试/引擎
/// 装配各调用一次)
/// The mods-side trait registration entry (the same shape as
/// sim::RegisterWorldTraits; each assembly entry — tests/engine — calls
/// once).
void RegisterCommonTraits();

/// 第七批 trait 的注册装配(conditions/cloak/experience/capture/selectable/
/// spawn_map_actors;独立翻译单元 —— RegisterCommonTraits 内调用)
/// The batch-7 trait registry assembly (conditions/cloak/experience/
/// capture/selectable/spawn_map_actors; a separate translation unit —
/// called from within RegisterCommonTraits).
void RegisterCommonTraitsBatch7();

/// 第八批 trait 的注册装配(render_sprites/with_sprite_body 族/
/// with_infantry_body/with_sprite_turret/with_make_animation/
/// with_make_overlay/proximity_capturable;独立翻译单元 ——
/// RegisterCommonTraits 内调用)
/// The batch-8 trait registry assembly (render_sprites/the
/// with_sprite_body family/with_infantry_body/with_sprite_turret/
/// with_make_animation/with_make_overlay/proximity_capturable; a separate
/// translation unit — called from within RegisterCommonTraits).
void RegisterCommonTraitsBatch8();

/// 第九批 trait 的注册装配(selection_decorations/with_decoration/
/// with_death_animation/with_damage_overlay/production_bar/pips 装饰;
/// 独立翻译单元 —— RegisterCommonTraits 内调用)
/// The batch-9 trait registry assembly (selection_decorations/
/// with_decoration/with_death_animation/with_damage_overlay/
/// production_bar/the pip decorations; a separate translation unit —
/// called from within RegisterCommonTraits).
void RegisterCommonTraitsBatch9();

}  // namespace ora::mods
