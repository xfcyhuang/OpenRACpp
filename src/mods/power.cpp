#include "mods/power.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/production_support.hpp"
#include "net/order.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

PowerManagerInfoData PowerManagerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  PowerManagerInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "AdviceInterval") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_advice_interval = static_cast<int>(*n);
      } else if (name == "SpeechNotification") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_speech_notification = *s;
      } else if (name == "TextNotification") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_text_notification = *s;
      }
    }
  }

  return data;
}

PowerManager::PowerManager(ActorInitializer& init,
                           const PowerManagerInfoData& info)
    : info_{info} {

  p_self_ = &init.Self();
  p_dev_mode_ = p_self_->Trait<DeveloperMode>();
  b_was_hack_enabled = p_dev_mode_->UnlimitedPower;
  PlayLowPowerNotification = info.int4_advice_interval > 0;
}

void PowerManager::Created(Actor& self) {

  self.world().AddFrameEndTask(
      [this](sim::World&) { UpdatePowerRequiringActors(); });
}

void PowerManager::UpdateActor(Actor& a) {

  if (!a.IsInWorld())
    return;

  int old = 0;
  if (const auto it_find = map_power_drain_.find(&a);
      it_find != map_power_drain_.end())
    old = it_find->second;

  int amount = 0;
  for (Power* p : a.TraitsImplementing<Power>())
    if (!p->IsTraitDisabled())
      amount += p->GetEnabledPower();
  map_power_drain_[&a] = amount;

  if (amount == old || p_dev_mode_->UnlimitedPower)
    return;

  if (old > 0)
    PowerProvided -= old;
  else if (old < 0)
    PowerDrained += old;

  if (amount > 0)
    PowerProvided += amount;
  else if (amount < 0)
    PowerDrained -= amount;

  UpdatePowerState();
}

void PowerManager::RemoveActor(Actor& a) {

  if (a.IsInWorld())
    return;

  const auto it_find = map_power_drain_.find(&a);
  if (it_find == map_power_drain_.end())
    return;
  const int amount = it_find->second;
  map_power_drain_.erase(it_find);

  if (p_dev_mode_->UnlimitedPower)
    return;

  if (amount > 0)
    PowerProvided -= amount;
  else if (amount < 0)
    PowerDrained -= amount;

  UpdatePowerState();
}

void PowerManager::UpdatePowerState() {

  b_is_low_power = ExcessPower() < 0;

  if (b_is_low_power != b_was_low_power)
    UpdatePowerRequiringActors();

  if (b_is_low_power && !b_was_low_power)
    int8_last_power_advice_time = -info_.int4_advice_interval;

  b_was_low_power = b_is_low_power;
}

void PowerManager::Tick(Actor& ) {

  if (b_was_hack_enabled != p_dev_mode_->UnlimitedPower) {
    PowerProvided = 0;
    PowerDrained = 0;

    if (!p_dev_mode_->UnlimitedPower) {
      for (const auto& [actor, value] : map_power_drain_) {
        [[maybe_unused]] Actor* unused_actor = actor;
        if (value > 0)
          PowerProvided += value;
        else if (value < 0)
          PowerDrained -= value;
      }
    }

    b_was_hack_enabled = p_dev_mode_->UnlimitedPower;
    UpdatePowerState();
  }

  if (PowerOutageRemainingTicks > 0 && --PowerOutageRemainingTicks == 0)
    UpdatePowerOutageActors();
}

void PowerManager::TriggerPowerOutage(int total_ticks) {

  PowerOutageTotalTicks = PowerOutageRemainingTicks = total_ticks;
  UpdatePowerOutageActors();
}

void PowerManager::UpdatePowerOutageActors() {

  for (auto& [actor, trait] :
       p_self_->world().ActorsWithTrait<AffectedByPowerOutage>())
    if (!actor->IsDead() && actor->IsInWorld() &&
        actor->Owner() == p_self_->Owner())
      trait->UpdateStatus(*actor);
}

void PowerManager::UpdatePowerRequiringActors() {

  for (auto& [actor, trait] :
       p_self_->world().ActorsWithTrait<sim::INotifyPowerLevelChanged>())
    if (!actor->IsDead() && actor->IsInWorld() &&
        actor->Owner() == p_self_->Owner())
      trait->PowerLevelChanged(*actor);
}

void PowerManager::ResolveOrder(Actor& self, const net::Order& order) {

  if (p_dev_mode_->Enabled &&
      order.str_order_string == "DevPowerOutage") {
    TriggerPowerOutage(static_cast<int>(order.uint4_extra_data));
  }
}

PowerInfoData PowerInfoData::Parse(const meta::RecordObject& rec_info) {
  PowerInfoData data;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      if (fields[i]->str_name != "Amount")
        continue;
      const meta::GenericValue& v = generated->Slot(i);
      if (auto* n = std::get_if<std::int64_t>(&v.val))
        data.int4_amount = static_cast<int>(*n);
    }
  }
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

Power::Power(ActorInitializer& init, const PowerInfoData& info)
    : sim::ConditionalTraitCore<Power>(info.conditional), info_{info} {

  p_self_ = &init.Self();
  p_player_power_ =
      p_self_->Owner()->PlayerActor()->Trait<PowerManager>();
}

void Power::Created(Actor& self) {

  CoreCreated(self);
}

int Power::GetEnabledPower() {

  if (!b_modifiers_resolved_) {
    vec_power_modifiers_ =
        p_self_->TraitsImplementing<sim::IPowerModifier>();
    b_modifiers_resolved_ = true;
  }

  vec_percentages_buffer_.clear();
  for (sim::IPowerModifier* m : vec_power_modifiers_)
    vec_percentages_buffer_.push_back(m->GetPowerModifier());
  return ApplyPercentageModifiers(info_.int4_amount,
                                   vec_percentages_buffer_);
}

void Power::TraitEnabledHook(Actor& self) { p_player_power_->UpdateActor(self); }
void Power::TraitDisabledHook(Actor& self) {
  p_player_power_->UpdateActor(self);
}

void Power::AddedToWorld(Actor& self) {
  p_player_power_->UpdateActor(self);
}

void Power::RemovedFromWorld(Actor& self) {
  p_player_power_->RemoveActor(self);
}

void Power::OnOwnerChanged(Actor& self, Player& ,
                           Player& new_owner) {

  p_player_power_->RemoveActor(self);
  p_player_power_ = new_owner.PlayerActor()->Trait<PowerManager>();
  p_player_power_->UpdateActor(self);
}

AffectedByPowerOutageInfoData AffectedByPowerOutageInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AffectedByPowerOutageInfoData data;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      if (fields[i]->str_name != "Condition")
        continue;
      const meta::GenericValue& v = generated->Slot(i);
      if (auto* s = std::get_if<std::string>(&v.val))
        data.str_condition = *s;
    }
  }
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

AffectedByPowerOutage::AffectedByPowerOutage(
    ActorInitializer& init, const AffectedByPowerOutageInfoData& info)
    : sim::ConditionalTraitCore<AffectedByPowerOutage>(info.conditional),
      info_{info} {

  p_player_power_ =
      init.Self().Owner()->PlayerActor()->Trait<PowerManager>();
}

void AffectedByPowerOutage::Created(Actor& self) {
  CoreCreated(self);
}

void AffectedByPowerOutage::UpdateStatus(Actor& self) {

  if (!IsTraitDisabled() && p_player_power_->PowerOutageRemainingTicks > 0)
    Grant(self);
  else
    Revoke(self);
}

void AffectedByPowerOutage::Grant(Actor& self) {

  if (int4_token_ == Actor::InvalidConditionToken)
    int4_token_ = self.GrantCondition(info_.str_condition);
}

void AffectedByPowerOutage::Revoke(Actor& self) {

  if (int4_token_ != Actor::InvalidConditionToken)
    int4_token_ = self.RevokeCondition(int4_token_);
}

void AffectedByPowerOutage::AddedToWorld(Actor& self) {
  UpdateStatus(self);
}

void AffectedByPowerOutage::OnOwnerChanged(Actor& self,
                                           Player& ,
                                           Player& new_owner) {

  p_player_power_ = new_owner.PlayerActor()->Trait<PowerManager>();
  UpdateStatus(self);
}

namespace {

int PowerManagerSyncHash(const sim::ISync* s) {
  const auto* pm = static_cast<const PowerManager*>(s);
  int hash = sim::sync::CombineSyncHash(0, pm->PowerProvided);
  return sim::sync::CombineSyncHash(hash, pm->PowerDrained);
}

const bool b_power_manager_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.PowerManager",
                                &PowerManagerSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_power_manager_sync_registered_anchor =
    &b_power_manager_sync_registered;

}

}
