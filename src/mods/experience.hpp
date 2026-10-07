// UPSTREAM: OpenRA.Mods.Common/Traits/GainsExperience.cs @b6fc03f L21-172
//          全文 + GivesExperience.cs L15-79 全文 +
//          Multipliers/GainsExperienceMultiplier.cs L11-29 全文 +
//          Player/PlayerExperience.cs L14-35 全文
//          The whole of GainsExperience.cs L21-172 + GivesExperience.cs
//          L15-79 + Multipliers/GainsExperienceMultiplier.cs L11-29 +
//          Player/PlayerExperience.cs L14-35.
//
// 机制对照 / Mechanism mapping:
//  - Conditions 的 FrozenDictionary<int,string> → 插入序 pair vector
//    (yaml 节点序 = 上游 FieldLoader 插入序;nextLevel 的等级序保真)
//    Conditions' FrozenDictionary<int,string> → an insertion-ordered
//    pair vector (the yaml node order = upstream's FieldLoader insert
//    order; nextLevel's level order preserved).
//  - 击杀方的 PlayerExperience 授予 / 升级声音+SpriteEffect / Fluent
//    通知:PlayerExperience 全量;声音/特效/通知面省略(声音注入惯例,
//    无 RNG)
//    The killer's PlayerExperience grant / the level-up sound+SpriteEffect
//    / Fluent notifications: PlayerExperience in full; the sound/effect/
//    notification faces omitted (the sound-injection convention, no RNG).
//  - DevLevelUp order 的 DeveloperMode 检查全量保留
//    The DevLevelUp order's DeveloperMode check is kept in full.
#pragma once
import std;

#include "meta/generic_record.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

/// GainsExperienceInfo(L23-58)的解析面
/// The parsed face of GainsExperienceInfo (L23-58).
struct GainsExperienceInfoData {
  /// L29:Conditions(int 百分比 → 条件;插入序)
  /// L29: Conditions (int percentage → condition; insertion order).
  std::vector<std::pair<int, std::string>> vec_conditions;
  std::string str_level_up_image;      // L35
  std::string str_level_up_sequence = "levelup";  // L39
  std::string str_level_up_palette = "effect";    // L43
  int int4_experience_modifier = -1;   // L46
  bool b_suppress_levelup_animation = true;       // L49
  std::string str_level_up_notification;          // L52

  static GainsExperienceInfoData Parse(const meta::RecordObject& rec);
};

/// GainsExperience(L60-172)
class GainsExperience final : public sim::TraitBase,
                              public sim::INotifyCreated,
                              public sim::ISync,
                              public sim::IResolveOrder,
                              public sim::ITransformActorInitModifier {
 public:
  static constexpr std::string_view OrderName = "DevLevelUp";  // L65

  ORA_TRAIT_INTERFACES(GainsExperience,
                       OpenRA_Mods_Common_Traits_GainsExperience,
                       sim::INotifyCreated, sim::ISync,
                       sim::IResolveOrder, sim::ITransformActorInitModifier)

  GainsExperience(sim::ActorInitializer& init, GainsExperienceInfoData info);
  ~GainsExperience() override;

  bool CanGainLevel() const { return Level < int4_max_level_; }  // L103

  /// L105-112:GiveLevels
  void GiveLevels(int num_levels, bool silent = false);

  /// L114-139:GiveExperience(负数抛 ArgumentException 文本逐字)
  /// L114-139: GiveExperience (a negative throws ArgumentException's
  /// text verbatim).
  void GiveExperience(int amount, bool silent = false);

  void Created(sim::Actor& self) override;  // L92-101
  void ResolveOrder(sim::Actor& self,      // L141-159
                    const net::Order& order) override;
  void ModifyTransformActorInit(           // L161-164
      sim::Actor& self, sim::TypeDictionary& init) override;

  int Experience = 0;  // [VerifySync] L76 | the [VerifySync] member.
  int Level = 0;       // [VerifySync] L79 | the [VerifySync] member.

 private:
  sim::Actor* ptr_self_ = nullptr;
  GainsExperienceInfoData info_;
  int int4_initial_experience_ = 0;
  int int4_max_level_ = 0;

  /// L72:nextLevel(RequiredExperience, Condition)表(等级序)
  /// L72: the nextLevel (RequiredExperience, Condition) table (level
  /// order).
  std::vector<std::pair<int, std::string>> vec_next_level_;
};

/// GivesExperienceInfo(L19-38)的解析面
/// The parsed face of GivesExperienceInfo (L19-38).
struct GivesExperienceInfoData {
  int int4_experience = -1;  // L22
  sim::PlayerRelationship valid_relationships =  // L25
      sim::PlayerRelationship::Neutral | sim::PlayerRelationship::Enemy;
  int int4_actor_experience_modifier = 10000;  // L28
  int int4_player_experience_modifier = 0;     // L31

  static GivesExperienceInfoData Parse(const meta::RecordObject& rec);
};

/// GivesExperience(L40-79)
class GivesExperience final : public sim::TraitBase,
                              public sim::INotifyCreated,
                              public sim::INotifyKilled {
 public:
  ORA_TRAIT_INTERFACES(GivesExperience,
                       OpenRA_Mods_Common_Traits_GivesExperience,
                       sim::INotifyCreated, sim::INotifyKilled)

  explicit GivesExperience(GivesExperienceInfoData info);
  ~GivesExperience() override;

  void Created(sim::Actor& self) override;                     // L54-60
  void Killed(sim::Actor& self, const sim::AttackInfo& e) override;  // L62-79

 private:
  GivesExperienceInfoData info_;
  int int4_exp_ = 0;
  std::vector<int> vec_experience_modifiers_;  // IGivesExperienceModifier 链
                                               // the modifier chain.
};

/// GainsExperienceMultiplier(L18-29;IGainsExperienceModifier 的条件承载)
/// GainsExperienceMultiplier (L18-29; IGainsExperienceModifier's
/// conditional carrier).
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
  int int4_modifier_ = 100;  // L16
};

/// PlayerExperience(PlayerExperience.cs L14-35;player actor 挂载)
/// PlayerExperience (PlayerExperience.cs L14-35; mounted on the player
/// actor).
class PlayerExperience final : public sim::TraitBase, public sim::ISync {
 public:
  ORA_TRAIT_INTERFACES(PlayerExperience,
                       OpenRA_Mods_Common_Traits_PlayerExperience,
                       sim::ISync)

  void GiveExperience(int num) { Experience += num; }  // L31-34

  int Experience = 0;  // [VerifySync] L27 | the [VerifySync] member.
};

}  // namespace ora::mods
