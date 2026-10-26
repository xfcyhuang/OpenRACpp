// UPSTREAM: OpenRA.Mods.Common/Traits/Plug.cs + Traits/Pluggable.cs
//          @1f594e320(实现部分)
//          The implementation halves.
#include "mods/plug.hpp"

#include "meta/generic_record.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

namespace {

std::vector<std::pair<std::string, std::string>> ParseStringDict(
    const meta::RecordObject& rec_info, std::string_view str_name) {
  std::vector<std::pair<std::string, std::string>> vec_out;
  if (const auto* gv = sim::RecordFieldValue(rec_info, str_name))
    if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [k, v] : *dict)
        if (const auto* ks = std::get_if<std::string>(&k.val))
          if (const auto* vs = std::get_if<std::string>(&v.val))
            vec_out.emplace_back(*ks, *vs);
  return vec_out;
}

}  // namespace

PlugInfoData PlugInfoData::Parse(const meta::RecordObject& rec_info) {
  PlugInfoData data;
  if (const auto v = sim::RecordFieldString(rec_info, "Type"))
    data.str_type = std::string{*v};
  return data;
}

PluggableInfoData PluggableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  PluggableInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Offset"))
    if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
      if (tuple->uint1_count >= 2)
        data.cvec_offset = CVec{static_cast<int>(tuple->arr_ints[0]),
                                static_cast<int>(tuple->arr_ints[1])};
  data.vec_conditions = ParseStringDict(rec_info, "Conditions");

  if (const auto* gv = sim::RecordFieldValue(rec_info, "Requirements"))
    if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [k, v] : *dict)
        if (const auto* ks = std::get_if<std::string>(&k.val))
          if (const auto* vs = std::get_if<std::string>(&v.val))
            if (!vs->empty())
              data.vec_requirements.emplace_back(
                  *ks, expr::BooleanExpression{std::string{*vs}});

  if (const auto v = sim::RecordFieldString(rec_info, "EmptyOption"))
    if (!v->empty())
      data.str_empty_option = std::string{*v};
  return data;
}

Pluggable::Pluggable(sim::ActorInitializer& init, PluggableInfoData info,
                     std::string str_instance_name)
    : info_{std::move(info)}, str_instance_name_{std::move(str_instance_name)} {
  str_initial_plug_ =
      init.GetValue<PlugInit, std::string>(str_instance_name_, std::string{});

  if (!info_.vec_requirements.empty())
    for (const auto& [plug, expression] : info_.vec_requirements)
      map_plug_types_availability[plug] = true;
}

void Pluggable::Created(sim::Actor& self) {
  if (!str_initial_plug_.empty())
    EnablePlug(self, str_initial_plug_);
}

bool Pluggable::AcceptsPlug(std::string_view str_type) const {
  bool has_condition = false;
  for (const auto& [k, v] : info_.vec_conditions)
    if (k == str_type)
      has_condition = true;
  if (!has_condition)
    return false;

  bool has_requirement = false;
  for (const auto& [k, v] : info_.vec_requirements)
    if (k == str_type)
      has_requirement = true;
  if (!has_requirement)
    return str_active_.empty();

  const auto it = map_plug_types_availability.find(std::string{str_type});
  return it != map_plug_types_availability.end() && it->second;
}

void Pluggable::EnablePlug(sim::Actor& self, std::string_view str_type) {
  std::string condition;
  for (const auto& [k, v] : info_.vec_conditions)
    if (k == str_type)
      condition = v;
  if (condition.empty())
    return;

  if (int4_condition_token_ != sim::Actor::InvalidConditionToken)
    self.RevokeCondition(int4_condition_token_);

  int4_condition_token_ = self.GrantCondition(condition);
  str_active_ = std::string{str_type};
}

void Pluggable::DisablePlug(sim::Actor& self, std::string_view str_type) {
  if (str_type != str_active_)
    return;

  if (int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);

  str_active_.clear();
}

std::vector<sim::VariableObserver> Pluggable::GetVariableObservers() {
  std::vector<sim::VariableObserver> vec_observers;
  for (const auto& [str_key, expression] : info_.vec_requirements) {
    std::vector<std::string> vec_variables(expression.Variables().begin(),
                                           expression.Variables().end());
    vec_observers.push_back(sim::VariableObserver{
        [this, str_key, expression](sim::Actor&,
                                    sim::ConditionCacheView vars) {
          std::vector<std::pair<std::string, std::int32_t>> vec_symbols{
              vars.begin(), vars.end()};
          map_plug_types_availability[str_key] =
              expression.Evaluate(vec_symbols);
        },
        std::move(vec_variables)});
  }
  return vec_observers;
}

}  // namespace ora::mods
