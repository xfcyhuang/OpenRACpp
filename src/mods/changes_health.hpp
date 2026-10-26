// UPSTREAM: OpenRA.Mods.Common/Traits/ChangesHealth.cs @1f594e320 L21-64(Info
//          字段面 + 运行时类的 ISync 面;第十二批 G2 黄金对拍补面 —
//          ^Infantry 的两实例 ChangesHealth 条目缺失平移 SyncHash 的 n
//          计数。血量增减的 Tick 面随医疗 UI 批)
//          The Info field face + the runtime class's ISync face (L21-64;
//          the batch-12 G2 golden differential's missing face — ^Infantry's
//          two-instance ChangesHealth entries missing shift the SyncHash n
//          counter. The HP-adjustment Tick face rides the medical-UI
//          batch).
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

/// ChangesHealthInfo 的解析面(L23-46)
/// The parsed face of ChangesHealthInfo (L23-46).
struct ChangesHealthInfoData {
  int int4_step = 0;             // L27
  int int4_delay = 5;            // L31
  int int4_damage_cooldown = 0;  // L34
  int int4_start_if_below = 50;  // L38
  bool b_kill_if_below = 0;      // L42
  sim::ConditionalTraitData conditional;

  static ChangesHealthInfoData Parse(const meta::RecordObject& rec_info);
};

/// ChangesHealth(L48-64 的 ISync 面;两计数器自 0 起算)
/// ChangesHealth (the L48-64 ISync face; both counters start at 0).
class ChangesHealth final : public TraitBase,
                            public sim::ConditionalTraitCore<ChangesHealth>,
                            public sim::IObservesVariables,
                            public sim::ISync {
 public:
  ChangesHealth(ChangesHealthInfoData&& info);

  ORA_TRAIT_INTERFACES(ChangesHealth,
                       OpenRA_Mods_Common_Traits_ChangesHealth,
                       sim::IObservesVariables, sim::ISync)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<ChangesHealth>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<ChangesHealth>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  /// L51-54:[VerifySync] {ticks, damageTicks}
  int int4_ticks = 0;
  int int4_damage_ticks = 0;

  const ChangesHealthInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<ChangesHealth>;

  ChangesHealthInfoData info_;
};

}  // namespace ora::mods
