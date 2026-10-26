// UPSTREAM: OpenRA.Mods.Common/Traits/Player/MissionObjectives.cs @1f594e320
//          L21-70(Info 字段面 + 运行时类的 ISync 面;第十二批 G2 黄金
//          对拍补面 — player actor 的 ISync 条目缺失平移 SyncHash 的 n
//          计数)+ L21-25(MissionObjective)+ L115-125(Add 与
//          ObjectiveAdded 事件面;第十七批 MenuButtonsChrome 补面)。
//          目标列表的胜负分发面随战役批。
//          The Info field face + the runtime class's ISync face (L21-70;
//          the batch-12 G2 golden differential's missing face) +
//          L21-25 (MissionObjective) + L115-125 (Add and the
//          ObjectiveAdded event; the batch-17 MenuButtonsChrome face).
//          The win-state dispatch rides the campaign batch.
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

/// MissionObjectives.cs L20
enum class ObjectiveState { Incomplete = 0, Completed = 1, Failed = 2 };

/// MissionObjectives.cs L21-31
struct MissionObjective {
  bool b_required = true;
  std::string str_description;
  std::string str_type;
  ObjectiveState state = ObjectiveState::Incomplete;

  MissionObjective(std::string str_description_arg, std::string str_type_arg,
                   bool b_required_arg)
      : b_required{b_required_arg},
        str_description{std::move(str_description_arg)},
        str_type{std::move(str_type_arg)} {}
};

/// MissionObjectivesInfo 的解析面(L40-67)
/// The parsed face of MissionObjectivesInfo (L40-67).
struct MissionObjectivesInfoData {
  bool b_cooperative = false;      // L29
  bool b_early_game_over = false;  // L34
  int int4_game_over_delay = 1500;  // L37
  std::string str_win_notification;   // L40
  std::string str_win_text_notification;
  std::string str_loss_notification;
  std::string str_loss_text_notification;
  std::string str_leave_notification;
  std::string str_leave_text_notification;
  std::string str_mission_type = "generic";  // L46

  static MissionObjectivesInfoData Parse(const meta::RecordObject& rec_info);
};

/// MissionObjectives(L49-70 的 ISync 面;ObjectivesHash = 目标态 XOR)
/// MissionObjectives (the L49-70 ISync face; ObjectivesHash = the
/// objective-state XOR).
class MissionObjectives final : public TraitBase, public sim::ISync {
 public:
  using ObjectiveAddedFn = std::function<void(sim::Player&, bool)>;

  MissionObjectives(sim::Player& player, MissionObjectivesInfoData&& info);

  ORA_TRAIT_INTERFACES(MissionObjectives,
                       OpenRA_Mods_Common_Traits_MissionObjectives,
                       sim::ISync)

  /// L61-68:ObjectivesHash(目标态的 HashUsingHashCode XOR;空表 = 0)
  /// L61-68: ObjectivesHash (the objective states' HashUsingHashCode XOR;
  /// the empty list = 0).
  int ObjectivesHash() const {
    int hash = 0;
    for (const MissionObjective& objective : vec_objectives_)
      hash ^= static_cast<int>(objective.state);
    return hash;
  }

  const MissionObjectivesInfoData& Info() const { return info_; }
  const std::vector<MissionObjective>& Objectives() const {
    return vec_objectives_;
  }

  /// L115-125:Add(插入 + ObjectiveAdded 事件 + INotifyObjectivesUpdated)
  /// L115-125: Add (insert + the ObjectiveAdded event +
  /// INotifyObjectivesUpdated).
  int Add(sim::Player& player, std::string str_description,
          std::string str_type, bool b_required = true,
          bool b_inhibit_announcement = false);

  std::size_t AddObjectiveAddedHandler(ObjectiveAddedFn fn_handler);
  void RemoveObjectiveAddedHandler(std::size_t n_token);

 private:
  sim::Player* p_player_ = nullptr;
  MissionObjectivesInfoData info_;
  std::vector<MissionObjective> vec_objectives_;
  std::vector<std::pair<std::size_t, ObjectiveAddedFn>> vec_objective_added_;
  std::size_t n_next_token_ = 1;
};

}  // namespace ora::mods
