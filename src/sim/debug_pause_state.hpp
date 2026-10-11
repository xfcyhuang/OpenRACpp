// UPSTREAM: OpenRA.Game/Traits/DebugPauseState.cs @b6fc03f L18-39 全文
//          (第十二批 G2 黄金对拍补面:world actor 的 ISync 条目缺失会
//          平移 SyncHash 的 n 计数)
//          The whole of DebugPauseState.cs L18-39 (the batch-12 G2 golden
//          differential's missing face: an absent world-actor ISync entry
//          shifts the SyncHash n counter).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

using TraitBase = sim::TraitBase;

/// DebugPauseState(L31-39):暂停状态入同步哈希
/// DebugPauseState (L31-39): the pause state entering the sync hash.
class DebugPauseState final : public TraitBase, public ISync {
 public:
  explicit DebugPauseState(World& world);

  ORA_TRAIT_INTERFACES(DebugPauseState, OpenRA_Traits_DebugPauseState,
                       sim::ISync)

  /// L36-37:IsWorldPaused => world.Paused
  bool IsWorldPaused() const;

 private:
  World& world_ref_;
};

}  // namespace ora::sim
