#include "mods/gps_watcher.hpp"

#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

GpsWatcher::GpsWatcher() = default;

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

}
