#include "mods/targetable.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

TargetableInfoData TargetableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  TargetableInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "TargetTypes") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.bitset_target_types =
              core::BitSet<sim::TargetableType>::FromRawBits(
                  static_cast<std::uint64_t>(*n));
      } else if (name == "RequiresForceFire") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_requires_force_fire = *b;
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

bool Targetable::TargetableBy(Actor& , Actor& ) {

  return !IsTraitDisabled();
}

}
