// UPSTREAM: OpenRA.Mods.Common/Traits/Infantry/ScaredyCat.cs @e180e3225 L21-40(Info
//          字段面)+ L42-58(运行时类的 ISync 面;第十二批 G2 黄金对拍
//          补面 — ^CivInfantry 的 ScaredyCat 条目缺失平移 SyncHash 的 n
//          计数。恐慌移动/序列前缀面随行为批)
//          The Info field face (L21-40) + the runtime class's ISync face
//          (L42-58; the batch-12 G2 golden differential's missing face —
//          ^CivInfantry's absent ScaredyCat entry shifts the SyncHash n
//          counter. The panic-move/sequence-prefix faces ride the
//          behaviour batch).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// ScaredyCatInfo 的解析面(L23-40)
/// The parsed face of ScaredyCatInfo (L23-40).
struct ScaredyCatInfoData {
  int int4_panic_chance = 40;    // L28
  int int4_attack_panic_chance = 20;  // L31
  std::string str_panic_sequence_prefix = "panic-";  // L34
  int int4_panic_speed_modifier = 150;  // L37
  std::vector<std::string> vec_avoid_terrain_types;  // L40

  static ScaredyCatInfoData Parse(const meta::RecordObject& rec_info);
};

/// ScaredyCat(L42-58 的 ISync 面;panicStartedTick 自 0 起算)
/// ScaredyCat (the L42-58 ISync face; panicStartedTick starts at 0).
class ScaredyCat final : public TraitBase, public sim::ISync {
 public:
  ScaredyCat(ScaredyCatInfoData&& info);

  ORA_TRAIT_INTERFACES(ScaredyCat, OpenRA_Mods_Common_Traits_ScaredyCat,
                       sim::ISync)

  /// L48-49:[VerifySync] panicStartedTick
  int int4_panic_started_tick = 0;

  const ScaredyCatInfoData& Info() const { return info_; }

 private:
  ScaredyCatInfoData info_;
};

}  // namespace ora::mods
