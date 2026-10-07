// UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ExternalCondition.cs @b6fc03f
//          L14-190 全文 + Traits/Conditions/ProximityExternalCondition.cs
//          L18-194 全文
//          The whole of ExternalCondition.cs L14-190 + the whole of
//          Conditions/ProximityExternalCondition.cs L18-194.
//
// 机制对照 / Mechanism mapping:
//  - 上游 source(授予方标识)是 object(Actor/战头/支持力等);C++ 侧以
//    const void* 源键承载(地址等价;上游按引用相等判同 —— Dictionary
//    键语义一致)
//    Upstream's source (the granting-side identity) is an object (an
//    Actor/warhead/support power); the C++ side carries it as a const
//    void* source key (address equality; identical to upstream's
//    reference-equality Dictionary keys).
//  - GrantExternalConditionByName:GrantExternalConditionToProduced/
//    ToCrusher 经按名转发的注册表面(避免 conditions.cpp ↔ 本文件的头
//    循环;上游是直接 Trait<ExternalCondition> 查询 + Info.Condition 匹配)
//    GrantExternalConditionByName: the registry face the
//    GrantExternalConditionToProduced/ToCrusher pair routes through
//    (breaking the conditions.cpp ↔ this-file header cycle; upstream is
//    the direct Trait<ExternalCondition> query + Info.Condition match).
//  - ProximityExternalCondition 的 Enable/DisableSound 面省略(声音注入
//    惯例);tokens 的 Actor 键 = WorldArena 生命周期内稳定指针
//    ProximityExternalCondition's Enable/DisableSound faces are omitted
//    (the sound-injection convention); the tokens' Actor keys are
//    pointers stable over the WorldArena lifetime.
#pragma once
import std;

#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

/// ExternalCondition(L33-190)
class ExternalCondition final : public sim::TraitBase,
                                public sim::ITick,
                                public sim::INotifyCreated,
                                public sim::INotifyOwnerChanged {
 public:
  ORA_TRAIT_INTERFACES(ExternalCondition,
                       OpenRA_Mods_Common_Traits_ExternalCondition,
                       sim::ITick, sim::INotifyCreated,
                       sim::INotifyOwnerChanged)

  ExternalCondition(std::string str_condition, int source_cap, int total_cap)
      : str_condition_{std::move(str_condition)},
        int4_source_cap_{source_cap}, int4_total_cap_{total_cap} {}

  /// L57-76:CanGrantCondition(timed 不占 source 上限)
  /// L57-76: CanGrantCondition (timed ones do not occupy the source cap).
  bool CanGrantCondition(const void* source) const;

  /// L78-138:GrantCondition(计时/永久分账 + 双上限淘汰;返回 token)
  /// L78-138: GrantCondition (the timed/permanent split + the two-cap
  /// eviction; returns the token).
  int GrantCondition(sim::Actor& self, const void* source, int duration = 0,
                     int remaining = 0);

  /// L140-160:TryRevokeCondition
  bool TryRevokeCondition(sim::Actor& self, const void* source, int token);

  void Tick(sim::Actor& self) override;                        // L162-186
  void Created(sim::Actor& self) override;                     // L188-190
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,  // L105-108
                      sim::Player& new_owner) override;

  const std::string& Condition() const { return str_condition_; }

 private:
  /// TimedToken(L28-33):按 Expires 升序插入
  /// TimedToken (L28-33): inserted in ascending Expires order.
  struct TimedToken {
    int int4_expires = 0;
    int int4_token = 0;
    const void* ptr_source = nullptr;
  };

  std::string str_condition_;  // L21
  int int4_source_cap_ = 0;    // L24
  int int4_total_cap_ = 0;     // L27

  std::map<const void*, std::vector<int>> map_permanent_tokens_;
  std::vector<TimedToken> vec_timed_tokens_;  // 升序 Expires | ascending.
  std::vector<sim::IConditionTimerWatcher*> vec_watchers_;
  int int4_duration_ = 0;
  int int4_expires_ = 0;
};

/// GrantExternalConditionToProduced/ToCrusher 的按名转发面(上游的
/// TraitsImplementing<ExternalCondition>().FirstOrDefault(Condition 匹配
/// && CanGrant) 的等价 —— conditions.cpp 无头依赖消费)
/// The by-name forwarding face of GrantExternalConditionToProduced/
/// ToCrusher (the equivalent of upstream's
/// TraitsImplementing<ExternalCondition>().FirstOrDefault(Condition match
/// && CanGrant) — consumed by conditions.cpp without a header dependency).
void GrantExternalConditionByName(sim::Actor& target, sim::Actor& source,
                                  const std::string& str_condition,
                                  int duration);

/// ProximityExternalCondition(L31-194)
class ProximityExternalCondition final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::ITick,
      public sim::INotifyAddedToWorld,
      public sim::INotifyRemovedFromWorld,
      public sim::INotifyOtherProduction,
      public sim::INotifyProximityOwnerChanged,
      private sim::ConditionalTraitCore<ProximityExternalCondition> {
 public:
  ORA_TRAIT_INTERFACES(
      ProximityExternalCondition,
      OpenRA_Mods_Common_Traits_ProximityExternalCondition,
      sim::IObservesVariables, sim::INotifyCreated, sim::ITick,
      sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld,
      sim::INotifyOtherProduction, sim::INotifyProximityOwnerChanged)

  ProximityExternalCondition(sim::Actor& self, std::string str_condition,
                             WDist range, WDist maximum_vertical_offset,
                             sim::PlayerRelationship valid_relationships,
                             bool b_affects_parent,
                             sim::ConditionalTraitData conditional);
  ~ProximityExternalCondition() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void AddedToWorld(sim::Actor& self) override;                  // L57-61
  void RemovedFromWorld(sim::Actor& self) override;              // L63-66
  void Tick(sim::Actor& self) override;                          // L85-96
  void UnitProducedByOther(sim::Actor& self, sim::Actor& producer,  // L117-135
                           sim::Actor& produced,
                           const std::string& production_type,
                           sim::TypeDictionary& init) override;
  void OnProximityOwnerChanged(sim::Actor& actor,                // L158-194
                               sim::Player* old_owner,
                               sim::Player* new_owner) override;

 private:
  friend class sim::ConditionalTraitCore<ProximityExternalCondition>;
  void TraitEnabledHook(sim::Actor& self);   // L68-73
  void TraitDisabledHook(sim::Actor& self);  // L75-80
  void ActorEntered(sim::Actor& a);          // L98-114
  void ActorExited(sim::Actor& a);           // L137-154

  /// 就地授予(token 记账)| grant in place (token bookkeeping).
  bool GrantInRange(sim::Actor& target, sim::Actor& self);

  std::string str_condition_;  // L21
  WDist range_;                // L24
  WDist v_range_max_;          // L29
  sim::PlayerRelationship valid_relationships_ =
      sim::PlayerRelationship::Ally;  // L32
  bool b_affects_parent_ = false;     // L35

  sim::Actor* ptr_self_ = nullptr;
  std::map<sim::Actor*, int> map_tokens_;
  int int4_proximity_trigger_ = 0;
  WPos pos_cached_;
  WDist range_cached_;
  WDist v_range_cached_;
  WDist range_desired_;
  WDist v_range_desired_;
};

}  // namespace ora::mods
