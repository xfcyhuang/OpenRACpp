// UPSTREAM: OpenRA.Mods.Common/Traits/Infantry/ScaredyCat.cs 实现部分
//          The implementation half.
#include "mods/scaredy_cat.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

ScaredyCatInfoData ScaredyCatInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ScaredyCatInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "PanicChance"))
    data.int4_panic_chance = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "AttackPanicChance"))
    data.int4_attack_panic_chance = static_cast<int>(*v);
  if (const auto v =
          sim::RecordFieldString(rec_info, "PanicSequencePrefix"))
    data.str_panic_sequence_prefix = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "PanicSpeedModifier"))
    data.int4_panic_speed_modifier = static_cast<int>(*v);

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "AvoidTerrainTypes") {
        // FrozenSet<string>:逗号分隔单串或集合节点(wands 同形)
        // FrozenSet<string>: a comma-separated single string or a
        // collection node (the same shape as wands').
        if (auto* s = std::get_if<std::string>(&v.val)) {
          std::string str_current;
          for (const char ch : *s + ",") {
            if (ch == ',') {
              const std::size_t begin =
                  str_current.find_first_not_of(' ');
              const std::size_t end =
                  str_current.find_last_not_of(' ');
              if (begin != std::string::npos)
                data.vec_avoid_terrain_types.emplace_back(
                    str_current.substr(begin, end - begin + 1));
              str_current.clear();
            } else {
              str_current.push_back(ch);
            }
          }
        } else if (auto* list =
                       std::get_if<std::vector<meta::GenericValue>>(
                           &v.val)) {
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              data.vec_avoid_terrain_types.push_back(*s);
        }
      }
    }
  }

  return data;
}

ScaredyCat::ScaredyCat(ScaredyCatInfoData&& info) : info_{std::move(info)} {}

// ———— [VerifySync] 哈希注册({panicStartedTick};第十二批 G2 补面)————
// ———— The [VerifySync] hash registration ({panicStartedTick}; the
//      batch-12 G2 missing face) ————

int ScaredyCatSyncHash(const sim::ISync* s) {
  const auto* cat = static_cast<const ScaredyCat*>(s);
  return sim::sync::CombineSyncHash(0, cat->int4_panic_started_tick);
}

const bool b_scaredy_cat_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.ScaredyCat",
                                &ScaredyCatSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_scaredy_cat_sync_registered_anchor =
    &b_scaredy_cat_sync_registered;

}  // namespace ora::mods
