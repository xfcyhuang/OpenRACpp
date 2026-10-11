// UPSTREAM: OpenRA.Mods.Cnc/Traits/GpsWatcher.cs 实现部分
//          The implementation half.
#include "mods/gps_watcher.hpp"

#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

GpsWatcher::GpsWatcher() = default;

// ———— [VerifySync] 哈希注册(gen/sync_gen.cpp 表序:Launched^
//      GrantedAllies^Granted^explored;第十二批 G2 补面)————
// ———— The [VerifySync] hash registration (the gen/sync_gen.cpp table
//      order: Launched^GrantedAllies^Granted^explored; the batch-12 G2
//      missing face) ————

int GpsWatcherSyncHash(const sim::ISync* s) {
  const auto* gps = static_cast<const GpsWatcher*>(s);
  int acc = 0;
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(gps->Launched));
  acc = sim::sync::CombineSyncHash(acc,
                                   sim::sync::HashBool(gps->GrantedAllies));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(gps->Granted));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(gps->Explored));
  return acc;
}

const bool b_gps_watcher_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Cnc.Traits.GpsWatcher",
                                &GpsWatcherSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_gps_watcher_sync_registered_anchor =
    &b_gps_watcher_sync_registered;

}  // namespace ora::mods
