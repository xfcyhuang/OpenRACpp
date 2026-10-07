// UPSTREAM: OpenRA.Mods.Common/Traits/BlocksProjectiles.cs @b6fc03f
//          L17-75 全文(逐语义重写)
//          The whole of BlocksProjectiles.cs L17-75.
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

/// BlocksProjectilesInfo 的解析面(L18-26)
/// The parsed face of BlocksProjectilesInfo (L18-26).
struct BlocksProjectilesInfoData {
  WDist dist_height = WDist::FromCells(1);  // L20
  sim::PlayerRelationship valid_relationships =  // L23
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;
  sim::ConditionalTraitData conditional;

  static BlocksProjectilesInfoData Parse(const meta::RecordObject& rec_info);
};

/// BlocksProjectiles(L28-74)
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

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  /// L37-45:AnyBlockingActorAt(静态)
  /// L37-45: AnyBlockingActorAt (static).
  static bool AnyBlockingActorAt(sim::World& world, const WPos& pos);

  /// L47-72:AnyBlockingActorsBetween(静态;out hit → 引用出参)
  /// L47-72: AnyBlockingActorsBetween (static; the out hit → a reference
  /// out-parameter).
  static bool AnyBlockingActorsBetween(sim::World& world,
                                       sim::Player* owner,
                                       const WPos& start, const WPos& end,
                                       const WDist& width, WPos& hit);

 private:
  friend class sim::ConditionalTraitCore<BlocksProjectiles>;
  BlocksProjectilesInfoData info_;
};

}  // namespace ora::mods
