// UPSTREAM: OpenRA.Mods.Common/Traits/Tooltip.cs @e180e3225
//          L16-102(实现面)
//          L16-102 (the implementation face).
#include "mods/tooltip.hpp"

#include "fluent/fluent_provider.hpp"
#include "sim/actor_init.hpp"

namespace ora::mods {

TooltipInfoData TooltipInfoData::Parse(const meta::RecordObject& rec_info) {
  TooltipInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = sim::RecordFieldString(rec_info, "Name"))
    data.str_name = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "GenericName"))
    data.str_generic_name = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "AllyPrefix"))
    data.str_ally_prefix = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "NeutralPrefix"))
    data.str_neutral_prefix = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "EnemyPrefix"))
    data.str_enemy_prefix = std::string{*v};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "GenericStancePrefix"))
    if (const auto* b = std::get_if<bool>(&gv->val))
      data.b_generic_stance_prefix = *b;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "ShowOwnerRow"))
    if (const auto* b = std::get_if<bool>(&gv->val))
      data.b_show_owner_row = *b;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "GenericVisibility"))
    if (const auto* n = std::get_if<std::int64_t>(&gv->val))
      data.kind_generic_visibility =
          static_cast<sim::PlayerRelationship>(*n);
  return data;
}

std::string TooltipInfoData::TooltipForPlayerStance(
    sim::PlayerRelationship relationship) {
  if (relationship == sim::PlayerRelationship::None ||
      !sim::HasRelationship(kind_generic_visibility, relationship))
    return fluent::FluentProvider::Instance().GetMessage(str_name);

  const std::string generic_name =
      str_generic_name.empty()
          ? std::string{}
          : fluent::FluentProvider::Instance().GetMessage(str_generic_name);
  if (b_generic_stance_prefix) {
    if (!str_ally_prefix.empty() &&
        relationship == sim::PlayerRelationship::Ally)
      return fluent::FluentProvider::Instance().GetMessage(str_ally_prefix) +
             " " + generic_name;

    if (!str_neutral_prefix.empty() &&
        relationship == sim::PlayerRelationship::Neutral)
      return fluent::FluentProvider::Instance().GetMessage(
                 str_neutral_prefix) +
             " " + generic_name;

    if (!str_enemy_prefix.empty() &&
        relationship == sim::PlayerRelationship::Enemy)
      return fluent::FluentProvider::Instance().GetMessage(str_enemy_prefix) +
             " " + generic_name;
  }

  return generic_name;
}

Tooltip::Tooltip(const ActorInitializer& init, TooltipInfoData info)
    : sim::ConditionalTraitCore<Tooltip>(info.conditional),
      info_{std::move(info)} {
  ptr_self_ = &init.Self();
}

}  // namespace ora::mods
