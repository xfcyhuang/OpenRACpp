#pragma once
import std;

#include "core/wdist.hpp"
#include "game/game_records.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/weapons.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct Barrel {
  WVec Offset;
  WAngle Yaw;
};

struct ArmamentInfoData {
  std::string str_name{"primary"};
  std::string str_weapon;
  std::string str_turret{"primary"};
  int int4_fire_delay = 0;
  std::vector<WVec> vec_local_offset{};
  std::vector<WAngle> vec_local_yaw{};
  WDist dist_recoil{0};
  WDist dist_recoil_recovery{9};
  std::string str_muzzle_sequence;
  std::string str_muzzle_palette{"effect"};
  std::string str_reloading_condition;
  sim::PlayerRelationship target_relationships =
      sim::PlayerRelationship::Enemy;
  sim::PlayerRelationship force_target_relationships =
      sim::PlayerRelationship::Enemy | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Ally;
  std::string str_cursor{"attack"};
  std::string str_outside_range_cursor{"attackoutsiderange"};
  int int4_ammo_usage = 1;

  const game::WeaponInfo* weapon_info = nullptr;
  WDist dist_modified_range{0};
  sim::ConditionalTraitData conditional;

  static ArmamentInfoData Parse(const meta::RecordObject& rec_info,
                                const sim::World& world);
};

class Armament final : public TraitBase,
                       public sim::ConditionalTraitCore<Armament>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::ITick {
 public:
  Armament(ActorInitializer& init, const ArmamentInfoData& info);

  ORA_TRAIT_INTERFACES(Armament, OpenRA_Mods_Common_Traits_Armament,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<Armament>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<Armament>::IsTraitDisabled();
  }

  const game::WeaponInfo* Weapon = nullptr;

  const ArmamentInfoData& InfoData() const { return info_; }
  std::vector<Barrel> vec_barrels;
  WDist Recoil;
  int FireDelay = 0;
  int Burst = 0;
  Actor* ActorPtr = nullptr;

  WDist MaxRange();

  void Created(Actor& self) override;

  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void Tick(Actor& self) override;
  bool CheckFire(Actor* self, sim::IFacing* facing,
                 const sim::Target& target);
  bool IsReloading() const {
    return FireDelay > 0 || IsTraitDisabled();
  }
  WVec MuzzleOffset(Actor* self, const Barrel& b);
  WRot MuzzleOrientation(Actor* self, const Barrel& b);

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<Armament>;

  struct DelayedAction {
    enum class Kind { FireProjectile, PlayAfterFireSound } kind;
    int ticks;
    int burst;
    sim::ProjectileArgs args;
    const Barrel* barrel = nullptr;
    sim::Target delayed_target_copy;

  };

  void UpdateCondition(Actor& self);
  void ScheduleDelayedAction(int t, int b,
                             DelayedAction action);

  void RunDelayedAction(Actor& self, DelayedAction& action);
  bool CanFire(Actor* self, const sim::Target& target);
  void FireBarrel(Actor* self, sim::IFacing* facing,
                  const sim::Target& target,
                  const Barrel& barrel);
  void UpdateBurst(Actor* self, const sim::Target& target);
  WVec CalculateMuzzleOffset(Actor* self, const Barrel& b);
  WRot CalculateMuzzleOrientation(Actor* self,
                                  const Barrel& b);

  ArmamentInfoData info_;
  Actor* p_self_ = nullptr;

  void* p_turret_ = nullptr;
  void* p_hovers_ = nullptr;
  class BodyOrientation* coords_ = nullptr;
  std::vector<sim::INotifyBurstComplete*> vec_notify_burst_complete_;
  std::vector<std::pair<Actor*, sim::INotifyAttack*>> vec_notify_attacks_;

  int int4_condition_token_ = Actor::InvalidConditionToken;

  std::vector<sim::IRangeModifier*> vec_range_trait_ptrs_;
  std::vector<sim::IReloadModifier*> vec_reload_trait_ptrs_;
  std::vector<sim::IFirepowerModifier*> vec_damage_trait_ptrs_;
  std::vector<sim::IInaccuracyModifier*> vec_inaccuracy_trait_ptrs_;

  int int4_ticks_since_last_shot_ = 0;
  int int4_current_barrel_ = 0;
  const int int4_barrel_count_;

  std::vector<DelayedAction> vec_delayed_actions_;
};

}
