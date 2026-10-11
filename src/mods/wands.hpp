// UPSTREAM: OpenRA.Mods.Common/Traits/Wanders.cs @b6fc03f L21-118 全文
//          (逐语义重写;第十二批随 G2 黄金对拍修出 —— 平民 7 人的
//          SharedRandom 构造消耗缺失致帧 1 SyncHash 分歧)
//          The whole of Wanders.cs L21-118 (verbatim-semantics rewrite;
//          fixed into existence by the batch-12 G2 golden differential —
//          the civilians' SharedRandom constructor draws were missing,
//          diverging frame 1's SyncHash).
//
// 机制对照 / Mechanism mapping:
//  - WandersInfo.Requires<IMoveInfo> 的装载期校验未设(C++ 侧全部宿主
//    均挂 Mobile;空缺面与 Refinery 的 Requires 登记侧跳过同形)
//    WandersInfo.Requires<IMoveInfo>'s load-time validation is not
//    installed (every C++ host carries Mobile; the gap is the same shape
//    as Refinery's registration-side Requires skip).
//  - AvoidTerrainTypes 的 FrozenSet<string> → vector<string>
//    (resource_layer 同形)
//    AvoidTerrainTypes' FrozenSet<string> → vector<string> (the same
//    shape as resource_layer's).
#pragma once
import std;

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

/// WandersInfo 的解析面(L23-40)
/// The parsed face of WandersInfo (L23-40).
struct WandersInfoData {
  int int4_wander_move_radius = 1;   // L26
  int int4_reduce_move_radius_delay = 5;  // L29
  int int4_min_move_delay = 0;       // L32
  int int4_max_move_delay = 0;       // L35
  std::vector<std::string> vec_avoid_terrain_types;  // L38
  sim::ConditionalTraitData conditional;

  static WandersInfoData Parse(const meta::RecordObject& rec_info);
};

/// Wanders(L42-118):闲逛状态机(倒计时 → 择目标格 → MoveTo 入队)
/// Wanders (L42-118): the wander state machine (countdown → target pick
/// → MoveTo enqueue).
class Wanders final : public TraitBase,
                      public sim::ConditionalTraitCore<Wanders>,
                      public sim::IObservesVariables,
                      public sim::INotifyBecomingIdle,
                      public sim::INotifyIdle {
 public:
  /// L47-55:构造里的 SharedRandom.Next(Min,Max) 是 G2 帧哈希的组成部分
  /// L47-55: the constructor's SharedRandom.Next(Min,Max) is a component
  /// of the G2 frame hash.
  Wanders(ActorInitializer& init, WandersInfoData&& info);

  ORA_TRAIT_INTERFACES(Wanders, OpenRA_Mods_Common_Traits_Wanders,
                       sim::IObservesVariables, sim::INotifyBecomingIdle,
                       sim::INotifyIdle)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<Wanders>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<Wanders>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  /// L57-63:OnBecomingIdle 重掷倒计时
  void OnBecomingIdle(Actor& self) override;

  /// L69-79:TickIdle 的倒计时推进与触发
  void TickIdle(Actor& self) override;

  /// L105-111:DoAction(活动入队;virtual 供子类替换)
  virtual void DoAction(Actor& self, const CPos& target_cell);

  const WandersInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 protected:
  /// L57-60:OnBecomingIdle 的可覆写核
  virtual void OnBecomingIdleInner(Actor& self);

 private:
  friend class sim::ConditionalTraitCore<Wanders>;

  /// L81-103:PickTargetLocation(朝向掷值 → 目标格;越界时半径衰减)
  std::optional<CPos> PickTargetLocation(Actor& self);

  WandersInfoData info_;
  sim::IMove* ptr_move_ = nullptr;      // L45 Move
  std::optional<core::Color> color_target_line_;  // moveInfo.GetTargetLineColor()
  int int4_countdown = 0;               // L50
  int int4_ticks_idle = 0;              // L51
  int int4_effective_move_radius = 0;   // L52
};

}  // namespace ora::mods
