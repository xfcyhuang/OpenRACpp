// UPSTREAM: OpenRA.Mods.Common/Traits/GainsExperience.cs @b6fc03f
//          L21-172 + GivesExperience.cs L15-79 +
//          Multipliers/GainsExperienceMultiplier.cs L11-29 +
//          Player/PlayerExperience.cs L14-35(逐语句重写;机制对照见
//          experience.hpp 头注)
//          Statement-by-statement; the mechanism mapping lives in
//          experience.hpp's header note.
#include "mods/experience.hpp"


#include "core/percent_modifiers.hpp"
#include "meta/field_loader.hpp"
#include "game/actor_info.hpp"
#include "mods/experience.hpp"
#include "mods/production_support.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— GainsExperience ————

GainsExperienceInfoData GainsExperienceInfoData::Parse(
    const meta::RecordObject& rec) {
  GainsExperienceInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec, "Conditions"))
    if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [key, value] : *dict) {
        int int_key = 0;
        if (const auto* k = std::get_if<std::int64_t>(&key.val))
          int_key = static_cast<int>(*k);
        std::string str_value;
        if (const auto* v = std::get_if<std::string>(&value.val))
          str_value = *v;
        data.vec_conditions.emplace_back(int_key, std::move(str_value));
      }
  if (const auto s = sim::RecordFieldString(rec, "LevelUpImage"))
    data.str_level_up_image = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec, "LevelUpSequence"))
    data.str_level_up_sequence = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec, "LevelUpPalette"))
    data.str_level_up_palette = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec, "ExperienceModifier"))
    data.int4_experience_modifier = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec, "SuppressLevelupAnimation"))
    data.b_suppress_levelup_animation = *v != 0;
  if (const auto s = sim::RecordFieldString(rec, "LevelUpNotification"))
    data.str_level_up_notification = std::string{*s};
  return data;
}

GainsExperience::GainsExperience(sim::ActorInitializer& init,
                                 GainsExperienceInfoData info)
    : info_{std::move(info)} {
  // L82-90
  ptr_self_ = &init.Self();
  Experience = 0;
  int4_max_level_ = static_cast<int>(info_.vec_conditions.size());
  int4_initial_experience_ =
      init.GetValue<sim::ExperienceInit, int>(std::string_view{}, 0);
}

GainsExperience::~GainsExperience() = default;

namespace {
/// 上游 TraitInfoOrDefault<ValuedInfo>() 的具体类名查询(注册面同形)
/// Upstream's TraitInfoOrDefault<ValuedInfo>() as the concrete-class-name
/// query (the same registration face).
const meta::RecordObject* FindTraitInfoByFullName(
    const game::ActorInfo& actor_info, std::string_view str_full_name) {
  for (const meta::RecordObject* rec_trait :
       actor_info.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name == str_full_name)
      return rec_trait;
  return nullptr;
}
}  // namespace

void GainsExperience::Created(sim::Actor& self) {
  // L92-101:requiredExperience = ExperienceModifier < 0 ? Valued.Cost : 1
  // : ExperienceModifier
  // L92-101: requiredExperience = ExperienceModifier < 0 ? Valued.Cost : 1
  // : ExperienceModifier.
  int required_experience = 1;
  if (info_.int4_experience_modifier < 0) {
    if (const meta::RecordObject* valued =
            FindTraitInfoByFullName(*self.Info(),
                                    "OpenRA.Mods.Common.Traits.ValuedInfo"))
      if (const auto v = sim::RecordFieldInt(*valued, "Cost"))
        required_experience = static_cast<int>(*v);
  } else {
    required_experience = info_.int4_experience_modifier;
  }

  for (const auto& [key, condition] : info_.vec_conditions)
    vec_next_level_.emplace_back(key * required_experience, condition);

  if (int4_initial_experience_ > 0)
    GiveExperience(int4_initial_experience_,
                   info_.b_suppress_levelup_animation);
}

void GainsExperience::GiveLevels(int num_levels, bool silent) {
  // L105-112
  if (int4_max_level_ == 0)
    return;

  const int new_level = std::min(Level + num_levels, int4_max_level_);
  GiveExperience(
      vec_next_level_[new_level - 1].first - Experience, silent);
}

void GainsExperience::GiveExperience(int amount, bool silent) {
  // L114-139
  if (amount < 0)
    throw std::runtime_error(
        "Revoking experience is not implemented. (Parameter 'amount')");

  if (int4_max_level_ == 0)
    return;

  // (Experience + amount).Clamp(0, 上限)
  // (Experience + amount).Clamp(0, the cap).
  int clamped = Experience + amount;
  const int cap = vec_next_level_[int4_max_level_ - 1].first;
  if (clamped < 0)
    clamped = 0;
  if (clamped > cap)
    clamped = cap;
  Experience = clamped;

  while (Level < int4_max_level_ &&
         Experience >= vec_next_level_[Level].first) {
    ptr_self_->GrantCondition(vec_next_level_[Level].second);

    Level++;

    // 声音/SpriteEffect/Fluent 面省略(无 RNG 消耗)
    // The sound/SpriteEffect/Fluent faces omitted (no RNG consumption).
    (void)silent;
  }
}

void GainsExperience::ResolveOrder(sim::Actor& self,
                                   const net::Order& order) {
  // L141-159
  if (order.str_order_string != OrderName)
    return;

  const DeveloperMode* developer_mode =
      self.Owner()->PlayerActor()->TraitOrDefault<DeveloperMode>();
  if (developer_mode == nullptr || !developer_mode->Enabled)
    return;

  if (order.uint4_extra_data > 0)
    GiveLevels(static_cast<int>(order.uint4_extra_data));
  else
    GiveLevels(1);

  // FluentProvider 的 cheat 通知面省略(Phase 6)
  // The FluentProvider cheat-notification face omitted (Phase 6).
}

void GainsExperience::ModifyTransformActorInit(
    sim::Actor& self, sim::TypeDictionary& init) {
  // L161-164
  init.Add(self.world().Arena().Create<sim::ExperienceInit>(Experience));
}

// ———— GivesExperience ————

GivesExperienceInfoData GivesExperienceInfoData::Parse(
    const meta::RecordObject& rec) {
  GivesExperienceInfoData data;
  if (const auto v = sim::RecordFieldInt(rec, "Experience"))
    data.int4_experience = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec, "ValidRelationships"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.valid_relationships =
          static_cast<sim::PlayerRelationship>(*bits);
  if (const auto v = sim::RecordFieldInt(rec, "ActorExperienceModifier"))
    data.int4_actor_experience_modifier = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec, "PlayerExperienceModifier"))
    data.int4_player_experience_modifier = static_cast<int>(*v);
  return data;
}

GivesExperience::GivesExperience(GivesExperienceInfoData info)
    : info_{std::move(info)} {}

GivesExperience::~GivesExperience() = default;

void GivesExperience::Created(sim::Actor& self) {
  // L54-60
  int cost = 0;
  if (const meta::RecordObject* valued = FindTraitInfoByFullName(
          *self.Info(), "OpenRA.Mods.Common.Traits.ValuedInfo"))
    if (const auto v = sim::RecordFieldInt(*valued, "Cost"))
      cost = static_cast<int>(*v);
  int4_exp_ = info_.int4_experience >= 0 ? info_.int4_experience : cost;

  for (const auto* modifier :
       self.TraitsImplementing<sim::IGivesExperienceModifier>())
    vec_experience_modifiers_.push_back(modifier->GetGivesExperienceModifier());
}

void GivesExperience::Killed(sim::Actor& self,
                             const sim::AttackInfo& e) {
  // L62-79
  if (int4_exp_ == 0 || e.Attacker == nullptr || e.Attacker->Disposed())
    return;

  if (!sim::HasRelationship(
          info_.valid_relationships,
          e.Attacker->Owner()->RelationshipWith(self.Owner())))
    return;
  // attacker 的可变面在首个判定后解析(上游同序)
  // The attacker's mutable face resolves after the first predicate
  // (upstream's order).

  int4_exp_ = ApplyPercentageModifiers(int4_exp_,
                                             vec_experience_modifiers_);

  // AttackInfo.Attacker 的 const 面(auto_target.cpp 同形 const_cast)
  // AttackInfo.Attacker's const face (the same const_cast shape as
  // auto_target.cpp).
  sim::Actor* attacker = const_cast<sim::Actor*>(e.Attacker);
  GainsExperience* killer = attacker->TraitOrDefault<GainsExperience>();
  if (killer != nullptr) {
    std::vector<int> vec_killer_modifiers;
    for (const auto* modifier :
         attacker->TraitsImplementing<sim::IGainsExperienceModifier>())
      vec_killer_modifiers.push_back(modifier->GetGainsExperienceModifier());
    vec_killer_modifiers.push_back(info_.int4_actor_experience_modifier);
      killer->GiveExperience(ApplyPercentageModifiers(
        int4_exp_, vec_killer_modifiers));
  }

  if (PlayerExperience* player_experience =
          attacker->Owner()
              ->PlayerActor()
              ->TraitOrDefault<PlayerExperience>())
    player_experience->GiveExperience(
        ApplyPercentageModifiers(
            int4_exp_, std::vector<int>{
                           info_.int4_player_experience_modifier}));
}

// ———— GainsExperienceMultiplier ————

GainsExperienceMultiplier::GainsExperienceMultiplier(
    int modifier, sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional}, int4_modifier_{modifier} {}

GainsExperienceMultiplier::~GainsExperienceMultiplier() = default;

}  // namespace ora::mods
