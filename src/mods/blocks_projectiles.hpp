#pragma once
import std;

#include "core/wdist.hpp"
#include "mods/world_exts.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct BlocksProjectilesInfoData {
  WDist dist_height = WDist::FromCells(1);
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;
  sim::ConditionalTraitData conditional;

  static BlocksProjectilesInfoData Parse(const meta::RecordObject& rec_info);
};

class BlocksProjectiles final
    : public TraitBase,
      public sim::ConditionalTraitCore<BlocksProjectiles>,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::IBlocksProjectiles {
 public:
  BlocksProjectiles(const BlocksProjectilesInfoData& info)
      : sim::ConditionalTraitCore<BlocksProjectiles>(info.conditional),
        info_{info} {}

  ORA_TRAIT_INTERFACES(BlocksProjectiles,
                       OpenRA_Mods_Common_Traits_BlocksProjectiles,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::IBlocksProjectiles)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<BlocksProjectiles>::
        IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<BlocksProjectiles>::
        IsTraitDisabled();
  }

  void Created(Actor& self) override { CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  WDist BlockingHeight() const override { return info_.dist_height; }
  sim::PlayerRelationship ValidRelationships() const override {
    return info_.valid_relationships;
  }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  static bool AnyBlockingActorAt(sim::World& world, const WPos& pos);

  static bool AnyBlockingActorsBetween(sim::World& world,
                                       sim::Player* owner,
                                       const WPos& start, const WPos& end,
                                       const WDist& width, WPos& hit);

 private:
  friend class sim::ConditionalTraitCore<BlocksProjectiles>;
  BlocksProjectilesInfoData info_;
};

}
