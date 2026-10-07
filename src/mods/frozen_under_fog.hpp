// UPSTREAM: OpenRA.Mods.Common/Traits/Modifiers/FrozenUnderFog.cs @b6fc03f
//          L17-190 全文 + HiddenUnderShroud.cs L15-72 全文 + FrozenUnderFog.cs
//          L189-190 的 HiddenUnderFogInit(SpawnedByMapInit 随标记面入
//          actor_init.hpp)
//          The whole of FrozenUnderFog.cs L17-190 + the whole of
//          HiddenUnderShroud.cs L15-72 + HiddenUnderFogInit of L189-190
//          (SpawnedByMapInit's marker face lives in actor_init.hpp).
//
// 机制对照 / Mechanism mapping:
//  - 上游 FrozenUnderFog = ICreatesFrozenActors + IRenderModifier +
//    IDefaultVisibility + ITickRender + ISync + INotify{Created,
//    OwnerChanged,ActorDisposing}:C++ 挂同步面四接口;IRenderModifier/
//    ITickRender 的渲染物化(renderables/bounds/mouseBounds)随渲染批 ——
//    TickRender 保留 NeedRenderables/ScreenMap.AddOrUpdateFrozen 的结构
//    面,渲染物化空(登记 COVERAGE)
//    Upstream's FrozenUnderFog = ICreatesFrozenActors + IRenderModifier +
//    IDefaultVisibility + ITickRender + ISync + INotify{Created,
//    OwnerChanged,ActorDisposing}: the C++ side mounts the four
//    synchronous interfaces; the IRenderModifier/ITickRender render
//    materialization (renderables/bounds/mouseBounds) rides the render
//    batch — TickRender keeps the NeedRenderables/ScreenMap.
//    AddOrUpdateFrozen structural face with an empty render materialization
//    (registered in COVERAGE).
//  - PlayerDictionary<FrozenState> → 玩家序 vector(World.Players() 的
//    IndexOf;上游按 playerIndex 索引同构)
//    PlayerDictionary<FrozenState> → the player-indexed vector
//    (World.Players()' IndexOf; isomorphic to upstream's playerIndex
//    indexing).
//  - startsRevealed 的 ShroudInfo 读:上游 Map.Rules.Actors[Player].
//    TraitInfo<ShroudInfo>() + LobbyInfo OptionOrDefault("explored",…)
//    —— C++ 沿 Shroud::Created 的同形读(玩家 Shroud 的 checkbox 面)
//    The startsRevealed ShroudInfo read: upstream's Map.Rules.Actors[
//    Player].TraitInfo<ShroudInfo>() + LobbyInfo OptionOrDefault(
//    "explored",…) — C++ keeps Shroud::Created's same-shaped read (the
//    player Shroud's checkbox face).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "mods/affects_shroud.hpp"  // VisibilityType(上游同名枚举共用)
                                       // the upstream same-name enum shared.
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
}

namespace ora::mods {

/// FrozenUnderFogInfo(L21-31)
struct FrozenUnderFogInfoData {
  sim::PlayerRelationship always_visible_relationships =
      sim::PlayerRelationship::Ally;  // L26

  static FrozenUnderFogInfoData Parse(const meta::RecordObject& rec_info);
};

/// FrozenUnderFog(L35-188)
class FrozenUnderFog final : public sim::TraitBase,
                             public sim::ICreatesFrozenActors,
                             public sim::IDefaultVisibility,
                             public sim::ITickRender,
                             public sim::ISync,
                             public sim::INotifyCreated,
                             public sim::INotifyOwnerChanged,
                             public sim::INotifyActorDisposing {
 public:
  ORA_TRAIT_INTERFACES(
      FrozenUnderFog, OpenRA_Mods_Common_Traits_FrozenUnderFog,
      sim::ICreatesFrozenActors, sim::IDefaultVisibility, sim::ITickRender,
      sim::ISync, sim::INotifyCreated, sim::INotifyOwnerChanged,
      sim::INotifyActorDisposing)

  FrozenUnderFog(sim::ActorInitializer& init,
                 FrozenUnderFogInfoData info);

  void Created(sim::Actor& self) override;  // L57-80

  /// L82-99:ICreatesFrozenActors.OnVisibilityChanged
  void OnVisibilityChanged(sim::FrozenActor& frozen) override;

  /// L114-121:IDefaultVisibility.IsVisible
  bool IsVisible(sim::Actor& self, sim::Player* by_player) override;

  /// L104-112:ITickRender.TickRender(结构面;渲染物化空)
  /// L104-112: ITickRender.TickRender (the structural face; the render
  /// materialization is empty).
  void TickRender(sim::Actor& self) override;

  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;  // L134-141

  void Disposing(sim::Actor& self) override;  // L143-148

  int VisibilityHash = 0;  // [VerifySync] L39

 private:
  /// L101-102:UpdateFrozenActor
  /// L101-102: UpdateFrozenActor.
  void UpdateFrozenActor(sim::FrozenActor& frozen_actor, int player_index);

  /// L106-109:IsVisibleInner
  /// L106-109: IsVisibleInner.
  bool IsVisibleInner(sim::Player& by_player);

  struct FrozenState {
    sim::FrozenActor* frozen_actor = nullptr;
    bool b_is_visible = false;
  };

  FrozenUnderFogInfoData info_;
  bool b_starts_revealed_ = false;
  std::vector<PPos> vec_footprint_;
  std::vector<FrozenState> vec_frozen_states_;  // 玩家序 | player order.
  bool b_created_ = false;
};

/// HiddenUnderShroudInfo(L21-37)
struct HiddenUnderShroudInfoData {
  sim::PlayerRelationship always_visible_relationships =
      sim::PlayerRelationship::Ally;  // L24
  VisibilityType type = VisibilityType::Footprint;  // L29

  static HiddenUnderShroudInfoData Parse(const meta::RecordObject& rec_info);
};

/// HiddenUnderShroud(L39-72)
class HiddenUnderShroud : public sim::TraitBase,
                          public sim::IDefaultVisibility {
 public:
  ORA_TRAIT_INTERFACES(
      HiddenUnderShroud, OpenRA_Mods_Common_Traits_HiddenUnderShroud,
      sim::IDefaultVisibility)

  explicit HiddenUnderShroud(HiddenUnderShroudInfoData info)
      : info_{std::move(info)} {}

  /// L46-56:protected IsVisibleInner(子类覆写面;HiddenUnderFog 等)
  /// L46-56: the protected IsVisibleInner (the subclass override face;
  /// HiddenUnderFog etc.).
  virtual bool IsVisibleInner(sim::Actor& self, sim::Player& by_player);

  /// L58-66:IDefaultVisibility.IsVisible
  bool IsVisible(sim::Actor& self, sim::Player* by_player) override;

  const HiddenUnderShroudInfoData& Info() const { return info_; }

 protected:
  HiddenUnderShroudInfoData info_;
};

}  // namespace ora::mods
