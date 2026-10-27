#include "mods/cloak.hpp"

#include "core/percent_modifiers.hpp"
#include "mods/armament.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

CloakInfoData CloakInfoData::Parse(const meta::RecordObject& rec) {
  CloakInfoData data;
  if (const auto v = sim::RecordFieldInt(rec, "InitialDelay"))
    data.int4_initial_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec, "CloakDelay"))
    data.int4_cloak_delay = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec, "UncloakOn"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.int4_uncloak_on = static_cast<std::int32_t>(*bits);
  if (const auto s = sim::RecordFieldString(rec, "CloakSound"))
    data.str_cloak_sound = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec, "UncloakSound"))
    data.str_uncloak_sound = std::string{*s};
  if (const auto* gv = sim::RecordFieldValue(rec, "DetectionTypes"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.bitset_detection_types =
          core::BitSet<DetectionType>::FromRawBits(
              static_cast<std::uint64_t>(*bits));
  if (const auto s = sim::RecordFieldString(rec, "CloakedCondition"))
    data.str_cloaked_condition = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec, "CloakType"))
    data.str_cloak_type = std::string{*s};
  data.conditional = sim::ConditionalTraitData::Parse(rec);

  return data;
}

Cloak::Cloak(CloakInfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {

  RemainingTime = info_.int4_initial_delay;
}

Cloak::~Cloak() = default;

void Cloak::Created(sim::Actor& self) {

  if (!info_.str_cloak_type.empty()) {
    for (Cloak* c : self.TraitsImplementing<Cloak>())
      if (c != this && c->info_.str_cloak_type == info_.str_cloak_type)
        vec_other_cloaks_.push_back(c);
  }

  if (Cloaked()) {
    b_was_cloaked_ = true;
    if (int4_cloaked_token_ == sim::Actor::InvalidConditionToken)
      int4_cloaked_token_ =
          self.GrantCondition(info_.str_cloaked_condition);
  }

  ConditionalTraitCore::CoreCreated(self);
}

void Cloak::Attacking(sim::Actor&, const sim::Target&, Armament&,
                      const Barrel&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::Attack))
    Uncloak();
}

void Cloak::Damaged(sim::Actor& self, const sim::AttackInfo& e) {

  if (e.Damage->Value == 0)
    return;

  const UncloakType type =
      e.Damage->Value < 0
          ? (e.Attacker == &self ? UncloakType::SelfHeal : UncloakType::Heal)
          : UncloakType::Damage;
  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on), type))
    Uncloak();
}

void Cloak::Tick(sim::Actor& self) {

  if (!ConditionalTraitCore::IsTraitDisabled() &&
      !ConditionalTraitCore::IsTraitPaused()) {
    if (RemainingTime > 0 && !b_is_docking_)
      RemainingTime--;

    if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                       UncloakType::Move) &&
        (!opt_last_pos_.has_value() || *opt_last_pos_ != self.Location())) {
      Uncloak();
      opt_last_pos_ = self.Location();
    }
  }

  const bool is_cloaked = Cloaked();
  if (is_cloaked && !b_was_cloaked_) {
    if (int4_cloaked_token_ == sim::Actor::InvalidConditionToken)
      int4_cloaked_token_ =
          self.GrantCondition(info_.str_cloaked_condition);
  } else if (!is_cloaked && b_was_cloaked_) {
    if (int4_cloaked_token_ != sim::Actor::InvalidConditionToken)
      int4_cloaked_token_ = self.RevokeCondition(int4_cloaked_token_);
  }

  b_was_cloaked_ = is_cloaked;
  b_first_tick_ = false;
}

void Cloak::TraitEnabledHook(sim::Actor&) {

  RemainingTime = info_.int4_initial_delay;
}

void Cloak::TraitDisabledHook(sim::Actor&) {

  Uncloak();
}

bool Cloak::IsVisible(sim::Actor& self, sim::Player* viewer) {

  if (!Cloaked() || self.Owner()->IsAlliedWith(viewer))
    return true;

  for (auto& pair :
       self.world().ActorsWithTrait<DetectCloaked>()) {
    if (!pair.actor->IsInWorld())
      continue;
    if (!pair.actor->Owner()->IsAlliedWith(viewer))
      continue;
    if (!info_.bitset_detection_types.Overlaps(
            pair.trait->DetectionTypes()))
      continue;
    const WVec delta = self.CenterPosition() - pair.actor->CenterPosition();
    if (delta.LengthSquared() <= pair.trait->Range().LengthSquared())
      return true;
  }
  return false;
}

void Cloak::Docked(sim::Actor&, sim::Actor&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::Dock)) {
    b_is_docking_ = true;
    Uncloak();
  }
}

void Cloak::Undocked(sim::Actor&, sim::Actor&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::Dock))
    b_is_docking_ = false;
}

void Cloak::Loading(sim::Actor&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::Load))
    Uncloak();
}

void Cloak::Unloading(sim::Actor&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::Unload))
    Uncloak();
}

void Cloak::Demolishing(sim::Actor&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::Demolish))
    Uncloak();
}

void Cloak::Infiltrating(sim::Actor&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::Infiltrate))
    Uncloak();
}

void Cloak::Activated(sim::Actor&, const std::string&) {

  if (HasUncloakFlag(static_cast<UncloakType>(info_.int4_uncloak_on),
                     UncloakType::SupportPower))
    Uncloak();
}

DetectCloaked::DetectCloaked(core::BitSet<DetectionType> detection_types,
                             WDist range,
                             sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      bitset_detection_types_{std::move(detection_types)},
      range_{range} {}

DetectCloaked::~DetectCloaked() = default;

void DetectCloaked::Created(sim::Actor& self) {

  for (auto* modifier :
       self.TraitsImplementing<sim::IDetectCloakedModifier>())
    vec_range_modifiers_.push_back(modifier);
  ConditionalTraitCore::CoreCreated(self);
}

WDist DetectCloaked::Range() const {

  if (ConditionalTraitCore::IsTraitDisabled())
    return WDist::Zero();

  std::vector<int> vec_modifiers;
  vec_modifiers.reserve(vec_range_modifiers_.size());
  for (const auto* modifier : vec_range_modifiers_)
    vec_modifiers.push_back(modifier->GetDetectCloakedModifier());
  return WDist{ApplyPercentageModifiers(range_.Length,
                                              vec_modifiers)};
}

}
