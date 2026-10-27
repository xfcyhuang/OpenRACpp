#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "mods/affects_shroud.hpp"

#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
}

namespace ora::mods {

struct FrozenUnderFogInfoData {
  sim::PlayerRelationship always_visible_relationships =
      sim::PlayerRelationship::Ally;

  static FrozenUnderFogInfoData Parse(const meta::RecordObject& rec_info);
};

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

  void Created(sim::Actor& self) override;

  void OnVisibilityChanged(sim::FrozenActor& frozen) override;

  bool IsVisible(sim::Actor& self, sim::Player* by_player) override;

  void TickRender(sim::Actor& self) override;

  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

  void Disposing(sim::Actor& self) override;

  int VisibilityHash = 0;

 private:

  void UpdateFrozenActor(sim::FrozenActor& frozen_actor, int player_index);

  bool IsVisibleInner(sim::Player& by_player);

  struct FrozenState {
    sim::FrozenActor* frozen_actor = nullptr;
    bool b_is_visible = false;
  };

  FrozenUnderFogInfoData info_;
  bool b_starts_revealed_ = false;
  std::vector<PPos> vec_footprint_;
  std::vector<FrozenState> vec_frozen_states_;
  bool b_created_ = false;
};

struct HiddenUnderShroudInfoData {
  sim::PlayerRelationship always_visible_relationships =
      sim::PlayerRelationship::Ally;
  VisibilityType type = VisibilityType::Footprint;

  static HiddenUnderShroudInfoData Parse(const meta::RecordObject& rec_info);
};

class HiddenUnderShroud : public sim::TraitBase,
                          public sim::IDefaultVisibility {
 public:
  ORA_TRAIT_INTERFACES(
      HiddenUnderShroud, OpenRA_Mods_Common_Traits_HiddenUnderShroud,
      sim::IDefaultVisibility)

  explicit HiddenUnderShroud(HiddenUnderShroudInfoData info)
      : info_{std::move(info)} {}

  virtual bool IsVisibleInner(sim::Actor& self, sim::Player& by_player);

  bool IsVisible(sim::Actor& self, sim::Player* by_player) override;

  const HiddenUnderShroudInfoData& Info() const { return info_; }

 protected:
  HiddenUnderShroudInfoData info_;
};

}
