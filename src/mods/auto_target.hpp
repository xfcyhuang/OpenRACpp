// UPSTREAM: OpenRA.Mods.Common/Traits/AutoTarget.cs @b6fc03f L21-487 +
//          AutoTargetPriority.cs L16-40(全文逐语义重写;IEditorActorOptions
//          的编辑器 UI 面随 Phase 6)
//          The whole of AutoTarget.cs L21-487 + AutoTargetPriority.cs
//          L16-40 (verbatim-semantics rewrites; the IEditorActorOptions
//          editor-UI face lands with Phase 6).
//
// 机制对照 / Mechanism mapping:
//  - FrozenDictionary<UnitStance, string> ConditionByStance → std::map<
//    UnitStance, string>(键序稳定;TryGetValue 语义不变)
//    The FrozenDictionary<UnitStance, string> ConditionByStance →
//    std::map<UnitStance, string> (a stable key order; TryGetValue
//    semantics unchanged).
//  - ChooseTarget 的 IEnumerable 惰性链(Select/Concat/Where)→ 两段物化
//    循环(actor 域 + frozen 域;PERF 注释的 validPriorities 复用列表 →
//    成员缓冲)
//    ChooseTarget's lazy IEnumerable chains (Select/Concat/Where) → the
//    two materialized loops (the actor domain + the frozen domain; the
//    PERF-noted validPriorities reuse list → a member buffer).
//  - Passenger trait 未移植:attacker 换乘 transport 面恒不触发(空集
//    等价分支;COVERAGE 登记)
//    The Passenger trait is unported: the attacker-swaps-to-transport face
//    never triggers (the empty-set equivalent branch; registered in
//    COVERAGE).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "mods/attack_base.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::Player;
using sim::TraitBase;

/// AutoTarget.cs L21:UnitStance
/// AutoTarget.cs L21: UnitStance.
enum class UnitStance : std::int32_t {
  HoldFire = 0,
  ReturnFire = 1,
  Defend = 2,
  AttackAnything = 3,
};

/// AutoTarget.cs L23-27:IActivityNotifyStanceChanged(IActivityInterface)
/// AutoTarget.cs L23-27: IActivityNotifyStanceChanged (an
/// IActivityInterface).
class IActivityNotifyStanceChanged : public sim::IActivityInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IActivityNotifyStanceChanged;
  virtual void StanceChanged(Actor& self, class AutoTarget* auto_target,
                             UnitStance old_stance, UnitStance new_stance) = 0;
};

/// AutoTarget.cs L29-33:INotifyStanceChanged
/// AutoTarget.cs L29-33: INotifyStanceChanged.
class INotifyStanceChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyStanceChanged;
  virtual ~INotifyStanceChanged() = default;
  virtual void StanceChanged(Actor& self, AutoTarget* auto_target,
                             UnitStance old_stance, UnitStance new_stance) = 0;
};

/// AutoTargetInfo 的解析面(L36-134)
/// The parsed face of AutoTargetInfo (L36-134).
struct AutoTargetInfoData {
  bool b_allow_movement = true;       // L39
  bool b_allow_turning = true;        // L42
  bool b_scan_on_idle = true;         // L45
  int int4_scan_radius = -1;          // L48
  UnitStance initial_stance_ai = UnitStance::AttackAnything;  // L52
  UnitStance initial_stance = UnitStance::Defend;             // L55
  std::string str_hold_fire_condition;    // L59
  std::string str_return_fire_condition;  // L62
  std::string str_defend_condition;       // L65
  std::string str_attack_anything_condition;  // L70
  bool b_enable_stances = true;        // L77
  int int4_minimum_scan_time_interval = 3;   // L80
  int int4_maximum_scan_time_interval = 8;   // L83
  sim::ConditionalTraitData conditional;

  /// RulesetLoaded(L90-108)的 ConditionByStance 物化
  /// The ConditionByStance materialization of RulesetLoaded (L90-108).
  std::map<UnitStance, std::string> map_condition_by_stance;

  static AutoTargetInfoData Parse(const meta::RecordObject& rec_info);
};

/// StanceInit(AutoTarget.cs L482-486;ValueActorInit;ISingleInstanceInit)
/// StanceInit (AutoTarget.cs L482-486; a ValueActorInit;
/// ISingleInstanceInit).
class StanceInit final : public sim::ValueActorInit<UnitStance>,
                         public sim::ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(StanceInit, OpenRA_Mods_Common_Traits_StanceInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit StanceInit(UnitStance value,
                      std::string str_instance_name = "")
      : sim::ValueActorInit<UnitStance>(value,
                                        std::move(str_instance_name)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

/// AutoTargetPriorityInfo 的解析面(AutoTargetPriority.cs L18-33)
/// The parsed face of AutoTargetPriorityInfo (AutoTargetPriority.cs
/// L18-33).
struct AutoTargetPriorityInfoData {
  core::BitSet<sim::TargetableType> bitset_valid_targets;  // L21 默认
                                             // Ground/Water/Air 的原始位由
                                             // 注册表装载面解析
  core::BitSet<sim::TargetableType> bitset_invalid_targets;  // L24
  sim::PlayerRelationship valid_relationships =              // L27
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;
  int int4_priority = 1;  // L30
  sim::ConditionalTraitData conditional;

  static AutoTargetPriorityInfoData Parse(const meta::RecordObject& rec_info);
};

/// AutoTargetPriority(AutoTargetPriority.cs L35-39;ConditionalTrait 空体)
/// AutoTargetPriority (AutoTargetPriority.cs L35-39; the ConditionalTrait
/// empty body).
class AutoTargetPriority final
    : public TraitBase,
      public sim::ConditionalTraitCore<AutoTargetPriority>,
      public sim::IObservesVariables,
      public sim::INotifyCreated {
 public:
  AutoTargetPriority(AutoTargetPriorityInfoData&& info);

  ORA_TRAIT_INTERFACES(AutoTargetPriority,
                       OpenRA_Mods_Common_Traits_AutoTargetPriority,
                       sim::IObservesVariables, sim::INotifyCreated)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AutoTargetPriority>::
        IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AutoTargetPriority>::
        IsTraitDisabled();
  }

  void Created(Actor& self) override { CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const AutoTargetPriorityInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<AutoTargetPriority>;
  AutoTargetPriorityInfoData info_;
};

/// AutoTarget(AutoTarget.cs L136-480)
class AutoTarget final : public TraitBase,
                         public sim::ConditionalTraitCore<AutoTarget>,
                         public sim::IObservesVariables,
                         public sim::INotifyCreated,
                         public sim::INotifyIdle,
                         public sim::INotifyDamage,
                         public sim::ITick,
                         public sim::IResolveOrder,
                         public sim::ISync,
                         public sim::INotifyOwnerChanged {
 public:
  AutoTarget(ActorInitializer& init, const AutoTargetInfoData& info);

  ORA_TRAIT_INTERFACES(AutoTarget, OpenRA_Mods_Common_Traits_AutoTarget,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyIdle, sim::INotifyDamage, sim::ITick,
                       sim::IResolveOrder, sim::ISync,
                       sim::INotifyOwnerChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AutoTarget>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AutoTarget>::IsTraitDisabled();
  }

  /// L138:ActiveAttackBases(构造期非禁用过滤)
  /// L138: ActiveAttackBases (the construction-time non-disabled filter).
  std::span<AttackFrontal* const> ActiveAttackBases() const {
    return vec_active_attack_bases_;
  }

  /// [VerifySync] L143/149 | the [VerifySync] members L143/149.
  int next_scan_time = 0;
  Actor* Aggressor = nullptr;

  UnitStance Stance() const { return stance_; }
  bool AllowMove() const {
    return b_allow_movement_ && stance_ > UnitStance::Defend;
  }

  /// NOT SYNCED: do not refer to this anywhere other than UI code(上游)
  UnitStance PredictedStance() const { return predicted_stance_; }

  /// L158-173:SetStance | L158-173: SetStance.
  void SetStance(Actor& self, UnitStance value);

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;   // L213-216
  void ResolveOrder(Actor& self, const net::Order& order) override;  // L218-222
  void Damaged(Actor& self, const sim::AttackInfo& e) override;  // L224-275
  void TickIdle(Actor& self) override;                // L277-284
  void Tick(Actor& self) override;                    // L286-293

  /// L295-321:ScanForTarget | L295-321: ScanForTarget.
  sim::Target ScanForTarget(Actor& self, bool allow_move, bool allow_turn,
                            bool ignore_scan_interval = false);

  /// L323-328:ScanAndAttack | L323-328: ScanAndAttack.
  void ScanAndAttack(Actor& self, bool allow_move, bool allow_turn);

  /// 活跃优先级表(上游的惰性 Where —— 每次枚举重评 IsTraitDisabled;
  /// L200 注释明示;C++ 以消费时过滤承载)
  /// The active priority table (upstream's lazy Where — IsTraitDisabled
  /// re-evaluated per enumeration; the L200 comment says so; carried by
  /// the consumption-time filter on the C++ side).
  std::vector<const AutoTargetPriorityInfoData*> ActiveTargetPriorities()
      const;

  /// L336-353:HasValidTargetPriority
  bool HasValidTargetPriority(Actor& self, Player* owner,
                              const core::BitSet<sim::TargetableType>&
                                  target_types);

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<AutoTarget>;

  /// L175-182:ApplyStanceCondition | L175-182: ApplyStanceCondition.
  void ApplyStanceCondition(Actor& self);

  /// L330-334:Attack | L330-334: Attack.
  void Attack(const sim::Target& target, bool allow_move);

  /// L355-470:ChooseTarget | L355-470: ChooseTarget.
  sim::Target ChooseTarget(Actor& self, AttackFrontal* ab,
                           sim::PlayerRelationship attack_stances,
                           WDist scan_range, bool allow_move,
                           bool allow_turn);

  /// L472-479:PreventsAutoTarget(静态)| L472-479: PreventsAutoTarget
  /// (static).
  static bool PreventsAutoTarget(Actor& attacker, Actor& target);

  const AutoTargetInfoData info_;
  bool b_allow_movement_ = false;
  UnitStance stance_ = UnitStance::Defend;
  UnitStance predicted_stance_ = UnitStance::Defend;
  std::vector<AttackFrontal*> vec_active_attack_bases_;
  std::vector<sim::IOverrideAutoTarget*> vec_override_auto_target_;
  std::vector<INotifyStanceChanged*> vec_notify_stance_changed_;
  std::vector<AutoTargetPriority*>
      vec_priorities_ordered_;  // L201-203(Priority 稳定降序;禁用过滤保
                                // 持上游惰性 —— 见 ActiveTargetPriorities)
  int int4_condition_token_ = Actor::InvalidConditionToken;  // L156

  /// ChooseTarget 的复用缓冲(L374 的 PERF 注释)
  /// ChooseTarget's reuse buffer (the L374 PERF note).
  std::vector<const AutoTargetPriorityInfoData*>
      vec_valid_priorities_buffer_;
};

}  // namespace ora::mods
