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

enum class ObjectiveState { Incomplete = 0, Completed = 1, Failed = 2 };

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

struct MissionObjectivesInfoData {
  bool b_cooperative = false;
  bool b_early_game_over = false;
  int int4_game_over_delay = 1500;
  std::string str_win_notification;
  std::string str_win_text_notification;
  std::string str_loss_notification;
  std::string str_loss_text_notification;
  std::string str_leave_notification;
  std::string str_leave_text_notification;
  std::string str_mission_type = "generic";

  static MissionObjectivesInfoData Parse(const meta::RecordObject& rec_info);
};

class MissionObjectives final : public TraitBase, public sim::ISync {
 public:
  using ObjectiveAddedFn = std::function<void(sim::Player&, bool)>;

  MissionObjectives(sim::Player& player, MissionObjectivesInfoData&& info);

  ORA_TRAIT_INTERFACES(MissionObjectives,
                       OpenRA_Mods_Common_Traits_MissionObjectives,
                       sim::ISync)

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

}
