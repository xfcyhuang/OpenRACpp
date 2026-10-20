// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/RepairableBuilding.cs
//          @e180e3225 L22-208 全文(实现部分)。
#include "mods/repairable_building.hpp"

#include "mods/actor_exts.hpp"
#include "mods/experience.hpp"
#include "mods/player_resources.hpp"
#include "mods/sellable.hpp"
#include "net/text_notifications.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec, str_name);
}

std::optional<bool> FieldBool(const meta::RecordObject& rec,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* b = std::get_if<bool>(&gv->val))
    return *b;
  return std::nullopt;
}

std::optional<std::string_view> FieldString(const meta::RecordObject& rec,
                                            std::string_view str_name) {
  return sim::RecordFieldString(rec, str_name);
}

}  // namespace

RepairableBuildingInfoData RepairableBuildingInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RepairableBuildingInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldInt(rec_info, "RepairPercent"))
    data.int4_repair_percent = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "RepairInterval"))
    data.int4_repair_interval = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "RepairStep"))
    data.int4_repair_step = static_cast<int>(*v);
  if (const auto* slot = sim::RecordFieldValue(rec_info, "RepairDamageTypes"))
    if (auto* n = std::get_if<std::int64_t>(&slot->val))
      data.bitset_repair_damage_types =
          core::BitSet<sim::DamageType>::FromRawBits(
              static_cast<std::uint64_t>(*n));
  if (const auto* gv = sim::RecordFieldValue(rec_info, "RepairBonuses"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val)) {
      data.vec_repair_bonuses.clear();
      for (const auto& item : *items)
        if (const auto* n = std::get_if<std::int64_t>(&item.val))
          data.vec_repair_bonuses.push_back(static_cast<int>(*n));
    }
  if (const auto v = FieldBool(rec_info, "CancelWhenDisabled"))
    data.b_cancel_when_disabled = *v;
  if (const auto v = FieldInt(rec_info, "PlayerExperience"))
    data.int4_player_experience = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "RepairCondition"))
    data.str_repair_condition = std::string{*v};
  if (const auto v = FieldString(rec_info, "RepairingNotification"))
    data.str_repairing_notification = std::string{*v};
  if (const auto v = FieldString(rec_info, "RepairingTextNotification"))
    data.str_repairing_text_notification = std::string{*v};
  if (const auto v = FieldString(rec_info, "RepairingStoppedNotification"))
    data.str_repairing_stopped_notification = std::string{*v};
  if (const auto v = FieldString(rec_info, "RepairingStoppedTextNotification"))
    data.str_repairing_stopped_text_notification = std::string{*v};
  return data;
}

RepairableBuilding::RepairableBuilding(Actor& self,
                                       RepairableBuildingInfoData info)
    : ConditionalTraitCore{info.conditional},
      info_{std::move(info)} {
  ptr_health_ = self.Trait<sim::IHealth>();
}

int RepairableBuilding::RepairersHash() const {
  int hash = 0;
  for (const sim::Player* player : vec_repairers_)
    hash ^= sim::sync::HashPlayer(player);
  return hash;
}

void RepairableBuilding::UpdateCondition(Actor& self) {
  if (info_.str_repair_condition.empty())
    return;

  while (vec_repairers_.size() > vec_repair_tokens_.size())
    vec_repair_tokens_.push_back(
        self.GrantCondition(info_.str_repair_condition));

  while (vec_repairers_.size() < vec_repair_tokens_.size() &&
         !vec_repair_tokens_.empty()) {
    const int token = vec_repair_tokens_.back();
    vec_repair_tokens_.pop_back();
    self.RevokeCondition(token);
  }
}

void RepairableBuilding::RepairBuilding(Actor& self, sim::Player* ptr_player) {
  if (IsTraitDisabled() || !AppearsFriendlyTo(self, *ptr_player->PlayerActor()))
    return;

  const auto it_player = std::find(vec_repairers_.begin(),
                                   vec_repairers_.end(), ptr_player);
  if (it_player != vec_repairers_.end()) {
    vec_repairers_.erase(it_player);
    UpdateCondition(self);
    if (vec_repairers_.empty())
      net::TextNotificationsManager::Instance().AddTransientLine(
          self.Owner(), info_.str_repairing_stopped_text_notification);

    return;
  }

  if (vec_repairers_.size() >= info_.vec_repair_bonuses.size() - 1)
    return;

  vec_repairers_.push_back(ptr_player);

  net::TextNotificationsManager::Instance().AddTransientLine(
      self.Owner(), info_.str_repairing_text_notification);

  UpdateCondition(self);
}

void RepairableBuilding::Tick(Actor& self) {
  if (IsTraitDisabled()) {
    if (b_repair_active_ && info_.b_cancel_when_disabled) {
      vec_repairers_.clear();
      UpdateCondition(self);
    }

    return;
  }

  if (int4_remaining_ticks_ == 0) {
    vec_repairers_.erase(
        std::remove_if(vec_repairers_.begin(), vec_repairers_.end(),
                       [&self](const sim::Player* player) {
                         return player->PlayerWinState() !=
                                    sim::WinState::Undefined ||
                                self.Owner()->RelationshipWith(player) !=
                                    sim::PlayerRelationship::Ally;
                       }),
        vec_repairers_.end());
    UpdateCondition(self);

    if (vec_repairers_.empty()) {
      b_repair_active_ = false;
      return;
    }

    const int int4_building_value = GetSellValue(self);

    const int int4_hp_to_repair =
        std::min(info_.int4_repair_step,
                 ptr_health_->MaxHP() - ptr_health_->HP());

    const int int4_cost = std::max(
        1, static_cast<int>(static_cast<std::int64_t>(int4_hp_to_repair) *
                            info_.int4_repair_percent * int4_building_value /
                            (static_cast<std::int64_t>(ptr_health_->MaxHP()) *
                             100)));

    int int4_active_players = 0;
    for (sim::Player* repairer : vec_repairers_)
      if (repairer->PlayerActor()->Trait<PlayerResources>()->TakeCash(
              int4_cost, true))
        int4_active_players++;

    b_repair_active_ = int4_active_players > 0;

    if (!b_repair_active_) {
      int4_remaining_ticks_ = 1;
      return;
    }

    ptr_health_->InflictDamage(
        self, &self,
        sim::Damage{-(int4_hp_to_repair *
                      info_.vec_repair_bonuses[int4_active_players - 1] / 100),
                    info_.bitset_repair_damage_types},
        false);

    if (ptr_health_->DamageState() == sim::DamageState::Undamaged) {
      for (sim::Player* repairer : vec_repairers_) {
        if (repairer == self.Owner())
          continue;
        if (PlayerExperience* experience =
                repairer->PlayerActor()->TraitOrDefault<PlayerExperience>())
          experience->GiveExperience(info_.int4_player_experience);
      }

      vec_repairers_.clear();
      b_repair_active_ = false;
      UpdateCondition(self);
      return;
    }

    int4_remaining_ticks_ = info_.int4_repair_interval;
  } else {
    --int4_remaining_ticks_;
  }
}

int RepairableBuildingSyncHash(const sim::ISync* s) {
  const auto* trait = static_cast<const RepairableBuilding*>(s);
  return trait->RepairersHash();
}

const bool b_repairable_building_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.RepairableBuilding",
                                &RepairableBuildingSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_repairable_building_sync_registered_anchor =
    &b_repairable_building_sync_registered;

}  // namespace ora::mods
