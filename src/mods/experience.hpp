#pragma once
import std;

#include "meta/generic_record.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

struct GainsExperienceInfoData {

  std::vector<std::pair<int, std::string>> vec_conditions;
  std::string str_level_up_image;
  std::string str_level_up_sequence = "levelup";
  std::string str_level_up_palette = "effect";
  int int4_experience_modifier = -1;
  bool b_suppress_levelup_animation = true;
  std::string str_level_up_notification;

  static GainsExperienceInfoData Parse(const meta::RecordObject& rec);
};

class GainsExperience final : public sim::TraitBase,
                              public sim::INotifyCreated,
                              public sim::ISync,
                              public sim::IResolveOrder,
                              public sim::ITransformActorInitModifier {
 public:
  static constexpr std::string_view OrderName = "DevLevelUp";

  ORA_TRAIT_INTERFACES(GainsExperience,
                       OpenRA_Mods_Common_Traits_GainsExperience,
                       sim::INotifyCreated, sim::ISync,
                       sim::IResolveOrder, sim::ITransformActorInitModifier)

  GainsExperience(sim::ActorInitializer& init, GainsExperienceInfoData info);
  ~GainsExperience() override;

  bool CanGainLevel() const { return Level < int4_max_level_; }

  void GiveLevels(int num_levels, bool silent = false);

  void GiveExperience(int amount, bool silent = false);

  void Created(sim::Actor& self) override;
  void ResolveOrder(sim::Actor& self,
                    const net::Order& order) override;
  void ModifyTransformActorInit(
      sim::Actor& self, sim::TypeDictionary& init) override;

  int Experience = 0;
  int Level = 0;

 private:
  sim::Actor* ptr_self_ = nullptr;
  GainsExperienceInfoData info_;
  int int4_initial_experience_ = 0;
  int int4_max_level_ = 0;

  std::vector<std::pair<int, std::string>> vec_next_level_;
};

struct GivesExperienceInfoData {
  int int4_experience = -1;
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Neutral | sim::PlayerRelationship::Enemy;
  int int4_actor_experience_modifier = 10000;
  int int4_player_experience_modifier = 0;

  static GivesExperienceInfoData Parse(const meta::RecordObject& rec);
};

class GivesExperience final : public sim::TraitBase,
                              public sim::INotifyCreated,
                              public sim::INotifyKilled {
 public:
  ORA_TRAIT_INTERFACES(GivesExperience,
                       OpenRA_Mods_Common_Traits_GivesExperience,
                       sim::INotifyCreated, sim::INotifyKilled)

  explicit GivesExperience(GivesExperienceInfoData info);
  ~GivesExperience() override;

  void Created(sim::Actor& self) override;
  void Killed(sim::Actor& self, const sim::AttackInfo& e) override;

 private:
  GivesExperienceInfoData info_;
  int int4_exp_ = 0;
  std::vector<int> vec_experience_modifiers_;

};

class GainsExperienceMultiplier final
    : public sim::TraitBase,
      public sim::IObservesVariables,
      public sim::INotifyCreated,
      public sim::IGainsExperienceModifier,
      private sim::ConditionalTraitCore<GainsExperienceMultiplier> {
 public:
  ORA_TRAIT_INTERFACES(
      GainsExperienceMultiplier,
      OpenRA_Mods_Common_Traits_GainsExperienceMultiplier,
      sim::IObservesVariables, sim::INotifyCreated,
      sim::IGainsExperienceModifier)

  GainsExperienceMultiplier(int modifier,
                            sim::ConditionalTraitData conditional);
  ~GainsExperienceMultiplier() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  int GetGainsExperienceModifier() const override { return int4_modifier_; }

 private:
  friend class sim::ConditionalTraitCore<GainsExperienceMultiplier>;
  int int4_modifier_ = 100;
};

class PlayerExperience final : public sim::TraitBase, public sim::ISync {
 public:
  ORA_TRAIT_INTERFACES(PlayerExperience,
                       OpenRA_Mods_Common_Traits_PlayerExperience,
                       sim::ISync)

  void GiveExperience(int num) { Experience += num; }

  int Experience = 0;
};

}
