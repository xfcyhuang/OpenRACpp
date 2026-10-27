#pragma once
import std;

#include "core/bitset.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::TraitBase;

struct RepairableBuildingInfoData {
  sim::ConditionalTraitData conditional;
  int int4_repair_percent = 20;
  int int4_repair_interval = 24;
  int int4_repair_step = 7;
  core::BitSet<sim::DamageType> bitset_repair_damage_types;
  std::vector<int> vec_repair_bonuses{100, 150, 175, 200, 220, 240, 260, 280, 300};
  bool b_cancel_when_disabled = false;
  int int4_player_experience = 0;
  std::string str_repair_condition;
  std::string str_repairing_notification;
  std::string str_repairing_text_notification;
  std::string str_repairing_stopped_notification;
  std::string str_repairing_stopped_text_notification;

  static RepairableBuildingInfoData Parse(const meta::RecordObject& rec_info);
};

class RepairableBuilding final : public TraitBase,
                                 public sim::ConditionalTraitCore<RepairableBuilding>,
                                 public sim::IObservesVariables,
                                 public sim::INotifyCreated,
                                 public sim::ITick,
                                 public sim::ISync {
 public:
  RepairableBuilding(Actor& self, RepairableBuildingInfoData info);

  ORA_TRAIT_INTERFACES(RepairableBuilding,
                       OpenRA_Mods_Common_Traits_RepairableBuilding,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick, sim::ISync)

  void Created(Actor& self) override {
    sim::ConditionalTraitCore<RepairableBuilding>::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<RepairableBuilding>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<RepairableBuilding>::IsTraitDisabled();
  }

  int RepairersHash() const;

  void RepairBuilding(Actor& self, sim::Player* ptr_player);

  void Tick(Actor& self) override;

  const RepairableBuildingInfoData& Info() const { return info_; }
  bool RepairActive() const { return b_repair_active_; }
  const std::vector<sim::Player*>& Repairers() const { return vec_repairers_; }

 private:
  void UpdateCondition(Actor& self);

  RepairableBuildingInfoData info_;
  sim::IHealth* ptr_health_ = nullptr;
  std::vector<int> vec_repair_tokens_;
  std::vector<sim::Player*> vec_repairers_;
  int int4_remaining_ticks_ = 0;
  bool b_repair_active_ = false;
};

}
