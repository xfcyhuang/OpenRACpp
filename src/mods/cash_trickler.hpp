#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "mods/player_resources.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct CashTricklerInfoData {
  int int4_interval = 50;
  int int4_initial_delay = 0;
  int int4_amount = 15;
  bool b_show_ticks = true;
  int int4_display_duration = 30;
  bool b_use_resource_storage = false;
  sim::ConditionalTraitData conditional;

  static CashTricklerInfoData Parse(const meta::RecordObject& rec_info);
};

class CashTrickler final : public TraitBase,
                           public sim::ConditionalTraitCore<CashTrickler>,
                           public sim::IObservesVariables,
                           public sim::INotifyCreated,
                           public sim::ITick,
                           public sim::ISync {
 public:
  CashTrickler(CashTricklerInfoData&& info);

  ORA_TRAIT_INTERFACES(CashTrickler,
                       OpenRA_Mods_Common_Traits_CashTrickler,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick, sim::ISync)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<CashTrickler>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<CashTrickler>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  void Created(Actor& self) override;

  void Tick(Actor& self) override;

  int Ticks = 0;

  const CashTricklerInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<CashTrickler>;

  CashTricklerInfoData info_;
  PlayerResources* ptr_resources_ = nullptr;
};

}
