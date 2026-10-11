// UPSTREAM: OpenRA.Mods.Common/Traits/ChangesHealth.cs 实现部分
//          The implementation half.
#include "mods/changes_health.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

ChangesHealthInfoData ChangesHealthInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ChangesHealthInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "Step"))
    data.int4_step = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "Delay"))
    data.int4_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "DamageCooldown"))
    data.int4_damage_cooldown = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "StartIfBelow"))
    data.int4_start_if_below = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "KillIfBelow"))
    data.b_kill_if_below = *v != 0;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);

  return data;
}

ChangesHealth::ChangesHealth(ChangesHealthInfoData&& info)
    : sim::ConditionalTraitCore<ChangesHealth>(info.conditional),
      info_{std::move(info)} {}

// ———— [VerifySync] 哈希注册({ticks, damageTicks};第十二批 G2 补面)————
// ———— The [VerifySync] hash registration ({ticks, damageTicks}; the
//      batch-12 G2 missing face) ————

int ChangesHealthSyncHash(const sim::ISync* s) {
  const auto* changes = static_cast<const ChangesHealth*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(0, changes->int4_ticks),
      changes->int4_damage_ticks);
}

const bool b_changes_health_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.ChangesHealth",
                                &ChangesHealthSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_changes_health_sync_registered_anchor =
    &b_changes_health_sync_registered;

}  // namespace ora::mods
