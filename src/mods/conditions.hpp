// UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/GrantCondition.cs @b6fc03f
//          (第七批 31 件:GrantCondition L17-51 / GrantRandomCondition
//          L23-46 / GrantConditionOnTileSet L19-48 / GrantConditionWhileAiming
//          L19-46 / GrantConditionOnDamageState L19-82 / GrantConditionOnHealth
//          L22-100 / GrantConditionOnTerrain L18-72 / GrantConditionOnFaction
//          L26-65 / GrantConditionOnBotOwner L19-55 / GrantConditionOnCombatantOwner
//          L19-56 / GrantConditionOnPlayerResources L19-67 / GrantConditionOnPowerState
//          L21-95 / GrantConditionOnMovement L18-58 / GrantConditionOnAttack
//          L18-167 / GrantConditionOnProduction L19-86 / GrantConditionOnPrerequisite
//          L22-90 + Player/GrantConditionOnPrerequisiteManager.cs L17-85 /
//          SpreadsCondition L19-69 / GrantExternalConditionToProduced L19-48 /
//          GrantExternalConditionToCrusher L20-58 / ToggleConditionOnOrder L28-117 /
//          GrantConditionOnClientDock L19-88 / GrantConditionOnHostDock L19-88 /
//          GrantConditionOnDeploy L38-352 / GrantChargedConditionOnToggle L51-253 +
//          Activities/DeployForGrantedCondition.cs 全文 + GrantConditionOnLayer
//          L18-63(Subterranean/Tunnel 两子类))
//          The batch-7 set of 31: every file listed above, in full.
//
// 机制对照 / Mechanism mapping:
//  - GrantConditionOnLineBuildDirection/LineBuildSegmentExternalCondition/
//    GrantConditionOnMinelaying 三件依赖 LineBuild/Minelayer 系统(未移植)
//    —— 延后至其宿主批(COVERAGE 登记)
//    The three files GrantConditionOnLineBuildDirection/
//    LineBuildSegmentExternalCondition/GrantConditionOnMinelaying depend on
//    the unported LineBuild/Minelayer systems — deferred to their host
//    batch (registered in COVERAGE).
//  - 声音/Fluent 通知面:上游 Game.Sound.Play/PlayNotification +
//    TextNotificationsManager —— 本批不构造(与 CreateEffect 批同);
//    声音数组的 CosmeticRandom(LocalRandom 域)消耗保留
//    The sound/Fluent notification faces: upstream's Game.Sound.Play/
//    PlayNotification + TextNotificationsManager — not constructed in this
//    batch (same as the CreateEffect batch); the sound arrays'
//    CosmeticRandom (the LocalRandom domain) consumption is kept.
//  - GrantConditionOnDeploy 的 notify(INotifyDeployTriggered)与
//    GrantChargedConditionOnToggle 的 DeployOrderTargeter/cursor:实现者
//    是渲染 trait/输入批 —— 空集承载;Deploy/Undeploy 的动画等待分支
//    (notify.Length == 0)即真分支
//    GrantConditionOnDeploy's notify (INotifyDeployTriggered) and
//    GrantChargedConditionOnToggle's DeployOrderTargeter/cursor: the
//    implementors are render traits/the input batch — carried as empty
//    sets; the no-animation branch of Deploy/Undeploy (notify.Length == 0)
//    is therefore the live branch.
//  - GrantConditionOnLayer 抽象基类保留虚 UpdateConditions;两子类在
//    C++ 侧仅 Info 差异(ValidLayerType 注入),类体直通基类
//    The GrantConditionOnLayer abstract base keeps the virtual
//    UpdateConditions; the two subclasses differ only in Info (the
//    ValidLayerType injection) on the C++ side, their bodies pass through
//    to the base.
#pragma once
import std;

#include "core/color.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

class Armament;   // INotifyAttack 签面前向 | the INotifyAttack forward.
struct Barrel;    // 同上 | same.
class GrantChargedConditionOnToggle;  // ToggleChargedCondition 的指向前向
                                      // ToggleChargedCondition's pointer fwd.

// ———— 简单授予面 ————
// ———— The simple grant faces ————

struct GrantConditionInfoData {
  std::string str_condition;              // L24
  bool b_grant_permanently = false;       // L27
  sim::ConditionalTraitData conditional;  // 基类段 | the base segment

  static GrantConditionInfoData Parse(const meta::RecordObject& rec);
};

/// GrantCondition(L29-51)
class GrantCondition final : public sim::TraitBase,
                             public sim::IObservesVariables,
                             public sim::INotifyCreated,
                             private sim::ConditionalTraitCore<GrantCondition> {
 public:
  ORA_TRAIT_INTERFACES(GrantCondition, OpenRA_Mods_Common_Traits_GrantCondition,
                       sim::IObservesVariables, sim::INotifyCreated)

  explicit GrantCondition(GrantConditionInfoData info)
      : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {}

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override {
    ConditionalTraitCore::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

 private:
  friend class sim::ConditionalTraitCore<GrantCondition>;
  // L33-37 TraitEnabled | L39-46 TraitDisabled
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  GrantConditionInfoData info_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnTileSet(L29-48:Created 时 Tileset 命中即授)
/// GrantConditionOnTileSet (L29-48: granted at Created when the tileset
/// matches).
class GrantConditionOnTileSet final : public sim::TraitBase,
                                      public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnTileSet,
                       OpenRA_Mods_Common_Traits_GrantConditionOnTileSet,
                       sim::INotifyCreated)

  GrantConditionOnTileSet(std::string str_condition,
                          std::vector<std::string> vec_tile_sets)
      : str_condition_{std::move(str_condition)},
        vec_tile_sets_{std::move(vec_tile_sets)} {}

  void Created(sim::Actor& self) override;  // L38-44

 private:
  std::string str_condition_;               // L26
  std::vector<std::string> vec_tile_sets_;  // L29(插入序 | insertion order)
};

/// GrantRandomCondition(L34-46:Created 时 SharedRandom 择一授予)
/// GrantRandomCondition (L34-46: one picked by SharedRandom at Created).
class GrantRandomCondition final : public sim::TraitBase,
                                   public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(
      GrantRandomCondition,
      OpenRA_Mods_Common_Traits_Conditions_GrantRandomCondition,
      sim::INotifyCreated)

  explicit GrantRandomCondition(std::vector<std::string> vec_conditions)
      : vec_conditions_{std::move(vec_conditions)} {}

  void Created(sim::Actor& self) override;  // L41-45

 private:
  std::vector<std::string> vec_conditions_;  // L26
};

/// GrantConditionWhileAiming(L24-46)
class GrantConditionWhileAiming final : public sim::TraitBase,
                                        public sim::INotifyAiming {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionWhileAiming,
                       OpenRA_Mods_Common_Traits_GrantConditionWhileAiming,
                       sim::INotifyAiming)

  explicit GrantConditionWhileAiming(std::string str_condition)
      : str_condition_{std::move(str_condition)} {}

  void StartedAiming(sim::Actor& self, sim::TraitBase* attack) override;  // L36-40
  void StoppedAiming(sim::Actor& self, sim::TraitBase* attack) override;  // L42-46

 private:
  std::string str_condition_;  // L24
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

// ———— 伤害/生命面 ————
// ———— The damage/health faces ————

/// GrantConditionOnDamageState(L31-82:伤害状态位域触发)
/// GrantConditionOnDamageState (L31-82: triggered by the damage-state
/// bit set).
class GrantConditionOnDamageState final
    : public sim::TraitBase,
      public sim::INotifyDamageStateChanged,
      public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnDamageState,
                       OpenRA_Mods_Common_Traits_GrantConditionOnDamageState,
                       sim::INotifyDamageStateChanged, sim::INotifyCreated)

  GrantConditionOnDamageState(std::string str_condition,
                              std::vector<std::string> vec_enabled_sounds,
                              std::vector<std::string> vec_disabled_sounds,
                              sim::DamageState valid_damage_states,
                              bool b_grant_permanently)
      : str_condition_{std::move(str_condition)},
        vec_enabled_sounds_{std::move(vec_enabled_sounds)},
        vec_disabled_sounds_{std::move(vec_disabled_sounds)},
        valid_damage_states_{valid_damage_states},
        b_grant_permanently_{b_grant_permanently} {}

  void Created(sim::Actor& self) override;                   // L46-49
  void DamageStateChanged(sim::Actor& self,                  // L60-81
                          const sim::AttackInfo& e) override;

 private:
  void GrantConditionOnValidDamageState(sim::Actor& self);  // L51-58

  std::string str_condition_;                     // L26
  std::vector<std::string> vec_enabled_sounds_;   // L29
  std::vector<std::string> vec_disabled_sounds_;  // L32
  sim::DamageState valid_damage_states_ =         // L35(Horizontal|Critical 位组合)
      static_cast<sim::DamageState>(              // L35 (the Heavy|Critical bit union)
          static_cast<std::int32_t>(sim::DamageState::Heavy) |
          static_cast<std::int32_t>(sim::DamageState::Critical));
  bool b_grant_permanently_ = false;              // L38
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnHealth(L38-100:HP 区间触发;MinHP > MaxHP 的
/// RulesetLoaded 校验 = 工厂时点)
/// GrantConditionOnHealth (L38-100: the HP-interval trigger; the
/// MinHP > MaxHP RulesetLoaded validation moves to factory time).
class GrantConditionOnHealth final : public sim::TraitBase,
                                     public sim::INotifyCreated,
                                     public sim::INotifyDamage {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnHealth,
                       OpenRA_Mods_Common_Traits_GrantConditionOnHealth,
                       sim::INotifyCreated, sim::INotifyDamage)

  GrantConditionOnHealth(std::string str_condition,
                         std::vector<std::string> vec_enabled_sounds,
                         std::vector<std::string> vec_disabled_sounds,
                         int min_hp, int max_hp, bool b_grant_permanently)
      : str_condition_{std::move(str_condition)},
        vec_enabled_sounds_{std::move(vec_enabled_sounds)},
        vec_disabled_sounds_{std::move(vec_disabled_sounds)},
        int4_min_hp_{min_hp}, int4_max_hp_{max_hp},
        b_grant_permanently_{b_grant_permanently} {}

  void Created(sim::Actor& self) override;                              // L65-68
  void Damaged(sim::Actor& self, const sim::AttackInfo& e) override;    // L81-99

  /// RulesetLoaded L56-62 校验(MinHP > actor MaxHP 即抛,文本逐字;
  /// 工厂时点)
  /// The RulesetLoaded L56-62 validation (MinHP above the actor MaxHP
  /// throws verbatim; factory time).
  void Validate(int health_max_hp) const;

 private:
  void GrantConditionOnValidHealth(sim::Actor& self);  // L70-78

  std::string str_condition_;                    // L25
  std::vector<std::string> vec_enabled_sounds_;  // L28
  std::vector<std::string> vec_disabled_sounds_;  // L31
  int int4_min_hp_ = 0;                          // L34
  int int4_max_hp_ = 0;                          // L37
  bool b_grant_permanently_ = false;             // L40
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
  int int4_effective_max_hp_ = 0;  // ctor L58:MaxHP>0?MaxHP:health.MaxHP | the ctor L58 resolve.
};

// ———— 地形/世界面 ————
// ———— The terrain/world faces ————

/// GrantConditionOnTerrain(L33-72)
class GrantConditionOnTerrain final : public sim::TraitBase,
                                      public sim::ITick {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnTerrain,
                       OpenRA_Mods_Common_Traits_GrantConditionOnTerrain,
                       sim::ITick)

  GrantConditionOnTerrain(std::string str_condition,
                          std::vector<std::string> vec_terrain_types)
      : str_condition_{std::move(str_condition)},
        vec_terrain_types_{std::move(vec_terrain_types)} {}

  void Tick(sim::Actor& self) override;  // L50-71

 private:
  std::string str_condition_;                   // L23
  std::vector<std::string> vec_terrain_types_;  // L26
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
  std::string str_cached_terrain_;              // L41
};

// ———— 玩家/阵营面 ————
// ———— The player/faction faces ————

/// GrantConditionOnBotOwner(L29-55)
class GrantConditionOnBotOwner final : public sim::TraitBase,
                                       public sim::INotifyCreated,
                                       public sim::INotifyOwnerChanged {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnBotOwner,
                       OpenRA_Mods_Common_Traits_GrantConditionOnBotOwner,
                       sim::INotifyCreated, sim::INotifyOwnerChanged)

  GrantConditionOnBotOwner(std::string str_condition,
                           std::vector<std::string> vec_bots)
      : str_condition_{std::move(str_condition)},
        vec_bots_{std::move(vec_bots)} {}

  void Created(sim::Actor& self) override;                       // L39-43
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,  // L45-53
                      sim::Player& new_owner) override;

 private:
  std::string str_condition_;         // L22
  std::vector<std::string> vec_bots_;  // L25
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnCombatantOwner(L31-56)
class GrantConditionOnCombatantOwner final : public sim::TraitBase,
                                             public sim::INotifyCreated,
                                             public sim::INotifyOwnerChanged {
 public:
  ORA_TRAIT_INTERFACES(
      GrantConditionOnCombatantOwner,
      OpenRA_Mods_Common_Traits_GrantConditionOnCombatantOwner,
      sim::INotifyCreated, sim::INotifyOwnerChanged)

  explicit GrantConditionOnCombatantOwner(std::string str_condition)
      : str_condition_{std::move(str_condition)} {}

  void Created(sim::Actor& self) override;                       // L39-43
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,  // L45-54
                      sim::Player& new_owner) override;

 private:
  std::string str_condition_;  // L21
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnPlayerResources(L31-67:Threshold 逐 tick 检查)
/// GrantConditionOnPlayerResources (L31-67: the per-tick Threshold check).
class GrantConditionOnPlayerResources final
    : public sim::TraitBase,
      public sim::INotifyCreated,
      public sim::INotifyOwnerChanged,
      public sim::ITick {
 public:
  ORA_TRAIT_INTERFACES(
      GrantConditionOnPlayerResources,
      OpenRA_Mods_Common_Traits_GrantConditionOnPlayerResources,
      sim::INotifyCreated, sim::INotifyOwnerChanged, sim::ITick)

  GrantConditionOnPlayerResources(std::string str_condition, int threshold)
      : str_condition_{std::move(str_condition)},
        int4_threshold_{threshold} {}

  void Created(sim::Actor& self) override;                       // L43-46
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,  // L48-51
                      sim::Player& new_owner) override;
  void Tick(sim::Actor& self) override;                          // L53-66

 private:
  std::string str_condition_;  // L21
  int int4_threshold_ = 0;     // L24
  class PlayerResources* ptr_player_resources_ = nullptr;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnFaction(L30-65)
class GrantConditionOnFaction final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::INotifyOwnerChanged,
      private sim::ConditionalTraitCore<GrantConditionOnFaction> {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnFaction,
                       OpenRA_Mods_Common_Traits_GrantConditionOnFaction,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyOwnerChanged)

  GrantConditionOnFaction(sim::ActorInitializer& init,
                          std::string str_condition,
                          std::vector<std::string> vec_factions,
                          bool b_reset_on_owner_change,
                          sim::ConditionalTraitData conditional);
  ~GrantConditionOnFaction() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,  // L43-52
                      sim::Player& new_owner) override;

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnFaction>;
  void TraitEnabledHook(sim::Actor& self);   // L54-57
  void TraitDisabledHook(sim::Actor& self);  // L59-64

  std::string str_condition_;              // L25
  std::vector<std::string> vec_factions_;  // L28
  bool b_reset_on_owner_change_ = false;   // L31
  std::string str_faction_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnPowerState(L27-95;PowerState 位域 = Normal/Low/
/// Critical 的 1/2/4)
/// GrantConditionOnPowerState (L27-95; the PowerState bits Normal/Low/
/// Critical = 1/2/4).
class GrantConditionOnPowerState final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::INotifyOwnerChanged,
      public sim::INotifyPowerLevelChanged,
      private sim::ConditionalTraitCore<GrantConditionOnPowerState> {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnPowerState,
                       OpenRA_Mods_Common_Traits_GrantConditionOnPowerState,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyOwnerChanged, sim::INotifyPowerLevelChanged)

  GrantConditionOnPowerState(std::string str_condition,
                             std::int32_t int4_valid_power_states,
                             sim::ConditionalTraitData conditional);
  ~GrantConditionOnPowerState() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override;                       // L40-43
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,  // L81-88
                      sim::Player& new_owner) override;
  void PowerLevelChanged(sim::Actor& self) override;             // L76-79

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnPowerState>;
  void TraitEnabledHook(sim::Actor& self) { Update(self); }   // L50-53
  void TraitDisabledHook(sim::Actor& self) { Update(self); }  // L55-58
  void Update(sim::Actor& self);                              // L60-74

  std::string str_condition_;  // L25
  std::int32_t int4_valid_power_states_ = 2 | 4;  // L28
  class PowerManager* ptr_player_power_ = nullptr;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
  bool b_valid_power_state_ = false;
};

// ———— 移动/攻击面 ————
// ———— The movement/attack faces ————

/// GrantConditionOnMovement(L25-58)
class GrantConditionOnMovement final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::INotifyMoving,
      private sim::ConditionalTraitCore<GrantConditionOnMovement> {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnMovement,
                       OpenRA_Mods_Common_Traits_GrantConditionOnMovement,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyMoving)

  GrantConditionOnMovement(sim::Actor& self, std::string str_condition,
                           sim::MovementType valid_movement_types,
                           sim::ConditionalTraitData conditional);
  ~GrantConditionOnMovement() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void MovementTypeChanged(sim::Actor& self,                    // L44-47
                           sim::MovementType types) override;

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnMovement>;
  void UpdateCondition(sim::Actor& self, sim::MovementType types);  // L31-41
  void TraitEnabledHook(sim::Actor& self);   // L49-52
  void TraitDisabledHook(sim::Actor& self);  // L54-57

  std::string str_condition_;                // L21
  sim::MovementType valid_movement_types_ =  // L24
      sim::MovementType::Horizontal;
  sim::IMove* ptr_movement_ = nullptr;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnAttack(L32-167:射击计数栈 + 冷却回收)
/// GrantConditionOnAttack (L32-167: the shot-count stack + the
/// cooldown reclaim).
class GrantConditionOnAttack final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::ITick,
      public sim::INotifyAttack,
      private sim::ConditionalTraitCore<GrantConditionOnAttack> {
 public:
  struct InfoData {
    std::string str_condition;               // L24
    std::vector<std::string> vec_armament_names{"primary"};  // L27
    std::vector<int> vec_required_shots_per_instance{1};     // L31
    int int4_maximum_instances = 1;          // L35
    bool b_is_cyclic = false;                // L38
    int int4_revoke_delay = 15;              // L41
    bool b_revoke_on_new_target = false;     // L44
    bool b_revoke_all = false;               // L47
    sim::ConditionalTraitData conditional;
  };

  explicit GrantConditionOnAttack(InfoData info);
  ~GrantConditionOnAttack() override;

  ORA_TRAIT_INTERFACES(GrantConditionOnAttack,
                       OpenRA_Mods_Common_Traits_GrantConditionOnAttack,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick, sim::INotifyAttack)

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitPaused() const { return ConditionalTraitCore::IsTraitPaused(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void Tick(sim::Actor& self) override;  // L68-75
  void Attacking(sim::Actor& self, const sim::Target& target,   // L108-163
                 Armament& armament, const Barrel& barrel) override;
  void PreparingAttack(sim::Actor&, const sim::Target&,      // L165
                       Armament&, const Barrel&) override {}

  /// L77-106:TargetChanged(Frozen↔Actor 同体恒 false 的上游语义)
  /// L77-106: TargetChanged (upstream's Frozen↔Actor same-body is
  /// constantly false).
  static bool TargetChanged(const sim::Target& last_target,
                            const sim::Target& target);

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnAttack>;
  void GrantInstance(sim::Actor& self);                   // L55-60
  void RevokeInstance(sim::Actor& self, bool revoke_all);  // L62-74
  void TraitDisabledHook(sim::Actor& self);               // L107→RevokeInstance(true)

  InfoData info_;
  std::vector<int> vec_tokens_;  // Stack<int> → vector(栈顶 = 尾)| a
                                 // Stack<int> as a vector (top = tail).
  int int4_cooldown_ = 0;
  int int4_shots_fired_ = 0;
  sim::Target target_last_;  // 仅 RevokeOnNewTarget 追踪 | tracked only
                             // under RevokeOnNewTarget.
};

// ———— 生产/前置面 ————
// ———— The production/prerequisite faces ————

/// GrantConditionOnProduction(L35-86)
class GrantConditionOnProduction final : public sim::TraitBase,
                                         public sim::INotifyProduction,
                                         public sim::ITick,
                                         public sim::ISync,
                                         public sim::ISelectionBar {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnProduction,
                       OpenRA_Mods_Common_Traits_GrantConditionOnProduction,
                       sim::INotifyProduction, sim::ITick, sim::ISync,
                       sim::ISelectionBar)

  GrantConditionOnProduction(std::string str_condition,
                             std::vector<std::string> vec_actors,
                             int duration, bool b_show_selection_bar,
                             core::Color selection_bar_color)
      : str_condition_{std::move(str_condition)},
        vec_actors_{std::move(vec_actors)}, int4_duration_{duration},
        b_show_selection_bar_{b_show_selection_bar},
        color_selection_bar_{selection_bar_color} {
    Ticks = duration;
  }

  void UnitProduced(sim::Actor& self, sim::Actor& other,  // L52-61
                    CPos exit) override;
  void Tick(sim::Actor& self) override;                   // L63-67
  float GetValue() override;                              // L69-76
  core::Color GetColor() override { return color_selection_bar_; }  // L77
  bool DisplayWhenEmpty() const override { return false; }  // L78

  int Ticks = 0;  // [VerifySync] L39 | the [VerifySync] member.

 private:
  std::string str_condition_;            // L21
  std::vector<std::string> vec_actors_;  // L25(小写化比较面 | the
                                         // lowercased comparison face)
  int int4_duration_ = -1;               // L28
  bool b_show_selection_bar_ = true;     // L31
  core::Color color_selection_bar_;      // L32
  int int4_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnPrerequisite(L28-90;玩家级 Manager 汇流)
/// GrantConditionOnPrerequisite (L28-90; the player-level Manager
/// funnel).
class GrantConditionOnPrerequisite final
    : public sim::TraitBase,
      public sim::INotifyCreated,
      public sim::INotifyAddedToWorld,
      public sim::INotifyRemovedFromWorld,
      public sim::INotifyOwnerChanged {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnPrerequisite,
                       OpenRA_Mods_Common_Traits_GrantConditionOnPrerequisite,
                       sim::INotifyCreated, sim::INotifyAddedToWorld,
                       sim::INotifyRemovedFromWorld, sim::INotifyOwnerChanged)

  GrantConditionOnPrerequisite(std::string str_condition,
                               std::vector<std::string> vec_prerequisites)
      : str_condition_{std::move(str_condition)},
        vec_prerequisites_{std::move(vec_prerequisites)} {}

  void Created(sim::Actor& self) override;                       // L42-45
  void AddedToWorld(sim::Actor& self) override;                  // L47-50
  void RemovedFromWorld(sim::Actor& self) override;              // L52-55
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,  // L57-60
                      sim::Player& new_owner) override;
  void PrerequisitesUpdated(sim::Actor& self, bool available);   // L62-79

 private:
  std::string str_condition_;                    // L21
  std::vector<std::string> vec_prerequisites_;   // L24
  class GrantConditionOnPrerequisiteManager* ptr_global_manager_ = nullptr;
  bool b_was_available_ = false;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnPrerequisiteManager(GrantConditionOnPrerequisiteManager.cs
/// L21-87;player actor 挂载的世界级汇流)
/// GrantConditionOnPrerequisiteManager
/// (GrantConditionOnPrerequisiteManager.cs L21-87; the player-actor-
/// mounted world funnel).
class GrantConditionOnPrerequisiteManager final : public sim::TraitBase,
                                                  public sim::ITechTreeElement {
 public:
  ORA_TRAIT_INTERFACES(
      GrantConditionOnPrerequisiteManager,
      OpenRA_Mods_Common_Traits_GrantConditionOnPrerequisiteManager,
      sim::ITechTreeElement)

  explicit GrantConditionOnPrerequisiteManager(sim::ActorInitializer& init);

  /// L40-54:Register(首次键入注册 TechTree;即时报当前状态)
  /// L40-54: Register (the first key ingress also registers with the
  /// TechTree; the current state is reported immediately).
  void Register(sim::Actor* actor, GrantConditionOnPrerequisite* u,
                const std::vector<std::string>& vec_prerequisites);

  /// L56-66:Unregister(空表移除 TechTree 键)
  /// L56-66: Unregister (an emptied list removes the TechTree key).
  void Unregister(sim::Actor* actor, GrantConditionOnPrerequisite* u,
                  const std::vector<std::string>& vec_prerequisites);

  void PrerequisitesAvailable(const std::string& key) override;    // L68-75
  void PrerequisitesUnavailable(const std::string& key) override;  // L77-84
  void PrerequisitesItemHidden(const std::string&) override {}     // L86
  void PrerequisitesItemVisible(const std::string&) override {}    // L87

 private:
  /// L32-36:MakeKey("condition_" + Order() 排序连接)
  /// L32-36: MakeKey ("condition_" + the Order()-sorted join).
  static std::string MakeKey(
      const std::vector<std::string>& vec_prerequisites);

  struct Entry {
    sim::Actor* actor;
    GrantConditionOnPrerequisite* upgrade;
  };
  std::map<std::string, std::vector<Entry>> map_upgradables_;
  class TechTree* ptr_tech_tree_ = nullptr;
};

// ———— 扩散/外部授予面 ————
// ———— The spread/external-grant faces ————

/// SpreadsCondition(L24-69:概率扩散)
/// SpreadsCondition (L24-69: the probabilistic spread).
class SpreadsCondition final : public sim::TraitBase,
                               public sim::IObservesVariables,
                               public sim::INotifyCreated,
                               public sim::ITick,
                               private sim::ConditionalTraitCore<SpreadsCondition> {
 public:
  ORA_TRAIT_INTERFACES(SpreadsCondition,
                       OpenRA_Mods_Common_Traits_SpreadsCondition,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick)

  SpreadsCondition(int probability, WDist range, std::string spread_condition,
                   int delay, sim::ConditionalTraitData conditional);
  ~SpreadsCondition() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void Tick(sim::Actor& self) override;  // L42-67

 private:
  friend class sim::ConditionalTraitCore<SpreadsCondition>;
  int int4_probability_ = 5;                          // L20
  WDist range_;                                       // L23
  std::string str_spread_condition_ = "spreading";    // L26
  int int4_delay_config_ = 5;                         // L29
  int int4_delay_ = 0;                                // L37
};

/// GrantExternalConditionToProduced(L31-48:出厂授予外部条件)
/// GrantExternalConditionToProduced (L31-48: the on-produce external
/// grant).
class GrantExternalConditionToProduced final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::INotifyProduction,
      private sim::ConditionalTraitCore<GrantExternalConditionToProduced> {
 public:
  ORA_TRAIT_INTERFACES(
      GrantExternalConditionToProduced,
      OpenRA_Mods_Common_Traits_GrantExternalConditionToProduced,
      sim::IObservesVariables, sim::INotifyCreated, sim::INotifyProduction)

  GrantExternalConditionToProduced(std::string str_condition, int duration,
                                   sim::ConditionalTraitData conditional);
  ~GrantExternalConditionToProduced() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void UnitProduced(sim::Actor& self, sim::Actor& other,  // L37-47
                    CPos exit) override;

 private:
  friend class sim::ConditionalTraitCore<GrantExternalConditionToProduced>;
  std::string str_condition_;  // L21
  int int4_duration_ = 0;      // L24
};

/// GrantExternalConditionToCrusher(L25-58:碾压授予外部条件)
/// GrantExternalConditionToCrusher (L25-58: the on-crush external grant).
class GrantExternalConditionToCrusher final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::INotifyCrushed,
      private sim::ConditionalTraitCore<GrantExternalConditionToCrusher> {
 public:
  ORA_TRAIT_INTERFACES(
      GrantExternalConditionToCrusher,
      OpenRA_Mods_Common_Traits_GrantExternalConditionToCrusher,
      sim::IObservesVariables, sim::INotifyCreated, sim::INotifyCrushed)

  GrantExternalConditionToCrusher(std::string str_condition, int duration,
                                  sim::ConditionalTraitData conditional);
  ~GrantExternalConditionToCrusher() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnCrush(sim::Actor& self, sim::Actor& crusher,  // L31-40
               const core::BitSet<sim::CrushClass>& crush_classes) override;
  void WarnCrush(sim::Actor&, sim::Actor&,               // L42-47(通知面)
                 const core::BitSet<sim::CrushClass>&) override {}

 private:
  friend class sim::ConditionalTraitCore<GrantExternalConditionToCrusher>;
  std::string str_condition_;  // L21
  int int4_duration_ = 0;      // L24
};

// ———— 开关/部署面 ————
// ———— The toggle/deploy faces ————

/// ToggleConditionOnOrder(L42-117:order 切换 + pause 状态保持)
/// ToggleConditionOnOrder (L42-117: the order toggle + the paused-state
/// retention).
class ToggleConditionOnOrder final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::IResolveOrder,
      public sim::ISync,
      private sim::ConditionalTraitCore<ToggleConditionOnOrder> {
 public:
  ORA_TRAIT_INTERFACES(ToggleConditionOnOrder,
                       OpenRA_Mods_Common_Traits_ToggleConditionOnOrder,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::IResolveOrder, sim::ISync)

  ToggleConditionOnOrder(std::string str_condition,
                         std::string str_order_name,
                         sim::ConditionalTraitData conditional);
  ~ToggleConditionOnOrder() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitPaused() const { return ConditionalTraitCore::IsTraitPaused(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void ResolveOrder(sim::Actor& self,  // L104-111
                    const net::Order& order) override;

  bool Enabled = false;  // [VerifySync] L48 | the [VerifySync] member.

 private:
  friend class sim::ConditionalTraitCore<ToggleConditionOnOrder>;
  void SetCondition(sim::Actor& self, bool granted);  // L56-84(声音面省略)
  void TraitDisabledHook(sim::Actor& self);           // L113-116
  void TraitPausedHook(sim::Actor& self);             // L118-121
  void TraitResumedHook(sim::Actor& self);            // L123-126

  std::string str_condition_;   // L30
  std::string str_order_name_;  // L33
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnClientDock(L27-88)
class GrantConditionOnClientDock final : public sim::TraitBase,
                                         public sim::INotifyDockClient,
                                         public sim::ITick,
                                         public sim::ISync {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnClientDock,
                       OpenRA_Mods_Common_Traits_GrantConditionOnClientDock,
                       sim::INotifyDockClient, sim::ITick, sim::ISync)

  GrantConditionOnClientDock(std::string str_condition,
                             std::vector<std::string> vec_dock_host_names,
                             int after_dock_duration)
      : str_condition_{std::move(str_condition)},
        vec_dock_host_names_{std::move(vec_dock_host_names)},
        int4_after_dock_duration_{after_dock_duration} {}

  void Docked(sim::Actor& self, sim::Actor& host) override;     // L42-58
  void Undocked(sim::Actor& self, sim::Actor& host) override;   // L60-74
  void Tick(sim::Actor& self) override;                         // L76-79

  int Duration = 0;  // [VerifySync] L33 | the [VerifySync] member.

 private:
  std::string str_condition_;                      // L19
  std::vector<std::string> vec_dock_host_names_;   // L25(null = 全部 | null = all)
  int int4_after_dock_duration_ = 0;               // L22
  int int4_token_ = sim::Actor::InvalidConditionToken;
  int int4_delayed_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnHostDock(L27-88 的 Host 面)
/// GrantConditionOnHostDock (L27-88's Host face).
class GrantConditionOnHostDock final : public sim::TraitBase,
                                       public sim::INotifyDockHost,
                                       public sim::ITick,
                                       public sim::ISync {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnHostDock,
                       OpenRA_Mods_Common_Traits_GrantConditionOnHostDock,
                       sim::INotifyDockHost, sim::ITick, sim::ISync)

  GrantConditionOnHostDock(std::string str_condition,
                           std::vector<std::string> vec_dock_client_names,
                           int after_dock_duration)
      : str_condition_{std::move(str_condition)},
        vec_dock_client_names_{std::move(vec_dock_client_names)},
        int4_after_dock_duration_{after_dock_duration} {}

  void Docked(sim::Actor& self, sim::Actor& client) override;    // L42-58
  void Undocked(sim::Actor& self, sim::Actor& client) override;  // L60-74
  void Tick(sim::Actor& self) override;                          // L76-79

  int Duration = 0;  // [VerifySync] | the [VerifySync] member.

 private:
  std::string str_condition_;                       // L19
  std::vector<std::string> vec_dock_client_names_;  // L25
  int int4_after_dock_duration_ = 0;                // L22
  int int4_token_ = sim::Actor::InvalidConditionToken;
  int int4_delayed_token_ = sim::Actor::InvalidConditionToken;
};

/// GrantConditionOnDeploy.cs L105:DeployState
enum class DeployState : std::int32_t {
  Undeployed = 0,
  Deploying = 1,
  Deployed = 2,
  Undeploying = 3,
};

/// GrantConditionOnDeploy(L88-345)
class GrantConditionOnDeploy final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::IResolveOrder,
      public sim::INotifyDeployComplete,
      public sim::IIssueDeployOrder,
      public sim::IWrapMove,
      public sim::IDelayCarryallPickup,
      private sim::ConditionalTraitCore<GrantConditionOnDeploy> {
 public:
  struct InfoData {
    std::string str_undeployed_condition;                // L44
    std::string str_deployed_condition;                  // L47
    std::vector<std::string> vec_allowed_terrain_types;  // L50
    bool b_can_deploy_on_ramps = false;                  // L53
    bool b_smart_deploy = false;                         // L56
    std::optional<WAngle> opt_facing;                    // L68
    std::vector<std::string> vec_deploy_sounds;          // L71
    std::vector<std::string> vec_undeploy_sounds;        // L74
    bool b_skip_make_animation = false;                  // L77
    bool b_undeploy_on_move = false;                     // L80
    bool b_undeploy_on_pickup = false;                   // L83
    sim::ConditionalTraitData conditional;
  };

  GrantConditionOnDeploy(sim::ActorInitializer& init, InfoData info);
  ~GrantConditionOnDeploy() override;

  ORA_TRAIT_INTERFACES(GrantConditionOnDeploy,
                       OpenRA_Mods_Common_Traits_GrantConditionOnDeploy,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::IResolveOrder, sim::INotifyDeployComplete,
                       sim::IIssueDeployOrder, sim::IWrapMove,
                       sim::IDelayCarryallPickup)

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitPaused() const { return ConditionalTraitCore::IsTraitPaused(); }

  void Created(sim::Actor& self) override;  // L120-149
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  sim::Activity* WrapMove(sim::Activity* move_inner) override;  // L151-160
  bool TryLockForPickup(sim::Actor& self, sim::Actor& carrier) override;  // L162-172

  net::Order IssueDeployOrder(sim::Actor& self, bool queued) override;  // L222-225
  bool CanIssueDeployOrder(sim::Actor& self, bool queued) override;     // L227-250

  void ResolveOrder(sim::Actor& self,  // L252-260
                    const net::Order& order) override;
  void FinishedDeploy(sim::Actor& self) override;   // L291-294
  void FinishedUndeploy(sim::Actor& self) override;  // L296-299

  DeployState GetDeployState() const { return deploy_state_; }
  const InfoData& Info() const { return info_; }

  /// L271-276:Undeploy/Deploy 公共入口 | the public entries.
  void Undeploy() { UndeployInner(false); }
  void Deploy() { DeployInner(false); }
  bool IsValidTerrain(CPos location);  // L262-274
  void DeployInner(bool init);         // L277-302
  void UndeployInner(bool init);       // L303-326

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnDeploy>;
  bool CanDeploy();                        // L262-266
  bool IsValidTerrainType(CPos location);  // L275-285
  bool IsValidRampType(CPos location);     // L287-291
  void OnDeployStarted();                  // L328-334
  void OnDeployCompleted();                // L336-341
  void OnUndeployStarted();                // L343-347
  void OnUndeployCompleted();              // L349-355

  InfoData info_;
  sim::Actor* ptr_self_ = nullptr;
  bool b_check_terrain_type_ = false;
  DeployState deploy_state_ = DeployState::Undeployed;
  int int4_deployed_token_ = sim::Actor::InvalidConditionToken;
  int int4_undeployed_token_ = sim::Actor::InvalidConditionToken;
};

/// DeployForGrantedCondition(DeployForGrantedCondition.cs L17-66)+
/// DeployInner(L68-87)活动对
/// The DeployForGrantedCondition (DeployForGrantedCondition.cs L17-66)
/// + DeployInner (L68-87) activity pair.
class DeployForGrantedCondition final : public sim::Activity {
 public:
  DeployForGrantedCondition(sim::Actor& self, GrantConditionOnDeploy* deploy,
                            bool moving);

 protected:
  void OnFirstRun(sim::Actor& self) override;  // L33-37

 public:
  bool Tick(sim::Actor& self) override;  // L39-51

 private:
  GrantConditionOnDeploy* ptr_deploy_;
  bool b_can_turn_ = false;
  bool b_moving_ = false;
};

class DeployInner final : public sim::Activity {
 public:
  explicit DeployInner(GrantConditionOnDeploy* deployment);
  bool Tick(sim::Actor& self) override;  // L77-87

 private:
  GrantConditionOnDeploy* ptr_deployment_;
  bool b_initiated_ = false;
};

/// ToggleChargedCondition(GrantChargedConditionOnToggle.cs L217-242)
class ToggleChargedCondition final : public sim::Activity {
 public:
  ToggleChargedCondition(sim::Actor& self, GrantChargedConditionOnToggle* toggle)
      : ptr_toggle_{toggle} {}

 protected:
  void OnFirstRun(sim::Actor& self) override;  // L232-236

 private:
  GrantChargedConditionOnToggle* ptr_toggle_;
};

/// GrantChargedConditionOnToggle(L103-253;chargeTick [VerifySync])
/// GrantChargedConditionOnToggle (L103-253; chargeTick [VerifySync]).
class GrantChargedConditionOnToggle final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::IResolveOrder,
      public sim::ITick,
      public sim::ISelectionBar,
      public sim::ISync,
      public sim::IIssueDeployOrder,
      private sim::ConditionalTraitCore<GrantChargedConditionOnToggle> {
 public:
  struct InfoData {
    std::string str_activated_condition;      // L55
    std::string str_charged_condition;        // L58
    int int4_initial_charge = -1;             // L61
    int int4_charge_duration = 500;           // L64
    int int4_charge_threshhold = -1;          // L70
    int int4_condition_duration = 1;          // L76
    bool b_can_cancel_condition = false;      // L79
    bool b_cancels_current_activity = false;  // L82
    core::Color color_deactivated;            // L94
    core::Color color_activated;              // L97
    bool b_display_bar_when_empty = true;     // L100
    sim::ConditionalTraitData conditional;

    /// RulesetLoaded L92-101 校验(工厂时点)
    /// The RulesetLoaded L92-101 validation (factory time).
    void Validate() const;
  };

  explicit GrantChargedConditionOnToggle(InfoData info);
  ~GrantChargedConditionOnToggle() override;

  ORA_TRAIT_INTERFACES(GrantChargedConditionOnToggle,
                       OpenRA_Mods_Common_Traits_GrantChargedConditionOnToggle,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::IResolveOrder, sim::ITick, sim::ISelectionBar,
                       sim::ISync, sim::IIssueDeployOrder)

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitPaused() const { return ConditionalTraitCore::IsTraitPaused(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void ResolveOrder(sim::Actor& self,  // L173-183
                    const net::Order& order) override;
  void Tick(sim::Actor& self) override;  // L205-238
  float GetValue() override;             // L240-247
  core::Color GetColor() override;       // L248
  bool DisplayWhenEmpty() const override;  // L249
  net::Order IssueDeployOrder(sim::Actor& self, bool queued) override;  // L161-164
  bool CanIssueDeployOrder(sim::Actor& self, bool queued) override;     // L166

  bool CanToggle() const;          // L185
  void ToggleState(sim::Actor& self);  // L187-201

  int ChargeTick = 0;  // [VerifySync] L107 | the [VerifySync] member.

 private:
  friend class sim::ConditionalTraitCore<GrantChargedConditionOnToggle>;
  void Activate(sim::Actor& self);            // L203-212
  void Deactivate(sim::Actor& self);          // L214-222
  void TraitDisabledHook(sim::Actor& self);   // L140-153

  InfoData info_;
  bool b_is_active_ = false;
  int int4_activated_token_ = sim::Actor::InvalidConditionToken;
  int int4_charged_token_ = sim::Actor::InvalidConditionToken;
  int int4_charge_threshold_ = 0;             // ctor 缓存 | the ctor cache.
  int int4_activated_charge_threshold_ = 0;   // ctor 缓存 | the ctor cache.
};

// ———— 自定义层触发面 ————
// ———— The custom-layer trigger faces ————

/// GrantConditionOnLayer(L21-63 抽象基类;CustomLayerChanged 的 byte 层
/// 触发面)
/// GrantConditionOnLayer (the L21-63 abstract base; the byte-layer
/// trigger face of CustomLayerChanged).
class GrantConditionOnLayer : public sim::TraitBase,
                              public sim::IObservesVariables,
                              public sim::INotifyCreated,
                              public sim::INotifyCustomLayerChanged,
                              private sim::ConditionalTraitCore<GrantConditionOnLayer> {
 public:
  GrantConditionOnLayer(std::string str_condition, std::uint8_t valid_layer,
                        sim::ConditionalTraitData conditional);
  ~GrantConditionOnLayer() override;

  // 泛型基类不作注册键(上游 GrantConditionOnLayer<InfoType> 的 Type 键
  // 无查询点);子类自持 {self + 基类接口集} 完整表
  // The generic base serves as no registration key (the upstream
  // GrantConditionOnLayer<InfoType> Type key has no query sites); each
  // subclass carries its own full {self + the base's interface set}
  // table.

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void CustomLayerChanged(sim::Actor& self, std::uint8_t old_layer,  // L30-33
                          std::uint8_t new_layer) override;

 protected:
  friend class sim::ConditionalTraitCore<GrantConditionOnLayer>;
  /// L35-44:UpdateConditions(子类覆写面 —— 上游虚方法)
  /// L35-44: UpdateConditions (the subclass override face — an upstream
  /// virtual).
  virtual void UpdateConditions(sim::Actor& self, std::uint8_t old_layer,
                                std::uint8_t new_layer);
  void TraitEnabledHook(sim::Actor& self);  // L46-50
  void TraitDisabledHook(sim::Actor& self);  // L52-56(Revoke)

  std::uint8_t uint1_valid_layer_type_ = 0;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;

 private:
  std::string str_condition_;  // L18(基类字段 | the base field)
};

/// GrantConditionOnSubterraneanLayer(GrantConditionOnSubterraneanLayer.cs
/// L18-24:ValidLayerType = Index;接口表补齐基类全集 —— Classic 子类
/// upcast 漏基类接口集的既有真问题防线)
/// GrantConditionOnSubterraneanLayer
/// (GrantConditionOnSubterraneanLayer.cs L18-24: ValidLayerType = Index;
/// the interface table re-lists the base's full set — the guard against
/// the known Classic-subclass upcast omission).
class GrantConditionOnSubterraneanLayer final : public GrantConditionOnLayer {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionOnSubterraneanLayer,
                       OpenRA_Mods_Common_Traits_GrantConditionOnSubterraneanLayer,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyCustomLayerChanged)
  GrantConditionOnSubterraneanLayer(
      std::string str_condition, std::uint8_t valid_layer,
      sim::ConditionalTraitData conditional)
      : GrantConditionOnLayer{std::move(str_condition), valid_layer,
                              conditional} {}
};

/// GrantConditionOnTunnelLayer(GrantConditionOnTunnelLayer.cs L18-24)
class GrantConditionOnTunnelLayer final : public GrantConditionOnLayer {
 public:
  ORA_TRAIT_INTERFACES(
      GrantConditionOnTunnelLayer,
      OpenRA_Mods_Common_Traits_GrantConditionOnTunnelLayer,
      sim::IObservesVariables, sim::INotifyCreated,
      sim::INotifyCustomLayerChanged)
  GrantConditionOnTunnelLayer(std::string str_condition,
                              std::uint8_t valid_layer,
                              sim::ConditionalTraitData conditional)
      : GrantConditionOnLayer{std::move(str_condition), valid_layer,
                              conditional} {}
};

}  // namespace ora::mods
