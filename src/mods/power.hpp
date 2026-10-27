#pragma once
import std;

#include "core/percent_modifiers.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/player.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

class PowerManager;
class DeveloperMode;

using sim::Actor;
using sim::ActorInitializer;
using sim::Player;
using sim::TraitBase;

struct PowerManagerInfoData {
  int int4_advice_interval = 10000;
  std::string str_speech_notification;
  std::string str_text_notification;

  static PowerManagerInfoData Parse(const meta::RecordObject& rec_info);
};

class PowerManager final : public TraitBase,
                           public sim::INotifyCreated,
                           public sim::ITick,
                           public sim::ISync,
                           public sim::IResolveOrder {
 public:
  PowerManager(ActorInitializer& init, const PowerManagerInfoData& info);

  ORA_TRAIT_INTERFACES(PowerManager, OpenRA_Mods_Common_Traits_PowerManager,
                       sim::INotifyCreated, sim::ITick, sim::ISync,
                       sim::IResolveOrder)

  int PowerProvided = 0;
  int PowerDrained = 0;

  int ExcessPower() const { return PowerProvided - PowerDrained; }

  int PowerOutageRemainingTicks = 0;
  int PowerOutageTotalTicks = 0;
  bool PlayLowPowerNotification = false;

  void UpdateActor(Actor& a);

  void RemoveActor(Actor& a);

  sim::PowerState GetPowerState() const {
    if (PowerProvided >= PowerDrained)
      return sim::PowerState::Normal;
    if (PowerProvided > PowerDrained / 2)
      return sim::PowerState::Low;
    return sim::PowerState::Critical;
  }

  void TriggerPowerOutage(int total_ticks);

  void Created(Actor& self) override;
  void Tick(Actor& self) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

 private:

  void UpdatePowerState();

  void UpdatePowerOutageActors();

  void UpdatePowerRequiringActors();

  Actor* p_self_ = nullptr;
  PowerManagerInfoData info_;
  DeveloperMode* p_dev_mode_ = nullptr;

  std::map<Actor*, int> map_power_drain_;

  long long int8_last_power_advice_time = 0;
  bool b_is_low_power = false;
  bool b_was_low_power = false;
  bool b_was_hack_enabled = false;
};

struct PowerInfoData {
  int int4_amount = 0;
  sim::ConditionalTraitData conditional;

  static PowerInfoData Parse(const meta::RecordObject& rec_info);
};

class Power final : public TraitBase,
                    public sim::ConditionalTraitCore<Power>,
                    public sim::IObservesVariables,
                    public sim::INotifyCreated,
                    public sim::INotifyAddedToWorld,
                    public sim::INotifyRemovedFromWorld,
                    public sim::INotifyOwnerChanged {
 public:
  Power(ActorInitializer& init, const PowerInfoData& info);

  ORA_TRAIT_INTERFACES(Power, OpenRA_Mods_Common_Traits_Power,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyAddedToWorld,
                       sim::INotifyRemovedFromWorld, sim::INotifyOwnerChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Power>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Power>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  int GetEnabledPower();

  PowerManager* PlayerPower() const { return p_player_power_; }

  void TraitEnabledHook(Actor& self);
  void TraitDisabledHook(Actor& self);
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

  const PowerInfoData& Info() const { return info_; }

 private:
  friend class sim::ConditionalTraitCore<Power>;

  PowerInfoData info_;
  Actor* p_self_ = nullptr;
  PowerManager* p_player_power_ = nullptr;
  std::vector<sim::IPowerModifier*> vec_power_modifiers_;
  bool b_modifiers_resolved_ = false;
  std::vector<std::int32_t> vec_percentages_buffer_;
};

struct AffectedByPowerOutageInfoData {
  std::string str_condition;
  sim::ConditionalTraitData conditional;

  static AffectedByPowerOutageInfoData Parse(
      const meta::RecordObject& rec_info);
};

class AffectedByPowerOutage final
    : public TraitBase,
      public sim::ConditionalTraitCore<AffectedByPowerOutage>,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::INotifyAddedToWorld,
      public sim::INotifyOwnerChanged {
 public:
  AffectedByPowerOutage(ActorInitializer& init,
                        const AffectedByPowerOutageInfoData& info);

  ORA_TRAIT_INTERFACES(
      AffectedByPowerOutage,
      OpenRA_Mods_Common_Traits_AffectedByPowerOutage, sim::IObservesVariables,
      sim::INotifyCreated, sim::INotifyAddedToWorld,
      sim::INotifyOwnerChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<
        AffectedByPowerOutage>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<
        AffectedByPowerOutage>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void UpdateStatus(Actor& self);

  void TraitEnabledHook(Actor& self) { UpdateStatus(self); }
  void TraitDisabledHook(Actor& self) { Revoke(self); }
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  void AddedToWorld(Actor& self) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

 private:
  friend class sim::ConditionalTraitCore<AffectedByPowerOutage>;

  void Grant(Actor& self);

  void Revoke(Actor& self);

  AffectedByPowerOutageInfoData info_;
  PowerManager* p_player_power_ = nullptr;
  int int4_token_ = Actor::InvalidConditionToken;
};

}
