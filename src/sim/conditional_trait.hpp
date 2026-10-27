#pragma once
import std;

#include "meta/variable_expression.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/trait_registry.hpp"

namespace ora::sim {

inline std::vector<std::pair<std::string, std::int32_t>> ConditionSymbols(
    ConditionCacheView cache) {
  return {cache.begin(), cache.end()};
}

struct ConditionalTraitData {
  std::optional<expr::BooleanExpression> expr_requires;
  std::optional<expr::BooleanExpression> expr_pause;
  bool b_enabled_by_default = true;
  bool b_paused_by_default = false;

  static ConditionalTraitData Parse(const meta::RecordObject& rec_info);
};

template <class ConcreteT>
class ConditionalTraitCore {
 public:
  explicit ConditionalTraitCore(const ConditionalTraitData& data)
      : data_(data) {

    b_is_trait_disabled_ = data_.expr_requires.has_value();

    b_is_trait_paused_ = data_.b_paused_by_default;
  }

  bool IsTraitDisabled() const { return b_is_trait_disabled_; }
  bool IsTraitPaused() const { return b_is_trait_paused_; }

  void CoreCreated(Actor& self) {
    if (!data_.expr_requires.has_value())
      Derived().TraitEnabledHook(self);
    if (!data_.expr_pause.has_value())
      Derived().TraitResumedHook(self);
  }

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

}
