// UPSTREAM: OpenRA.Mods.Common/Traits/CashTrickler.cs @e180e3225 L21-72(Info
//          字段面 + 运行时类的 ISync 面;第十二批 G2 黄金对拍补面 —
//          oilb 的 ISync 条目缺失平移 SyncHash 的 n 计数。Tick 计费与
//          金币浮字面随经济 UI 批)
//          The Info field face + the runtime class's ISync face (L21-72;
//          the batch-12 G2 golden differential's missing face — an absent
//          oilb ISync entry shifts the SyncHash n counter. The tick
//          crediting and the floating-cash face ride the economy-UI
//          batch).
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

/// CashTricklerInfo 的解析面(L23-49)
/// The parsed face of CashTricklerInfo (L23-49).
struct CashTricklerInfoData {
  int int4_interval = 50;         // L27
  int int4_initial_delay = 0;     // L30
  int int4_amount = 15;           // L33
  bool b_show_ticks = true;       // L36
  int int4_display_duration = 30;  // L39
  bool b_use_resource_storage = false;  // L42
  sim::ConditionalTraitData conditional;

  static CashTricklerInfoData Parse(const meta::RecordObject& rec_info);
};

/// CashTrickler(L51-72 的 ISync 面;Ticks 自 InitialDelay 起算)
/// CashTrickler (the L51-72 ISync face; Ticks starts at InitialDelay).
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

  /// L67-72:Created(resources/cloaks 缓存)
  /// L67-72: Created (the resources/cloaks caches).
  void Created(Actor& self) override;

  /// L75-89:Tick(禁用态回填 Interval;计满入账)
  /// L75-89: Tick (the disabled state refills Interval; the full
  /// counter credits the cash).
  void Tick(Actor& self) override;

  /// L65-66:[VerifySync] Ticks
  int Ticks = 0;

  const CashTricklerInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<CashTrickler>;

  CashTricklerInfoData info_;
  PlayerResources* ptr_resources_ = nullptr;
};

}  // namespace ora::mods
