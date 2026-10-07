// UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ConditionalTrait.cs @b6fc03f
//          L19-92 全文 + PausableConditionalTrait.cs L19-77 全文
//          The whole of Conditions/ConditionalTrait.cs L19-92 + the whole of
//          PausableConditionalTrait.cs L19-77.
//
// 机制对照 / Mechanism mapping:
//  - C# 的 Info 侧基类(ConditionalTraitInfo.RulesetLoaded 的
//    EnabledByDefault/PausedByDefault)在值袋侧无生命周期钩子 → 解析时点
//    从"规则加载"移到"工厂解析"(NoVariables 求值同一表达式;时点差异 =
//    COVERAGE 登记,同 Health 的 D 系)
//    The C# Info-side base (ConditionalTraitInfo.RulesetLoaded's
//    EnabledByDefault/PausedByDefault) has no lifecycle hook on the bag
//    side → the evaluation moment moves from rules-load to
//    factory-parse (the same NoVariables expression; the timing
//    difference is registered in COVERAGE, like Health's D series).
//  - 泛型基类 ConditionalTrait<InfoType> → CRTP 组合核(ConcreteT 静态派发
//    TraitEnabled/TraitDisabled/TraitResumed/TraitPaused 四钩子,零虚表
//    间接);具体 trait 类继承 TraitBase + IObservesVariables +
//    INotifyCreated,转发到核
//    The generic base ConditionalTrait<InfoType> → a CRTP composition core
//    (ConcreteT statically dispatching the four hooks
//    TraitEnabled/TraitDisabled/TraitResumed/TraitPaused, zero vtable
//    indirection); the concrete trait class inherits TraitBase +
//    IObservesVariables + INotifyCreated and forwards into the core.
//  - 上游 GetVariableObservers 的 yield 序(base 先、子类追加)以
//    "核收集 + 子类 append"保真
//    Upstream's GetVariableObservers yield order (base first, the
//    subclass's additions after) is kept by "the core collects + the
//    subclass appends".
#pragma once
import std;

#include "meta/variable_expression.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/trait_registry.hpp"

namespace ora::sim {

/// 条件缓存 → 表达式符号向(求值桥;观察者路径,非热路径)
/// The condition cache → the expression symbol vector (the evaluation
/// bridge; an observer path, not hot).
inline std::vector<std::pair<std::string, std::int32_t>> ConditionSymbols(
    ConditionCacheView cache) {
  return {cache.begin(), cache.end()};
}

/// ConditionalTraitInfo + PausableConditionalTraitInfo 的解析面(值袋槽位)
/// The parsed face of ConditionalTraitInfo + PausableConditionalTraitInfo
/// (the bag slots).
struct ConditionalTraitData {
  std::optional<expr::BooleanExpression> expr_requires;  // L22 RequiresCondition
  std::optional<expr::BooleanExpression> expr_pause;     // L24 PauseOnCondition
  bool b_enabled_by_default = true;   // L31(NoVariables 求值)
  bool b_paused_by_default = false;   // L29(NoVariables 求值)

  /// 工厂面解析:RequiresCondition/PauseOnCondition 两键(缺 = 无条件)
  /// The factory-face parse: the RequiresCondition/PauseOnCondition keys
  /// (absent = unconditional).
  static ConditionalTraitData Parse(const meta::RecordObject& rec_info);
};

/// ConditionalTrait<InfoType> + PausableConditionalTrait<InfoType> 的组合核
/// (L41-92 + L39-77)。ConcreteT 需提供四钩子(可不写 —— 默认空体):
/// TraitEnabled/TraitDisabled/TraitResumed/TraitPaused
/// The composition core of ConditionalTrait<InfoType> +
/// PausableConditionalTrait<InfoType> (L41-92 + L39-77). ConcreteT may
/// provide the four hooks (optional — the defaults are empty):
/// TraitEnabled/TraitDisabled/TraitResumed/TraitPaused.
template <class ConcreteT>
class ConditionalTraitCore {
 public:
  explicit ConditionalTraitCore(const ConditionalTraitData& data)
      : data_(data) {
    // ConditionalTrait ctor L53:条件 trait 由 Actor 在 INotifyCreated 之后
    // 调 ConditionConsumers 启用 —— 初始即禁用(有 RequiresCondition 时)
    // The ConditionalTrait ctor L53: conditional traits are enabled (if
    // appropriate) by the Actor calling ConditionConsumers after
    // INotifyCreated — disabled at construction (when a RequiresCondition
    // exists).
    b_is_trait_disabled_ = data_.expr_requires.has_value();

    // PausableConditionalTrait ctor L50
    b_is_trait_paused_ = data_.b_paused_by_default;
  }

  bool IsTraitDisabled() const { return b_is_trait_disabled_; }
  bool IsTraitPaused() const { return b_is_trait_paused_; }

  /// ConditionalTrait.Created(L62-67)+ Pausable.Created(L50-56):无条件 →
  /// TraitEnabled + TraitResumed(有条件时的启用走首次条件通知)
  /// ConditionalTrait.Created (L62-67) + Pausable.Created (L50-56):
  /// unconditional → TraitEnabled + TraitResumed (a conditional trait's
  /// enabling rides the first condition notification).
  void CoreCreated(Actor& self) {
    if (!data_.expr_requires.has_value())
      Derived().TraitEnabledHook(self);
    if (!data_.expr_pause.has_value())
      Derived().TraitResumedHook(self);
  }

  /// GetVariableObservers(L49-52 + L57-63):base 序(requires → pause)
  /// GetVariableObservers (L49-52 + L57-63): the base order (requires →
  /// pause).
  std::vector<VariableObserver> CollectObservers() {
    std::vector<VariableObserver> vec_observers;
    if (data_.expr_requires.has_value())
      vec_observers.push_back(MakeObserver(
          [this](Actor& self, ConditionCacheView vars) {
            RequiredConditionsChanged(self, vars);
          },
          *data_.expr_requires));
    if (data_.expr_pause.has_value())
      vec_observers.push_back(MakeObserver(
          [this](Actor& self, ConditionCacheView vars) {
            PauseConditionsChanged(self, vars);
          },
          *data_.expr_pause));
    return vec_observers;
  }

 protected:
  /// 四钩子的静态派发落点(ConcreteT 未提供时落到本类空体)
  /// The static-dispatch landing of the four hooks (ConcreteT's overrides
  /// win; otherwise this class's empty bodies).
  void TraitEnabledHook(Actor&) {}
  void TraitDisabledHook(Actor&) {}
  void TraitResumedHook(Actor&) {}
  void TraitPausedHook(Actor&) {}

 private:
  ConcreteT& Derived() { return static_cast<ConcreteT&>(*this); }

  static VariableObserver MakeObserver(
      VariableObserverNotifier fn_notifier,
      const expr::BooleanExpression& expr) {
    VariableObserver observer;
    observer.fn_notifier = std::move(fn_notifier);
    observer.vec_variables.assign(expr.Variables().begin(),
                                  expr.Variables().end());
    return observer;
  }

  /// RequiredConditionsChanged(L69-84)
  void RequiredConditionsChanged(Actor& self, ConditionCacheView conditions) {
    if (!data_.expr_requires.has_value())
      return;

    const bool was_disabled = b_is_trait_disabled_;
    b_is_trait_disabled_ =
        !data_.expr_requires->Evaluate(ConditionSymbols(conditions));

    if (b_is_trait_disabled_ != was_disabled) {
      if (was_disabled)
        Derived().TraitEnabledHook(self);
      else
        Derived().TraitDisabledHook(self);
    }
  }

  /// PauseConditionsChanged(PausableConditionalTrait L65-76)
  void PauseConditionsChanged(Actor& self, ConditionCacheView conditions) {
    const bool was_paused = b_is_trait_paused_;
    b_is_trait_paused_ = data_.expr_pause->Evaluate(ConditionSymbols(conditions));

    if (b_is_trait_paused_ != was_paused) {
      if (was_paused)
        Derived().TraitResumedHook(self);
      else
        Derived().TraitPausedHook(self);
    }
  }

  ConditionalTraitData data_;
  bool b_is_trait_disabled_ = false;
  bool b_is_trait_paused_ = false;
};

}  // namespace ora::sim
