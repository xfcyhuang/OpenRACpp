// UPSTREAM: OpenRA.Mods.Cnc/Traits/GpsWatcher.cs @4f7930b5 L21-30(Info 面)
//          + L41-70(运行时类的 ISync 字段面;第十二批 G2 黄金对拍补面 —
//          player actor 的 ISync 条目缺失平移 SyncHash 的 n 计数;
//          GpsAdd/GpsRemove/ReachedOrbit 的刷新分发面随 support-power 批)
//          The Info face of GpsWatcher.cs L21-30 + the runtime class's
//          ISync field face (L41-70; the batch-12 G2 golden differential's
//          missing face — an absent player-actor ISync entry shifts the
//          SyncHash n counter; the GpsAdd/GpsRemove/ReachedOrbit refresh
//          dispatch rides the support-power batch).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// GpsWatcher(L41-70 的同步字段面)
/// GpsWatcher (the L41-70 synced-field face).
class GpsWatcher final : public TraitBase, public sim::ISync {
 public:
  GpsWatcher();

  ORA_TRAIT_INTERFACES(GpsWatcher, OpenRA_Mods_Cnc_Traits_GpsWatcher,
                       sim::ISync)

  bool Launched = false;       // L47
  bool GrantedAllies = false;  // L50
  bool Granted = false;        // L53
  bool Explored = false;       // L57 explored
};

}  // namespace ora::mods
