// UPSTREAM: OpenRA.Mods.Common/Traits/AmmoPool.cs + Rearmable.cs +
//          Repairable.cs + RepairsUnits.cs + Air/FallsToEarth.cs
//          @1f594e320
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "game/ruleset.hpp"
#include "meta/generic_record.hpp"
#include "mods/armament.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class WeaponInfo;
class ActorInfo;
class Ruleset;
}  // namespace ora::game

namespace ora::mods {

class Aircraft;

struct AmmoPoolInfoData {
  std::string str_name{"primary"};
  std::vector<std::string> vec_armaments{"primary", "secondary"};
  int int4_ammo = 1;
  int int4_initial_ammo = -1;
  int int4_reload_count = 1;
  std::string str_rearm_sound;
  int int4_reload_delay = 50;
  std::string str_ammo_condition;

  static AmmoPoolInfoData Parse(const meta::RecordObject& rec_info);
};

class AmmoPool final : public TraitBase,
                       public sim::INotifyCreated,
                       public sim::INotifyAttack,
                       public sim::ISync {
 public:
  explicit AmmoPool(const AmmoPoolInfoData& info);

  ORA_TRAIT_INTERFACES(AmmoPool, OpenRA_Mods_Common_Traits_AmmoPool,
                       sim::INotifyCreated, sim::INotifyAttack, sim::ISync)

  const AmmoPoolInfoData& Info() const { return info_; }

  int RemainingTicks() const { return int4_remaining_ticks_; }
  void SetRemainingTicks(int value) { int4_remaining_ticks_ = value; }
  int CurrentAmmoCount() const { return int4_current_ammo_count_; }
  bool HasAmmo() const { return int4_current_ammo_count_ > 0; }
  bool HasFullAmmo() const { return int4_current_ammo_count_ == info_.int4_ammo; }

  bool GiveAmmo(Actor& self, int count);
  bool TakeAmmo(Actor& self, int count);

  void Created(Actor& self) override;
  void Attacking(Actor& self, const sim::Target& target, Armament& a,
                 const Barrel& barrel) override;
  void PreparingAttack(Actor& self, const sim::Target& target, Armament& a,
                       const Barrel& barrel) override {}

 private:
  void UpdateCondition(Actor& self);

  AmmoPoolInfoData info_;
  std::vector<int> vec_tokens_;
  int int4_remaining_ticks_ = 0;
  int int4_current_ammo_count_ = 0;
};

struct RearmableInfoData {
  std::vector<std::string> vec_rearm_actors;
  std::vector<std::string> vec_ammo_pools{"primary"};

  static RearmableInfoData Parse(const meta::RecordObject& rec_info);
};

class Rearmable final : public TraitBase,
                        public sim::INotifyCreated,
                        public sim::INotifyDockClient {
 public:
  explicit Rearmable(RearmableInfoData info);

  ORA_TRAIT_INTERFACES(Rearmable, OpenRA_Mods_Common_Traits_Rearmable,
                       sim::INotifyCreated, sim::INotifyDockClient)

  const RearmableInfoData& Info() const { return info_; }
  std::span<AmmoPool* const> RearmableAmmoPools() const {
    return vec_rearmable_ammo_pools_;
  }

  void Created(Actor& self) override;
  void Docked(Actor& self, Actor& dock) override;
  void Undocked(Actor& self, Actor& dock) override {}

  bool RearmTick(Actor& self);

 private:
  RearmableInfoData info_;
  std::vector<AmmoPool*> vec_rearmable_ammo_pools_;
};

struct RepairsUnitsInfoData {
  sim::ConditionalTraitData conditional;
  int int4_value_percentage = 20;
  int int4_hp_per_step = 10;
  int int4_interval = 24;
  core::BitSet<sim::DamageType> bitset_repair_damage_types;
  std::string str_start_repairing_notification;
  std::string str_start_repairing_text_notification;
  std::string str_finish_repairing_notification;
  std::string str_finish_repairing_text_notification;
  int int4_player_experience = 0;

  static RepairsUnitsInfoData Parse(const meta::RecordObject& rec_info);
};

class RepairsUnits final : public TraitBase,
                           public sim::ConditionalTraitCore<RepairsUnits>,
                           public sim::IObservesVariables,
                           public sim::INotifyCreated {
 public:
  explicit RepairsUnits(const RepairsUnitsInfoData& info);

  ORA_TRAIT_INTERFACES(RepairsUnits,
                       OpenRA_Mods_Common_Traits_RepairsUnits,
                       sim::IObservesVariables, sim::INotifyCreated)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<RepairsUnits>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<RepairsUnits>::IsTraitDisabled();
  }

  void Created(Actor& self) override {
    sim::ConditionalTraitCore<RepairsUnits>::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const RepairsUnitsInfoData& Info() const { return info_; }

 private:
  RepairsUnitsInfoData info_;
};

struct RepairableInfoData {
  std::vector<std::string> vec_repair_actors;
  std::string str_voice{"Action"};
  int int4_hp_per_step = -1;
  std::optional<expr::BooleanExpression> expr_require_force_move;
  std::string str_enter_cursor{"enter"};
  std::string str_enter_blocked_cursor{"enter-blocked"};

  static RepairableInfoData Parse(const meta::RecordObject& rec_info);
};

class Repairable final : public TraitBase,
                         public sim::IIssueOrder,
                         public sim::IResolveOrder,
                         public sim::IOrderVoice,
                         public sim::INotifyCreated,
                         public sim::IObservesVariables {
 public:
  Repairable(ActorInitializer& init, const RepairableInfoData& info);

  ORA_TRAIT_INTERFACES(Repairable, OpenRA_Mods_Common_Traits_Repairable,
                       sim::IIssueOrder, sim::IResolveOrder, sim::IOrderVoice,
                       sim::INotifyCreated, sim::IObservesVariables)

  const RepairableInfoData& Info() const { return info_; }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override;

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

  Actor* FindRepairBuilding(Actor& self);

  bool CanRepairAt(Actor& target);
  bool CanRepairAt(Actor& target, sim::TargetModifiers modifiers);
  bool CanRepair();
  bool CanRearm();

  class RepairEnterTargeter final : public sim::IOrderTargeter {
   public:
    RepairEnterTargeter(const RepairableInfoData& info)
        : info_{info} {}

    std::string OrderID() const override { return "Repair"; }
    int OrderPriority() const override { return 5; }
    bool TargetOverridesSelection(
        Actor& self, const sim::Target& target,
        std::span<Actor* const> actors_at, CPos xy,
        sim::TargetModifiers modifiers) override;
    bool CanTarget(Actor& self, const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override;
    bool IsQueued() const override { return b_is_queued_; }

   private:
    const RepairableInfoData& info_;
    bool b_is_queued_ = false;
  };

 private:
  void RequireForceMoveConditionChanged(Actor& self,
                                        sim::ConditionCacheView conditions);

  RepairableInfoData info_;
  sim::IHealth* health_ = nullptr;
  Rearmable* rearmable_ = nullptr;
  bool b_require_force_move_ = false;
  bool b_is_aircraft_ = false;
  std::unique_ptr<RepairEnterTargeter> ptr_order_targeter_;
};

struct FallsToEarthInfoData {
  std::string str_explosion{"UnitExplode"};
  std::optional<WAngle> opt_maximum_spin_speed;
  bool b_moves = false;
  WDist dist_velocity{43};
  const game::WeaponInfo* ptr_explosion_weapon = nullptr;

  static FallsToEarthInfoData Parse(const meta::RecordObject& rec_info);
  void RulesetLoaded(const game::Ruleset& rules, const game::ActorInfo& ai);
};

class FallsToEarth final : public TraitBase,
                           public sim::IEffectiveOwner,
                           public sim::INotifyCreated {
 public:
  FallsToEarth(ActorInitializer& init, const FallsToEarthInfoData& info);

  ORA_TRAIT_INTERFACES(FallsToEarth, OpenRA_Mods_Common_Traits_FallsToEarth,
                       sim::IEffectiveOwner, sim::INotifyCreated)

  bool Disguised() const override { return true; }
  sim::Player* Owner() const override { return p_effective_owner_; }

  void Created(Actor& self) override;

 private:
  FallsToEarthInfoData info_;
  sim::Player* p_effective_owner_ = nullptr;
};

}  // namespace ora::mods
