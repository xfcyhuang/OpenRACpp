#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "mods/affects_shroud.hpp"
#include "mods/attack_base.hpp"
#include "mods/auto_target.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Turreted;
class AttackFollow;
class Rearmable;
class Aircraft;

namespace activities {

class AttackFollowActivity final : public sim::Activity,
                                   public IActivityNotifyStanceChanged {
 public:
  AttackFollowActivity(sim::Actor& self, AttackSource source,
                       const sim::Target& target, bool allow_move,
                       bool force_attack,
                       std::optional<core::Color> target_line_color);

  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;

  void StanceChanged(sim::Actor& self, AutoTarget* auto_target,
                     UnitStance old_stance, UnitStance new_stance) override;

 private:

  bool HasArmamentsFor(const sim::Target& target);

  AttackFollow* p_attack_;
  std::vector<RevealsShroud*> vec_reveals_shroud_;
  sim::IMove* p_move_ = nullptr;
  bool b_force_attack_;
  std::optional<core::Color> opt_target_line_color_;
  Rearmable* rearmable_ = nullptr;
  AttackSource source_;
  bool b_is_aircraft_ = false;
  MoveCooldownHelper move_cooldown_helper_;

  sim::Target target_;
  sim::Target last_visible_target_;
  bool b_use_last_visible_target_ = false;
  WDist dist_last_visible_maximum_range_{0};
  WDist dist_last_visible_minimum_range_{0};
  core::BitSet<sim::TargetableType> bitset_last_visible_target_types_;
  sim::Player* p_last_visible_owner_ = nullptr;
  bool b_has_ticked_ = false;
  bool b_return_to_base_ = false;
};

}

struct AttackFollowInfoData : AttackBaseInfoData {
  bool b_opportunity_fire = true;
  bool b_persistent_targeting = true;
  WDist dist_range_margin{1024};
  bool b_abort_on_resupply = true;

  static AttackFollowInfoData Parse(const meta::RecordObject& rec_info);
};

struct AttackTurretedInfoData : AttackFollowInfoData {
  std::vector<std::string> vec_turrets{"primary"};

  static AttackTurretedInfoData Parse(const meta::RecordObject& rec_info);
};

class AttackFollow : public AttackBaseCore<AttackFollow>,
                     public sim::IObservesVariables,
                     public sim::INotifyCreated,
                     public sim::INotifyOwnerChanged,
                     public sim::IOverrideAutoTarget,
                     public INotifyStanceChanged {
 public:
  AttackFollow(ActorInitializer& init, const AttackFollowInfoData& info);

  ORA_TRAIT_INTERFACES(
      AttackFollow, OpenRA_Mods_Common_Traits_AttackFollow, AttackBaseFace,
      sim::ITick, sim::IIssueOrder, sim::IResolveOrder, sim::IOrderVoice,
      sim::ISync, sim::IObservesVariables, sim::INotifyCreated,
      sim::INotifyOwnerChanged, sim::IOverrideAutoTarget,
      INotifyStanceChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AttackFollow>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AttackFollow>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const sim::Target& RequestedTarget() const { return target_requested_; }
  const sim::Target& OpportunityTarget() const {
    return target_opportunity_;
  }

  void SetRequestedTarget(const sim::Target& target,
                          bool is_force_attack = false,
                          sim::Activity* requested_target_preset = nullptr);

  void ClearRequestedTarget();

  virtual bool CanAimAtTarget(Actor& self, const sim::Target& target,
                              bool force_attack);

  void Tick(Actor& self) override;

  sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color = std::nullopt) override;

  void OnResolveAttackOrder(Actor& self, sim::Activity* activity,
                            const sim::Target& target, bool queued,
                            bool force_attack) override;

  void OnStopOrder(Actor& self) override;

  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

  std::optional<sim::Target> TryGetAutoTargetOverride(
      Actor& self) override;

  void StanceChanged(Actor& self, AutoTarget* auto_target,
                     UnitStance old_stance, UnitStance new_stance) override;

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  const AttackFollowInfoData& FollowInfo() const { return info_follow_; }

 protected:
  Mobile* p_mobile_ = nullptr;
  AutoTarget* p_auto_target_ = nullptr;

 private:
  friend class sim::ConditionalTraitCore<AttackFollow>;

  AttackFollowInfoData info_follow_;

  sim::Target target_requested_ = sim::Target::Invalid();
  sim::Target target_opportunity_ = sim::Target::Invalid();
  bool b_requested_force_attack_ = false;
  sim::Activity* p_requested_target_preset_for_activity_ = nullptr;
  bool b_opportunity_force_attack_ = false;
  bool b_opportunity_target_is_persistent_target_ = false;
};

class AttackTurreted final : public AttackFollow {
 public:
  AttackTurreted(ActorInitializer& init,
                 const AttackTurretedInfoData& info);

  ORA_TRAIT_INTERFACES(
      AttackTurreted, OpenRA_Mods_Common_Traits_AttackTurreted, AttackFollow,
      AttackBaseFace, sim::ITick, sim::IIssueOrder, sim::IResolveOrder,
      sim::IOrderVoice, sim::ISync, sim::IObservesVariables,
      sim::INotifyCreated, sim::INotifyOwnerChanged, sim::IOverrideAutoTarget,
      INotifyStanceChanged)

  bool CanAttack(Actor& self, const sim::Target& target) override;

  const AttackTurretedInfoData& TurretInfo() const { return info_turrets_; }

  std::span<Turreted* const> Turrets() const { return vec_turrets_; }

 private:
  AttackTurretedInfoData info_turrets_;
  std::vector<Turreted*> vec_turrets_;
};

}
