#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "mods/attack_base.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::Player;
using sim::TraitBase;

enum class UnitStance : std::int32_t {
  HoldFire = 0,
  ReturnFire = 1,
  Defend = 2,
  AttackAnything = 3,
};

class IActivityNotifyStanceChanged : public sim::IActivityInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IActivityNotifyStanceChanged;
  virtual void StanceChanged(Actor& self, class AutoTarget* auto_target,
                             UnitStance old_stance, UnitStance new_stance) = 0;
};

class INotifyStanceChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyStanceChanged;
  virtual ~INotifyStanceChanged() = default;
  virtual void StanceChanged(Actor& self, AutoTarget* auto_target,
                             UnitStance old_stance, UnitStance new_stance) = 0;
};

struct AutoTargetInfoData {
  bool b_allow_movement = true;
  bool b_allow_turning = true;
  bool b_scan_on_idle = true;
  int int4_scan_radius = -1;
  UnitStance initial_stance_ai = UnitStance::AttackAnything;
  UnitStance initial_stance = UnitStance::Defend;
  std::string str_hold_fire_condition;
  std::string str_return_fire_condition;
  std::string str_defend_condition;
  std::string str_attack_anything_condition;
  bool b_enable_stances = true;
  int int4_minimum_scan_time_interval = 3;
  int int4_maximum_scan_time_interval = 8;
  sim::ConditionalTraitData conditional;

  std::map<UnitStance, std::string> map_condition_by_stance;

  static AutoTargetInfoData Parse(const meta::RecordObject& rec_info);
};

class StanceInit final : public sim::ValueActorInit<UnitStance>,
                         public sim::ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(StanceInit, OpenRA_Mods_Common_Traits_StanceInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit StanceInit(UnitStance value,
                      std::string str_instance_name = "")
      : sim::ValueActorInit<UnitStance>(value,
                                        std::move(str_instance_name)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

struct AutoTargetPriorityInfoData {
  core::BitSet<sim::TargetableType> bitset_valid_targets;

  core::BitSet<sim::TargetableType> bitset_invalid_targets;
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;
  int int4_priority = 1;
  sim::ConditionalTraitData conditional;

  static AutoTargetPriorityInfoData Parse(const meta::RecordObject& rec_info);
};

class AutoTargetPriority final
    : public TraitBase,
      public sim::ConditionalTraitCore<AutoTargetPriority>,
      public sim::IObservesVariables,
      public sim::INotifyCreated {
 public:
  AutoTargetPriority(AutoTargetPriorityInfoData&& info);

  ORA_TRAIT_INTERFACES(AutoTargetPriority,
                       OpenRA_Mods_Common_Traits_AutoTargetPriority,
                       sim::IObservesVariables, sim::INotifyCreated)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AutoTargetPriority>::
        IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AutoTargetPriority>::
        IsTraitDisabled();
  }

  void Created(Actor& self) override { CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const AutoTargetPriorityInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<AutoTargetPriority>;
  AutoTargetPriorityInfoData info_;
};

class AutoTarget final : public TraitBase,
                         public sim::ConditionalTraitCore<AutoTarget>,
                         public sim::IObservesVariables,
                         public sim::INotifyCreated,
                         public sim::INotifyIdle,
                         public sim::INotifyDamage,
                         public sim::ITick,
                         public sim::IResolveOrder,
                         public sim::ISync,
                         public sim::INotifyOwnerChanged {
 public:
  AutoTarget(ActorInitializer& init, const AutoTargetInfoData& info);

  ORA_TRAIT_INTERFACES(AutoTarget, OpenRA_Mods_Common_Traits_AutoTarget,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyIdle, sim::INotifyDamage, sim::ITick,
                       sim::IResolveOrder, sim::ISync,
                       sim::INotifyOwnerChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AutoTarget>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AutoTarget>::IsTraitDisabled();
  }

  std::span<AttackBaseFace* const> ActiveAttackBases() const {
    return vec_active_attack_bases_;
  }

  int next_scan_time = 0;
  Actor* Aggressor = nullptr;

  UnitStance Stance() const { return stance_; }
  bool AllowMove() const {
    return b_allow_movement_ && stance_ > UnitStance::Defend;
  }

  UnitStance PredictedStance() const { return predicted_stance_; }

  void SetStance(Actor& self, UnitStance value);

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;
  void Damaged(Actor& self, const sim::AttackInfo& e) override;
  void TickIdle(Actor& self) override;
  void Tick(Actor& self) override;

  sim::Target ScanForTarget(Actor& self, bool allow_move, bool allow_turn,
                            bool ignore_scan_interval = false);

  void ScanAndAttack(Actor& self, bool allow_move, bool allow_turn);

  std::vector<const AutoTargetPriorityInfoData*> ActiveTargetPriorities()
      const;

  bool HasValidTargetPriority(Actor& self, Player* owner,
                              const core::BitSet<sim::TargetableType>&
                                  target_types);

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<AutoTarget>;

  void ApplyStanceCondition(Actor& self);

  void Attack(const sim::Target& target, bool allow_move);

  sim::Target ChooseTarget(Actor& self, AttackBaseFace* ab,
                           sim::PlayerRelationship attack_stances,
                           WDist scan_range, bool allow_move,
                           bool allow_turn);

  static bool PreventsAutoTarget(Actor& attacker, Actor& target);

  const AutoTargetInfoData info_;
  bool b_allow_movement_ = false;
  UnitStance stance_ = UnitStance::Defend;
  UnitStance predicted_stance_ = UnitStance::Defend;
  std::vector<AttackBaseFace*> vec_active_attack_bases_;
  std::vector<sim::IOverrideAutoTarget*> vec_override_auto_target_;
  std::vector<INotifyStanceChanged*> vec_notify_stance_changed_;
  std::vector<AutoTargetPriority*>
      vec_priorities_ordered_;

  int int4_condition_token_ = Actor::InvalidConditionToken;

  std::vector<const AutoTargetPriorityInfoData*>
      vec_valid_priorities_buffer_;
};

}
