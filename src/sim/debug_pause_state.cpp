// UPSTREAM: OpenRA.Game/Traits/DebugPauseState.cs 实现部分
//          The implementation half.
#include "sim/debug_pause_state.hpp"

#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::sim {

DebugPauseState::DebugPauseState(World& world) : world_ref_{world} {}

bool DebugPauseState::IsWorldPaused() const {
  return world_ref_.Paused();
}

// ———— [VerifySync] 哈希注册({IsWorldPaused};第十二批 G2 补面)————
// ———— The [VerifySync] hash registration ({IsWorldPaused}; the batch-12
//      G2 missing face) ————

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

}  // namespace ora::sim
