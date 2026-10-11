// UPSTREAM: OpenRA.Mods.Common/Traits/Player/MissionObjectives.cs @4f7930b5
//          L21-70(Info 字段面 + 运行时类的 ISync 面;第十二批 G2 黄金
//          对拍补面 — player actor 的 ISync 条目缺失平移 SyncHash 的 n
//          计数。目标列表的增删/胜负分发面随战役批)
//          The Info field face + the runtime class's ISync face (L21-70;
//          the batch-12 G2 golden differential's missing face — an absent
//          player-actor ISync entry shifts the SyncHash n counter. The
//          objective list's add/remove and win-state dispatch ride the
//          campaign batch).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// MissionObjectivesInfo 的解析面(L23-47)
/// The parsed face of MissionObjectivesInfo (L23-47).
struct MissionObjectivesInfoData {
  bool b_cooperative = false;      // L29
  bool b_early_game_over = false;  // L34
  int int4_game_over_delay = 1500;  // L37
  std::string str_win_notification;   // L40
  std::string str_loss_notification;  // L43
  std::string str_mission_type = "generic";  // L46

  static MissionObjectivesInfoData Parse(const meta::RecordObject& rec_info);
};

/// MissionObjectives(L49-70 的 ISync 面;ObjectivesHash = 目标态 XOR)
/// MissionObjectives (the L49-70 ISync face; ObjectivesHash = the
/// objective-state XOR).
class MissionObjectives final : public TraitBase, public sim::ISync {
 public:
  MissionObjectives(sim::Player& player, MissionObjectivesInfoData&& info);

  ORA_TRAIT_INTERFACES(MissionObjectives,
                       OpenRA_Mods_Common_Traits_MissionObjectives,
                       sim::ISync)

  /// L61-68:ObjectivesHash(目标态的 HashUsingHashCode XOR;空表 = 0)
  /// L61-68: ObjectivesHash (the objective states' HashUsingHashCode XOR;
  /// the empty list = 0).
  int ObjectivesHash() const {
    int hash = 0;
    for (const int state : vec_objective_states_)
      hash ^= state;
    return hash;
  }

  const MissionObjectivesInfoData& Info() const { return info_; }

 private:
  sim::Player* p_player_ = nullptr;
  MissionObjectivesInfoData info_;
  std::vector<int> vec_objective_states_;
};

}  // namespace ora::mods
