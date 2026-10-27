#include "sim/debug_pause_state.hpp"

#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::sim {

DebugPauseState::DebugPauseState(World& world) : world_ref_{world} {}

bool DebugPauseState::IsWorldPaused() const {
  return world_ref_.Paused();
}

int DebugPauseStateSyncHash(const ISync* s) {
  const auto* pause = static_cast<const DebugPauseState*>(s);
  return sync::CombineSyncHash(0, sync::HashBool(pause->IsWorldPaused()));
}

const bool b_debug_pause_state_sync_registered = [] {
  RegisterSyncHashFunction("OpenRA.Traits.DebugPauseState",
                           &DebugPauseStateSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_debug_pause_state_sync_registered_anchor =
    &b_debug_pause_state_sync_registered;

}
