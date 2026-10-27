#include "sim/conditional_trait.hpp"

namespace ora::sim {

namespace {

const std::vector<std::pair<std::string, std::int32_t>>& NoVariables() {
  static const std::vector<std::pair<std::string, std::int32_t>> kEmpty;
  return kEmpty;
}
}

ConditionalTraitData ConditionalTraitData::Parse(
    const meta::RecordObject& rec_info) {
  ConditionalTraitData data;

  if (const auto v = RecordFieldString(rec_info, "RequiresCondition");
      v.has_value() && !v->empty())
    data.expr_requires = expr::BooleanExpression{std::string{*v}};
  if (const auto v = RecordFieldString(rec_info, "PauseOnCondition");
      v.has_value() && !v->empty())
    data.expr_pause = expr::BooleanExpression{std::string{*v}};

  data.b_enabled_by_default =
      !data.expr_requires.has_value() ||
      data.expr_requires->Evaluate(NoVariables());

  data.b_paused_by_default = data.expr_pause.has_value() &&
                             data.expr_pause->Evaluate(NoVariables());

  return data;
}

}
