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

struct SeedsResourceInfoData {
  int int4_interval = 75;
  std::string str_resource_type = "Ore";
  int int4_max_range = 100;
  sim::ConditionalTraitData conditional;

  static SeedsResourceInfoData Parse(const meta::RecordObject& rec_info);
};

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

  void Seed(Actor& self) override;

  const SeedsResourceInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<SeedsResource>;

  SeedsResourceInfoData info_;
  sim::IResourceLayer* ptr_resource_layer_ = nullptr;
  int int4_ticks = 0;
};

}
