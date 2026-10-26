// UPSTREAM: OpenRA.Mods.Common/Traits/SeedsResource.cs @1f594e320 L21-81 全文
//          (第十二批 G2 黄金对拍修出:mine 的 Tick 播种 RandomWalk 是
//          SharedRandom 的每 tick 大宗消费者,缺失即 desync)
//          The whole of SeedsResource.cs L21-81 (rooted out by the
//          batch-12 G2 golden differential: the mines' tick-seeding
//          RandomWalk is a bulk SharedRandom consumer per tick — its
//          absence is an immediate desync).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// SeedsResourceInfo 的解析面(L23-33)
/// The parsed face of SeedsResourceInfo (L23-33).
struct SeedsResourceInfoData {
  int int4_interval = 75;            // L27
  std::string str_resource_type = "Ore";  // L28
  int int4_max_range = 100;          // L29
  sim::ConditionalTraitData conditional;

  static SeedsResourceInfoData Parse(const meta::RecordObject& rec_info);
};

/// SeedsResource(L35-81):首次 Tick 即播种,之后按 Interval 周期
/// SeedsResource (L35-81): seeds on the first tick, then every Interval.
class SeedsResource final : public TraitBase,
                            public sim::ConditionalTraitCore<SeedsResource>,
                            public sim::IObservesVariables,
                            public sim::ITick,
                            public sim::ISeedableResource {
 public:
  SeedsResource(ActorInitializer& init, SeedsResourceInfoData&& info);

  ORA_TRAIT_INTERFACES(SeedsResource,
                       OpenRA_Mods_Common_Traits_SeedsResource,
                       sim::IObservesVariables, sim::ITick,
                       sim::ISeedableResource)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<SeedsResource>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<SeedsResource>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  void Tick(Actor& self) override;

  /// L68-80:Seed(RandomWalk 游走至首个可加格)
  /// L68-80: Seed (the RandomWalk walk to the first addable cell).
  void Seed(Actor& self) override;

  const SeedsResourceInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<SeedsResource>;

  SeedsResourceInfoData info_;
  sim::IResourceLayer* ptr_resource_layer_ = nullptr;
  int int4_ticks = 0;
};

}  // namespace ora::mods
