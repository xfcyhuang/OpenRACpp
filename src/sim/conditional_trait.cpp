// UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ConditionalTrait.cs @b6fc03f
//          L19-39(ConditionalTraitInfo.RulesetLoaded 的 NoVariables 求值面)
//          The NoVariables-evaluation face of
//          ConditionalTraitInfo.RulesetLoaded (L19-39).
#include "sim/conditional_trait.hpp"

namespace ora::sim {

namespace {
/// VariableExpression.NoVariables(上游空符号表求值:未授条件计数为 0)
/// VariableExpression.NoVariables (upstream evaluates against the empty
/// symbol table: ungranted conditions count as 0).
const std::vector<std::pair<std::string, std::int32_t>>& NoVariables() {
  static const std::vector<std::pair<std::string, std::int32_t>> kEmpty;
  return kEmpty;
}
}  // namespace

ConditionalTraitData ConditionalTraitData::Parse(
    const meta::RecordObject& rec_info) {
  ConditionalTraitData data;

  // 空串 = 上游 null 载荷(yaml `RequiresCondition:` 无值)→ 无条件
  // An empty string = upstream's null payload (yaml `RequiresCondition:`
  // with no value) → unconditional.
  if (const auto v = RecordFieldString(rec_info, "RequiresCondition");
      v.has_value() && !v->empty())
    data.expr_requires = expr::BooleanExpression{std::string{*v}};
  if (const auto v = RecordFieldString(rec_info, "PauseOnCondition");
      v.has_value() && !v->empty())
    data.expr_pause = expr::BooleanExpression{std::string{*v}};

  // ConditionalTraitInfo.RulesetLoaded L30:EnabledByDefault =
  // RequiresCondition == null || RequiresCondition.Evaluate(NoVariables)
  // ConditionalTraitInfo.RulesetLoaded L30: EnabledByDefault =
  // RequiresCondition == null || RequiresCondition.Evaluate(NoVariables).
  data.b_enabled_by_default =
      !data.expr_requires.has_value() ||
      data.expr_requires->Evaluate(NoVariables());

  // PausableConditionalTraitInfo.RulesetLoaded L28:PausedByDefault =
  // PauseOnCondition != null && PauseOnCondition.Evaluate(NoVariables)
  // PausableConditionalTraitInfo.RulesetLoaded L28: PausedByDefault =
  // PauseOnCondition != null && PauseOnCondition.Evaluate(NoVariables).
  data.b_paused_by_default = data.expr_pause.has_value() &&
                             data.expr_pause->Evaluate(NoVariables());

  return data;
}

}  // namespace ora::sim
