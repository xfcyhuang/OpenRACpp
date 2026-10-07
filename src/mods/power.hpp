// UPSTREAM: OpenRA.Mods.Common/Traits/Power/Player/PowerManager.cs @b6fc03f
//          L17-233 全文 + Power.cs L17-57 全文 + Power/AffectedByPowerOutage.cs
//          L17-83 全文(逐语义重写;声音/文本通知面与 FluentReference 随
//          Phase 6/7 注入面)
//          The whole of Power/Player/PowerManager.cs L17-233 + the whole
//          of Power.cs L17-57 + the whole of Power/AffectedByPowerOutage.cs
//          L17-83 (verbatim-semantics rewrites; the sound/text
//          notification faces and the FluentReference ride the Phase 6/7
//          injection faces).
//
// 机制对照 / Mechanism mapping:
//  - powerDrain 的 Dictionary<Actor, int> → std::map<Actor*, int>(键序
//    稳定;Tick 的重建循环只做和,序不可观测)
//    powerDrain's Dictionary<Actor, int> → std::map<Actor*, int> (a
//    stable key order; the Tick rebuild loop only sums, so the order is
//    unobservable).
//  - 低电通知(Game.RunTime/Sound/TextNotificationsManager)未接:纯
//    unsynced 通知面(COVERAGE 登记;同 PlayerResources 的低资通知)
//    The low-power notification (Game.RunTime/Sound/
//    TextNotificationsManager) is unwired: a purely unsynced notification
//    face (registered in COVERAGE; the same as PlayerResources'
//    low-funds notification).
//  - AffectedByPowerOutage 的 ISelectionBar(UI 面)随 Phase 6
//    AffectedByPowerOutage's ISelectionBar (a UI face) rides Phase 6.
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

/// PowerManagerInfo 的解析面(L22-35)
/// The parsed face of PowerManagerInfo (L22-35).
struct PowerManagerInfoData {
  int int4_advice_interval = 10000;  // L24
  std::string str_speech_notification;  // L29
  std::string str_text_notification;    // L32

  static PowerManagerInfoData Parse(const meta::RecordObject& rec_info);
};

/// PowerManager(L37-232;player actor 挂载)
/// PowerManager (L37-232; mounted on the player actor).
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

  // ———— [VerifySync] L52-55 ————
  int PowerProvided = 0;
  int PowerDrained = 0;

  /// L57:ExcessPower | L57: ExcessPower.
  int ExcessPower() const { return PowerProvided - PowerDrained; }

  /// L59-61:PowerOutageRemainingTicks/TotalTicks/PlayLowPowerNotification
  /// L59-61: PowerOutageRemainingTicks/TotalTicks/
  /// PlayLowPowerNotification.
  int PowerOutageRemainingTicks = 0;
  int PowerOutageTotalTicks = 0;
  bool PlayLowPowerNotification = false;

  /// L86-112:UpdateActor | L86-112: UpdateActor.
  void UpdateActor(Actor& a);

  /// L114-133:RemoveActor | L114-133: RemoveActor.
  void RemoveActor(Actor& a);

  /// L183-195:PowerState | L183-195: PowerState.
  sim::PowerState GetPowerState() const {
    if (PowerProvided >= PowerDrained)
      return sim::PowerState::Normal;
    if (PowerProvided > PowerDrained / 2)
      return sim::PowerState::Low;
    return sim::PowerState::Critical;
  }

  /// L197-201:TriggerPowerOutage | L197-201: TriggerPowerOutage.
  void TriggerPowerOutage(int total_ticks);

  // ———— 接口面 ————
  void Created(Actor& self) override;
  void Tick(Actor& self) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

 private:
  /// L135-147:UpdatePowerState | L135-147: UpdatePowerState.
  void UpdatePowerState();

  /// L203-210:UpdatePowerOutageActors
  /// L203-210: UpdatePowerOutageActors.
  void UpdatePowerOutageActors();

  /// L212-219:UpdatePowerRequiringActors
  /// L212-219: UpdatePowerRequiringActors.
  void UpdatePowerRequiringActors();

  Actor* p_self_ = nullptr;             // L45
  PowerManagerInfoData info_;           // L46
  DeveloperMode* p_dev_mode_ = nullptr; // L47

  std::map<Actor*, int> map_power_drain_;  // L49

  long long int8_last_power_advice_time = 0;  // L63
  bool b_is_low_power = false;                // L64
  bool b_was_low_power = false;               // L65
  bool b_was_hack_enabled = false;            // L66
};

/// PowerInfo 的解析面(Power.cs L18-25)
/// The parsed face of PowerInfo (Power.cs L18-25).
struct PowerInfoData {
  int int4_amount = 0;  // L21
  sim::ConditionalTraitData conditional;

  static PowerInfoData Parse(const meta::RecordObject& rec_info);
};

/// Power(Power.cs L26-56)
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

  /// L32-35:GetEnabledPower(IPowerModifier 链 = OPT-A1 修正链)
  /// L32-35: GetEnabledPower (the IPowerModifier chain = the OPT-A1
  /// modifier chain).
  int GetEnabledPower();

  PowerManager* PlayerPower() const { return p_player_power_; }

  // 条件核四钩子(L44-45:TraitEnabled/TraitDisabled → UpdateActor)
  // The condition core's four hooks (L44-45: TraitEnabled/TraitDisabled →
  // UpdateActor).
  void TraitEnabledHook(Actor& self);
  void TraitDisabledHook(Actor& self);
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

  const PowerInfoData& Info() const { return info_; }

 private:
  friend class sim::ConditionalTraitCore<Power>;

  PowerInfoData info_;
  Actor* p_self_ = nullptr;  // Lazy 体捕获的 self(修正 trait 解析面)
  PowerManager* p_player_power_ = nullptr;
  std::vector<sim::IPowerModifier*> vec_power_modifiers_;  // Lazy 首查物化
  bool b_modifiers_resolved_ = false;
  std::vector<std::int32_t> vec_percentages_buffer_;  // OPT-A9 修正链缓冲
};

/// AffectedByPowerOutageInfo 的解析面(AffectedByPowerOutage.cs L18-25)
/// The parsed face of AffectedByPowerOutageInfo (L18-25).
struct AffectedByPowerOutageInfoData {
  std::string str_condition;  // L22
  sim::ConditionalTraitData conditional;

  static AffectedByPowerOutageInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// AffectedByPowerOutage(AffectedByPowerOutage.cs L27-83;ISelectionBar
/// 的 UI 面随 Phase 6)
/// AffectedByPowerOutage (L27-83; the ISelectionBar UI face rides
/// Phase 6).
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

  /// L57-63:UpdateStatus | L57-63: UpdateStatus.
  void UpdateStatus(Actor& self);

  void TraitEnabledHook(Actor& self) { UpdateStatus(self); }
  void TraitDisabledHook(Actor& self) { Revoke(self); }
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  void AddedToWorld(Actor& self) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

 private:
  friend class sim::ConditionalTraitCore<AffectedByPowerOutage>;

  /// L65-69:Grant | L65-69: Grant.
  void Grant(Actor& self);

  /// L71-75:Revoke | L71-75: Revoke.
  void Revoke(Actor& self);

  AffectedByPowerOutageInfoData info_;
  PowerManager* p_player_power_ = nullptr;
  int int4_token_ = Actor::InvalidConditionToken;
};

}  // namespace ora::mods
