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

class Armament;
struct Barrel;
class GrantChargedConditionOnToggle;

struct GrantConditionInfoData {
  std::string str_condition;
  bool b_grant_permanently = false;
  sim::ConditionalTraitData conditional;

  static GrantConditionInfoData Parse(const meta::RecordObject& rec);
};

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

  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  GrantConditionInfoData info_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Created(sim::Actor& self) override;

 private:
  std::string str_condition_;
  std::vector<std::string> vec_tile_sets_;
};

class GrantRandomCondition final : public sim::TraitBase,
                                   public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(
      GrantRandomCondition,
      OpenRA_Mods_Common_Traits_Conditions_GrantRandomCondition,
      sim::INotifyCreated)

  explicit GrantRandomCondition(std::vector<std::string> vec_conditions)
      : vec_conditions_{std::move(vec_conditions)} {}

  void Created(sim::Actor& self) override;

 private:
  std::vector<std::string> vec_conditions_;
};

class GrantConditionWhileAiming final : public sim::TraitBase,
                                        public sim::INotifyAiming {
 public:
  ORA_TRAIT_INTERFACES(GrantConditionWhileAiming,
                       OpenRA_Mods_Common_Traits_GrantConditionWhileAiming,
                       sim::INotifyAiming)

  explicit GrantConditionWhileAiming(std::string str_condition)
      : str_condition_{std::move(str_condition)} {}

  void StartedAiming(sim::Actor& self, sim::TraitBase* attack) override;
  void StoppedAiming(sim::Actor& self, sim::TraitBase* attack) override;

 private:
  std::string str_condition_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Created(sim::Actor& self) override;
  void DamageStateChanged(sim::Actor& self,
                          const sim::AttackInfo& e) override;

 private:
  void GrantConditionOnValidDamageState(sim::Actor& self);

  std::string str_condition_;
  std::vector<std::string> vec_enabled_sounds_;
  std::vector<std::string> vec_disabled_sounds_;
  sim::DamageState valid_damage_states_ =
      static_cast<sim::DamageState>(
          static_cast<std::int32_t>(sim::DamageState::Heavy) |
          static_cast<std::int32_t>(sim::DamageState::Critical));
  bool b_grant_permanently_ = false;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Created(sim::Actor& self) override;
  void Damaged(sim::Actor& self, const sim::AttackInfo& e) override;

  void Validate(int health_max_hp) const;

 private:
  void GrantConditionOnValidHealth(sim::Actor& self);

  std::string str_condition_;
  std::vector<std::string> vec_enabled_sounds_;
  std::vector<std::string> vec_disabled_sounds_;
  int int4_min_hp_ = 0;
  int int4_max_hp_ = 0;
  bool b_grant_permanently_ = false;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
  int int4_effective_max_hp_ = 0;
};

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

  void Tick(sim::Actor& self) override;

 private:
  std::string str_condition_;
  std::vector<std::string> vec_terrain_types_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
  std::string str_cached_terrain_;
};

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

  void Created(sim::Actor& self) override;
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

 private:
  std::string str_condition_;
  std::vector<std::string> vec_bots_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Created(sim::Actor& self) override;
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

 private:
  std::string str_condition_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Created(sim::Actor& self) override;
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void Tick(sim::Actor& self) override;

 private:
  std::string str_condition_;
  int int4_threshold_ = 0;
  class PlayerResources* ptr_player_resources_ = nullptr;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnFaction>;
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  std::string str_condition_;
  std::vector<std::string> vec_factions_;
  bool b_reset_on_owner_change_ = false;
  std::string str_faction_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Created(sim::Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void PowerLevelChanged(sim::Actor& self) override;

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnPowerState>;
  void TraitEnabledHook(sim::Actor& self) { Update(self); }
  void TraitDisabledHook(sim::Actor& self) { Update(self); }
  void Update(sim::Actor& self);

  std::string str_condition_;
  std::int32_t int4_valid_power_states_ = 2 | 4;
  class PowerManager* ptr_player_power_ = nullptr;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
  bool b_valid_power_state_ = false;
};

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
  void MovementTypeChanged(sim::Actor& self,
                           sim::MovementType types) override;

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnMovement>;
  void UpdateCondition(sim::Actor& self, sim::MovementType types);
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  std::string str_condition_;
  sim::MovementType valid_movement_types_ =
      sim::MovementType::Horizontal;
  sim::IMove* ptr_movement_ = nullptr;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

class GrantConditionOnAttack final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::ITick,
      public sim::INotifyAttack,
      private sim::ConditionalTraitCore<GrantConditionOnAttack> {
 public:
  struct InfoData {
    std::string str_condition;
    std::vector<std::string> vec_armament_names{"primary"};
    std::vector<int> vec_required_shots_per_instance{1};
    int int4_maximum_instances = 1;
    bool b_is_cyclic = false;
    int int4_revoke_delay = 15;
    bool b_revoke_on_new_target = false;
    bool b_revoke_all = false;
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
  void Tick(sim::Actor& self) override;
  void Attacking(sim::Actor& self, const sim::Target& target,
                 Armament& armament, const Barrel& barrel) override;
  void PreparingAttack(sim::Actor&, const sim::Target&,
                       Armament&, const Barrel&) override {}

  static bool TargetChanged(const sim::Target& last_target,
                            const sim::Target& target);

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnAttack>;
  void GrantInstance(sim::Actor& self);
  void RevokeInstance(sim::Actor& self, bool revoke_all);
  void TraitDisabledHook(sim::Actor& self);

  InfoData info_;
  std::vector<int> vec_tokens_;

  int int4_cooldown_ = 0;
  int int4_shots_fired_ = 0;
  sim::Target target_last_;

};

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

  void UnitProduced(sim::Actor& self, sim::Actor& other,
                    CPos exit) override;
  void Tick(sim::Actor& self) override;
  float GetValue() override;
  core::Color GetColor() override { return color_selection_bar_; }
  bool DisplayWhenEmpty() const override { return false; }

  int Ticks = 0;

 private:
  std::string str_condition_;
  std::vector<std::string> vec_actors_;

  int int4_duration_ = -1;
  bool b_show_selection_bar_ = true;
  core::Color color_selection_bar_;
  int int4_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Created(sim::Actor& self) override;
  void AddedToWorld(sim::Actor& self) override;
  void RemovedFromWorld(sim::Actor& self) override;
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void PrerequisitesUpdated(sim::Actor& self, bool available);

 private:
  std::string str_condition_;
  std::vector<std::string> vec_prerequisites_;
  class GrantConditionOnPrerequisiteManager* ptr_global_manager_ = nullptr;
  bool b_was_available_ = false;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

class GrantConditionOnPrerequisiteManager final : public sim::TraitBase,
                                                  public sim::ITechTreeElement {
 public:
  ORA_TRAIT_INTERFACES(
      GrantConditionOnPrerequisiteManager,
      OpenRA_Mods_Common_Traits_GrantConditionOnPrerequisiteManager,
      sim::ITechTreeElement)

  explicit GrantConditionOnPrerequisiteManager(sim::ActorInitializer& init);

  void Register(sim::Actor* actor, GrantConditionOnPrerequisite* u,
                const std::vector<std::string>& vec_prerequisites);

  void Unregister(sim::Actor* actor, GrantConditionOnPrerequisite* u,
                  const std::vector<std::string>& vec_prerequisites);

  void PrerequisitesAvailable(const std::string& key) override;
  void PrerequisitesUnavailable(const std::string& key) override;
  void PrerequisitesItemHidden(const std::string&) override {}
  void PrerequisitesItemVisible(const std::string&) override {}

 private:

  static std::string MakeKey(
      const std::vector<std::string>& vec_prerequisites);

  struct Entry {
    sim::Actor* actor;
    GrantConditionOnPrerequisite* upgrade;
  };
  std::map<std::string, std::vector<Entry>> map_upgradables_;
  class TechTree* ptr_tech_tree_ = nullptr;
};

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
  void Tick(sim::Actor& self) override;

 private:
  friend class sim::ConditionalTraitCore<SpreadsCondition>;
  int int4_probability_ = 5;
  WDist range_;
  std::string str_spread_condition_ = "spreading";
  int int4_delay_config_ = 5;
  int int4_delay_ = 0;
};

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
  void UnitProduced(sim::Actor& self, sim::Actor& other,
                    CPos exit) override;

 private:
  friend class sim::ConditionalTraitCore<GrantExternalConditionToProduced>;
  std::string str_condition_;
  int int4_duration_ = 0;
};

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
  void OnCrush(sim::Actor& self, sim::Actor& crusher,
               const core::BitSet<sim::CrushClass>& crush_classes) override;
  void WarnCrush(sim::Actor&, sim::Actor&,
                 const core::BitSet<sim::CrushClass>&) override {}

 private:
  friend class sim::ConditionalTraitCore<GrantExternalConditionToCrusher>;
  std::string str_condition_;
  int int4_duration_ = 0;
};

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
  void ResolveOrder(sim::Actor& self,
                    const net::Order& order) override;

  bool Enabled = false;

 private:
  friend class sim::ConditionalTraitCore<ToggleConditionOnOrder>;
  void SetCondition(sim::Actor& self, bool granted);
  void TraitDisabledHook(sim::Actor& self);
  void TraitPausedHook(sim::Actor& self);
  void TraitResumedHook(sim::Actor& self);

  std::string str_condition_;
  std::string str_order_name_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Docked(sim::Actor& self, sim::Actor& host) override;
  void Undocked(sim::Actor& self, sim::Actor& host) override;
  void Tick(sim::Actor& self) override;

  int Duration = 0;

 private:
  std::string str_condition_;
  std::vector<std::string> vec_dock_host_names_;
  int int4_after_dock_duration_ = 0;
  int int4_token_ = sim::Actor::InvalidConditionToken;
  int int4_delayed_token_ = sim::Actor::InvalidConditionToken;
};

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

  void Docked(sim::Actor& self, sim::Actor& client) override;
  void Undocked(sim::Actor& self, sim::Actor& client) override;
  void Tick(sim::Actor& self) override;

  int Duration = 0;

 private:
  std::string str_condition_;
  std::vector<std::string> vec_dock_client_names_;
  int int4_after_dock_duration_ = 0;
  int int4_token_ = sim::Actor::InvalidConditionToken;
  int int4_delayed_token_ = sim::Actor::InvalidConditionToken;
};

enum class DeployState : std::int32_t {
  Undeployed = 0,
  Deploying = 1,
  Deployed = 2,
  Undeploying = 3,
};

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
    std::string str_undeployed_condition;
    std::string str_deployed_condition;
    std::vector<std::string> vec_allowed_terrain_types;
    bool b_can_deploy_on_ramps = false;
    bool b_smart_deploy = false;
    std::optional<WAngle> opt_facing;
    std::vector<std::string> vec_deploy_sounds;
    std::vector<std::string> vec_undeploy_sounds;
    bool b_skip_make_animation = false;
    bool b_undeploy_on_move = false;
    bool b_undeploy_on_pickup = false;
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

  void Created(sim::Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  sim::Activity* WrapMove(sim::Activity* move_inner) override;
  bool TryLockForPickup(sim::Actor& self, sim::Actor& carrier) override;

  net::Order IssueDeployOrder(sim::Actor& self, bool queued) override;
  bool CanIssueDeployOrder(sim::Actor& self, bool queued) override;

  void ResolveOrder(sim::Actor& self,
                    const net::Order& order) override;
  void FinishedDeploy(sim::Actor& self) override;
  void FinishedUndeploy(sim::Actor& self) override;

  DeployState GetDeployState() const { return deploy_state_; }
  const InfoData& Info() const { return info_; }

  void Undeploy() { UndeployInner(false); }
  void Deploy() { DeployInner(false); }
  bool IsValidTerrain(CPos location);
  void DeployInner(bool init);
  void UndeployInner(bool init);

 private:
  friend class sim::ConditionalTraitCore<GrantConditionOnDeploy>;
  bool CanDeploy();
  bool IsValidTerrainType(CPos location);
  bool IsValidRampType(CPos location);
  void OnDeployStarted();
  void OnDeployCompleted();
  void OnUndeployStarted();
  void OnUndeployCompleted();

  InfoData info_;
  sim::Actor* ptr_self_ = nullptr;
  bool b_check_terrain_type_ = false;
  DeployState deploy_state_ = DeployState::Undeployed;
  int int4_deployed_token_ = sim::Actor::InvalidConditionToken;
  int int4_undeployed_token_ = sim::Actor::InvalidConditionToken;
};

class DeployForGrantedCondition final : public sim::Activity {
 public:
  DeployForGrantedCondition(sim::Actor& self, GrantConditionOnDeploy* deploy,
                            bool moving);

 protected:
  void OnFirstRun(sim::Actor& self) override;

 public:
  bool Tick(sim::Actor& self) override;

 private:
  GrantConditionOnDeploy* ptr_deploy_;
  bool b_can_turn_ = false;
  bool b_moving_ = false;
};

class DeployInner final : public sim::Activity {
 public:
  explicit DeployInner(GrantConditionOnDeploy* deployment);
  bool Tick(sim::Actor& self) override;

 private:
  GrantConditionOnDeploy* ptr_deployment_;
  bool b_initiated_ = false;
};

class ToggleChargedCondition final : public sim::Activity {
 public:
  ToggleChargedCondition(sim::Actor& self, GrantChargedConditionOnToggle* toggle)
      : ptr_toggle_{toggle} {}

 protected:
  void OnFirstRun(sim::Actor& self) override;

 private:
  GrantChargedConditionOnToggle* ptr_toggle_;
};

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
    std::string str_activated_condition;
    std::string str_charged_condition;
    int int4_initial_charge = -1;
    int int4_charge_duration = 500;
    int int4_charge_threshhold = -1;
    int int4_condition_duration = 1;
    bool b_can_cancel_condition = false;
    bool b_cancels_current_activity = false;
    core::Color color_deactivated;
    core::Color color_activated;
    bool b_display_bar_when_empty = true;
    sim::ConditionalTraitData conditional;

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

  void ResolveOrder(sim::Actor& self,
                    const net::Order& order) override;
  void Tick(sim::Actor& self) override;
  float GetValue() override;
  core::Color GetColor() override;
  bool DisplayWhenEmpty() const override;
  net::Order IssueDeployOrder(sim::Actor& self, bool queued) override;
  bool CanIssueDeployOrder(sim::Actor& self, bool queued) override;

  bool CanToggle() const;
  void ToggleState(sim::Actor& self);

  int ChargeTick = 0;

 private:
  friend class sim::ConditionalTraitCore<GrantChargedConditionOnToggle>;
  void Activate(sim::Actor& self);
  void Deactivate(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  InfoData info_;
  bool b_is_active_ = false;
  int int4_activated_token_ = sim::Actor::InvalidConditionToken;
  int int4_charged_token_ = sim::Actor::InvalidConditionToken;
  int int4_charge_threshold_ = 0;
  int int4_activated_charge_threshold_ = 0;
};

class GrantConditionOnLayer : public sim::TraitBase,
                              public sim::IObservesVariables,
                              public sim::INotifyCreated,
                              public sim::INotifyCustomLayerChanged,
                              private sim::ConditionalTraitCore<GrantConditionOnLayer> {
 public:
  GrantConditionOnLayer(std::string str_condition, std::uint8_t valid_layer,
                        sim::ConditionalTraitData conditional);
  ~GrantConditionOnLayer() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void CustomLayerChanged(sim::Actor& self, std::uint8_t old_layer,
                          std::uint8_t new_layer) override;

 protected:
  friend class sim::ConditionalTraitCore<GrantConditionOnLayer>;

  virtual void UpdateConditions(sim::Actor& self, std::uint8_t old_layer,
                                std::uint8_t new_layer);
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  std::uint8_t uint1_valid_layer_type_ = 0;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;

 private:
  std::string str_condition_;
};

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

}
