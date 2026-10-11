// UPSTREAM: OpenRA.Mods.Common/Traits/CashTrickler.cs 实现部分
//          The implementation half.
#include "mods/cash_trickler.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/sync_hash.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

CashTricklerInfoData CashTricklerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  CashTricklerInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "Interval"))
    data.int4_interval = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "InitialDelay"))
    data.int4_initial_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "Amount"))
    data.int4_amount = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "ShowTicks"))
    data.b_show_ticks = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "DisplayDuration"))
    data.int4_display_duration = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "UseResourceStorage"))
    data.b_use_resource_storage = *v != 0;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);

  return data;
}

CashTrickler::CashTrickler(CashTricklerInfoData&& info)
    : sim::ConditionalTraitCore<CashTrickler>(info.conditional),
      info_{std::move(info)} {
  Ticks = info_.int4_initial_delay;
}

void CashTrickler::Created(Actor& self) {
  ptr_resources_ = self.Owner()->PlayerActor()->Trait<PlayerResources>();
}

void CashTrickler::Tick(Actor& self) {
  // L76-89(禁用/暂停态的 Interval 回填是同步面;入账的浮字面随
  // Phase 6)
  // L76-89 (the disabled/paused Interval refills are the synced face;
  // the credited floating text rides Phase 6).
  if (IsTraitDisabled())
    Ticks = info_.int4_interval;

  if (IsTraitPaused() || IsTraitDisabled())
    return;

  if (--Ticks < 0) {
    Ticks = info_.int4_interval;
    if (ptr_resources_ != nullptr && !info_.b_use_resource_storage)
      ptr_resources_->ChangeCash(info_.int4_amount);
  }
}

// ———— [VerifySync] 哈希注册({Ticks};第十二批 G2 补面)————
// ———— The [VerifySync] hash registration ({Ticks}; the batch-12 G2
//      missing face) ————

int CashTricklerSyncHash(const sim::ISync* s) {
  const auto* trickler = static_cast<const CashTrickler*>(s);
  return sim::sync::CombineSyncHash(0, trickler->Ticks);
}

const bool b_cash_trickler_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.CashTrickler",
                                &CashTricklerSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_cash_trickler_sync_registered_anchor =
    &b_cash_trickler_sync_registered;

}  // namespace ora::mods
