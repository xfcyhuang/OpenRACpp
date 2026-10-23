// UPSTREAM: OpenRA.Mods.Common/Traits/AmmoPool.cs + Rearmable.cs +
//          Repairable.cs + RepairsUnits.cs + Air/FallsToEarth.cs 的实现面
#include "mods/air_support.hpp"

#include "game/ruleset.hpp"
#include "map/map.hpp"
#include "mods/air_activities.hpp"
#include "mods/aircraft.hpp"
#include "net/order.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec_info,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec_info, str_name);
}

std::optional<std::string> FieldString(const meta::RecordObject& rec_info,
                                       std::string_view str_name) {
  if (const auto v = sim::RecordFieldString(rec_info, str_name))
    return std::string{*v};
  return std::nullopt;
}

std::optional<bool> FieldBool(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* b = std::get_if<bool>(&gv->val))
    return *b;
  return std::nullopt;
}

std::vector<std::string> FieldStringArray(const meta::RecordObject& rec_info,
                                          std::string_view str_name) {
  std::vector<std::string> vec_out;
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return vec_out;
  if (const auto* list =
          std::get_if<std::vector<meta::GenericValue>>(&gv->val))
    for (const auto& element : *list)
      if (const auto* s = std::get_if<std::string>(&element.val))
        vec_out.push_back(*s);
  return vec_out;
}

}  // namespace

AmmoPoolInfoData AmmoPoolInfoData::Parse(const meta::RecordObject& rec_info) {
  AmmoPoolInfoData data;
  if (const auto v = FieldString(rec_info, "Name"))
    data.str_name = *v;
  data.vec_armaments = FieldStringArray(rec_info, "Armaments");
  if (data.vec_armaments.empty())
    data.vec_armaments = {"primary", "secondary"};
  if (const auto v = FieldInt(rec_info, "Ammo"))
    data.int4_ammo = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "InitialAmmo"))
    data.int4_initial_ammo = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "ReloadCount"))
    data.int4_reload_count = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "RearmSound"))
    data.str_rearm_sound = *v;
  if (const auto v = FieldInt(rec_info, "ReloadDelay"))
    data.int4_reload_delay = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "AmmoCondition"))
    data.str_ammo_condition = *v;
  return data;
}

AmmoPool::AmmoPool(const AmmoPoolInfoData& info)
    : info_{info} {
  int4_current_ammo_count_ =
      info_.int4_initial_ammo < info_.int4_ammo && info_.int4_initial_ammo >= 0
          ? info_.int4_initial_ammo
          : info_.int4_ammo;
}

bool AmmoPool::GiveAmmo(Actor& self, int count) {
  if (int4_current_ammo_count_ >= info_.int4_ammo || count < 0)
    return false;

  int4_current_ammo_count_ =
      std::clamp(int4_current_ammo_count_ + count, 0, info_.int4_ammo);
  UpdateCondition(self);
  return true;
}

bool AmmoPool::TakeAmmo(Actor& self, int count) {
  if (int4_current_ammo_count_ <= 0 || count < 0)
    return false;

  int4_current_ammo_count_ =
      std::clamp(int4_current_ammo_count_ - count, 0, info_.int4_ammo);
  UpdateCondition(self);
  return true;
}

void AmmoPool::Created(Actor& self) {
  UpdateCondition(self);
  int4_remaining_ticks_ = info_.int4_reload_delay;
}

void AmmoPool::Attacking(Actor& self, const sim::Target& target,
                         Armament& a, const Barrel& barrel) {
  if (std::find(info_.vec_armaments.begin(), info_.vec_armaments.end(),
                a.InfoData().str_name) != info_.vec_armaments.end())
    TakeAmmo(self, a.InfoData().int4_ammo_usage);
}

void AmmoPool::UpdateCondition(Actor& self) {
  if (info_.str_ammo_condition.empty())
    return;

  while (int4_current_ammo_count_ > static_cast<int>(vec_tokens_.size()) &&
         vec_tokens_.size() < static_cast<std::size_t>(info_.int4_ammo))
    vec_tokens_.push_back(self.GrantCondition(info_.str_ammo_condition));

  while (int4_current_ammo_count_ < static_cast<int>(vec_tokens_.size()) &&
         !vec_tokens_.empty()) {
    self.RevokeCondition(vec_tokens_.back());
    vec_tokens_.pop_back();
  }
}

RearmableInfoData RearmableInfoData::Parse(const meta::RecordObject& rec_info) {
  RearmableInfoData data;
  data.vec_rearm_actors = FieldStringArray(rec_info, "RearmActors");
  data.vec_ammo_pools = FieldStringArray(rec_info, "AmmoPools");
  if (data.vec_ammo_pools.empty())
    data.vec_ammo_pools = {"primary"};
  return data;
}

Rearmable::Rearmable(RearmableInfoData info)
    : info_{std::move(info)} {}

void Rearmable::Created(Actor& self) {
  for (auto* pool : self.TraitsImplementing<AmmoPool>())
    if (std::find(info_.vec_ammo_pools.begin(), info_.vec_ammo_pools.end(),
                  pool->Info().str_name) != info_.vec_ammo_pools.end())
      vec_rearmable_ammo_pools_.push_back(pool);
}

void Rearmable::Docked(Actor& self, Actor& dock) {
  for (auto* pool : vec_rearmable_ammo_pools_)
    pool->SetRemainingTicks(pool->Info().int4_reload_delay);
}

bool Rearmable::RearmTick(Actor& self) {
  auto rearm_complete = true;
  for (auto* ammo_pool : vec_rearmable_ammo_pools_) {
    if (!ammo_pool->HasFullAmmo()) {
      ammo_pool->SetRemainingTicks(ammo_pool->RemainingTicks() - 1);
      if (ammo_pool->RemainingTicks() <= 0) {
        ammo_pool->SetRemainingTicks(ammo_pool->Info().int4_reload_delay);
        ammo_pool->GiveAmmo(self, ammo_pool->Info().int4_reload_count);
      }

      rearm_complete = false;
    }
  }

  return rearm_complete;
}

RepairsUnitsInfoData RepairsUnitsInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RepairsUnitsInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldInt(rec_info, "ValuePercentage"))
    data.int4_value_percentage = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "HpPerStep"))
    data.int4_hp_per_step = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "Interval"))
    data.int4_interval = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "RepairDamageTypes"))
    data.bitset_repair_damage_types =
        core::BitSet<sim::DamageType>::FromRawBits(
            static_cast<std::uint64_t>(*v));
  if (const auto v = FieldString(rec_info, "StartRepairingNotification"))
    data.str_start_repairing_notification = *v;
  if (const auto v = FieldString(rec_info, "StartRepairingTextNotification"))
    data.str_start_repairing_text_notification = *v;
  if (const auto v = FieldString(rec_info, "FinishRepairingNotification"))
    data.str_finish_repairing_notification = *v;
  if (const auto v = FieldString(rec_info, "FinishRepairingTextNotification"))
    data.str_finish_repairing_text_notification = *v;
  if (const auto v = FieldInt(rec_info, "PlayerExperience"))
    data.int4_player_experience = static_cast<int>(*v);
  return data;
}

RepairsUnits::RepairsUnits(const RepairsUnitsInfoData& info)
    : sim::ConditionalTraitCore<RepairsUnits>(info.conditional),
      info_{info} {}

RepairableInfoData RepairableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RepairableInfoData data;
  data.vec_repair_actors = FieldStringArray(rec_info, "RepairActors");
  if (const auto v = FieldString(rec_info, "Voice"))
    data.str_voice = *v;
  if (const auto v = FieldInt(rec_info, "HpPerStep"))
    data.int4_hp_per_step = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "RequireForceMoveCondition");
      v.has_value() && !v->empty())
    data.expr_require_force_move = expr::BooleanExpression{std::string{*v}};
  if (const auto v = FieldString(rec_info, "EnterCursor"))
    data.str_enter_cursor = *v;
  if (const auto v = FieldString(rec_info, "EnterBlockedCursor"))
    data.str_enter_blocked_cursor = *v;
  return data;
}

Repairable::Repairable(ActorInitializer& init, const RepairableInfoData& info)
    : info_{info} {
  health_ = init.Self().Trait<sim::IHealth>();
}

void Repairable::Created(Actor& self) {
  rearmable_ = self.TraitOrDefault<Rearmable>();
  b_is_aircraft_ = self.Info()->HasTraitInfoOfInterface(
      "OpenRA.Mods.Common.Traits.AircraftInfo");
}

std::vector<sim::VariableObserver> Repairable::GetVariableObservers() {
  std::vector<sim::VariableObserver> vec_observers;
  if (info_.expr_require_force_move.has_value()) {
    sim::VariableObserver observer;
    observer.fn_notifier =
        [this](Actor& self, sim::ConditionCacheView vars) {
          RequireForceMoveConditionChanged(self, vars);
        };
    observer.vec_variables.assign(
        info_.expr_require_force_move->Variables().begin(),
        info_.expr_require_force_move->Variables().end());
    vec_observers.push_back(std::move(observer));
  }
  return vec_observers;
}

bool Repairable::CanRepairAt(Actor& target) {
  return std::find(info_.vec_repair_actors.begin(),
                   info_.vec_repair_actors.end(),
                   target.InfoName()) != info_.vec_repair_actors.end();
}

bool Repairable::CanRepairAt(Actor& target, sim::TargetModifiers modifiers) {
  if (b_require_force_move_ &&
      (static_cast<std::int32_t>(modifiers) &
       static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) == 0)
    return false;

  return CanRepairAt(target);
}

bool Repairable::CanRepair() {
  return health_->DamageState() > sim::DamageState::Undamaged;
}

bool Repairable::CanRearm() {
  if (rearmable_ == nullptr)
    return false;
  for (const auto* pool : rearmable_->RearmableAmmoPools())
    if (!pool->HasFullAmmo())
      return true;
  return false;
}

std::vector<sim::IOrderTargeter*> Repairable::Orders() {
  std::vector<sim::IOrderTargeter*> vec_orders;
  if (!b_is_aircraft_) {
    if (!ptr_order_targeter_)
      ptr_order_targeter_ = std::make_unique<RepairEnterTargeter>(info_);
    vec_orders.push_back(ptr_order_targeter_.get());
  }
  return vec_orders;
}

std::unique_ptr<net::Order> Repairable::IssueOrder(
    Actor& self, sim::IOrderTargeter* order,
    const sim::Target& target, bool queued) {
  if (order->OrderID() == "Repair")
    return std::make_unique<net::Order>(order->OrderID(), &self, target,
                                        queued);

  return nullptr;
}

std::string Repairable::VoicePhraseForOrder(Actor& self,
                                            const net::Order& order) {
  return order.str_order_string == "Repair" && (CanRepair() || CanRearm())
             ? info_.str_voice
             : std::string{};
}

void Repairable::ResolveOrder(Actor& self, const net::Order& order) {
  if (order.str_order_string != "Repair")
    return;

  if (order.target.Type() != sim::TargetType::Actor)
    return;

  if (b_is_aircraft_)
    return;

  auto* target_actor = const_cast<Actor*>(order.target.ActorPtr);
  if (!CanRepairAt(*target_actor) || (!CanRepair() && !CanRearm()))
    return;

  self.QueueActivity(
      order.b_queued,
      self.world().Arena().Create<activities::Resupply>(self, *target_actor,
                                                        WDist{512}));
  self.ShowTargetLines();
}

Actor* Repairable::FindRepairBuilding(Actor& self) {
  struct Entry {
    Actor* actor;
    bool own;
    std::int64_t dist;
  };
  std::vector<Entry> vec_entries;
  for (auto* a : self.world().ActorsHavingTrait<RepairsUnits>()) {
    if (a->IsDead() || !a->IsInWorld())
      continue;
    if (!a->Owner()->IsAlliedWith(self.Owner()))
      continue;
    if (std::find(info_.vec_repair_actors.begin(),
                  info_.vec_repair_actors.end(),
                  a->InfoName()) == info_.vec_repair_actors.end())
      continue;
    vec_entries.push_back(Entry{
        a, a->Owner() == self.Owner(),
        (self.Location() - a->Location()).LengthSquared()});
  }
  std::stable_sort(vec_entries.begin(), vec_entries.end(),
                   [](const Entry& a, const Entry& b) {
                     if (a.own != b.own)
                       return a.own;
                     return a.dist < b.dist;
                   });
  return vec_entries.empty() ? nullptr : vec_entries.front().actor;
}

void Repairable::RequireForceMoveConditionChanged(
    Actor&, sim::ConditionCacheView conditions) {
  b_require_force_move_ =
      info_.expr_require_force_move->Evaluate(sim::ConditionSymbols(conditions));
}

bool Repairable::RepairEnterTargeter::TargetOverridesSelection(
    Actor& self, const sim::Target& target, std::span<Actor* const>,
    CPos, sim::TargetModifiers) {
  return false;
}

bool Repairable::RepairEnterTargeter::CanTarget(
    Actor& self, const sim::Target& target, sim::TargetModifiers& modifiers,
    std::string& cursor) {
  if (target.Type() != sim::TargetType::Actor)
    return false;
  auto* target_actor = const_cast<Actor*>(target.ActorPtr);
  if (!self.Owner()->IsAlliedWith(target_actor->Owner()))
    return false;
  if (!target_actor->Info()->HasTraitInfoOfInterface(
          "OpenRA.Traits.BuildingInfo"))
    return false;
  auto* repairable = self.TraitOrDefault<Repairable>();
  if (repairable == nullptr ||
      !repairable->CanRepairAt(*target_actor, modifiers))
    return false;

  cursor = repairable->CanRepair() || repairable->CanRearm()
               ? info_.str_enter_cursor
               : info_.str_enter_blocked_cursor;
  return true;
}

FallsToEarthInfoData FallsToEarthInfoData::Parse(
    const meta::RecordObject& rec_info) {
  FallsToEarthInfoData data;
  if (const auto v = FieldString(rec_info, "Explosion"))
    data.str_explosion = *v;
  if (const auto v = FieldInt(rec_info, "MaximumSpinSpeed"))
    data.opt_maximum_spin_speed =
        std::optional<WAngle>{WAngle{static_cast<std::int32_t>(*v)}};
  if (const auto v = FieldBool(rec_info, "Moves"))
    data.b_moves = *v;
  if (const auto v = FieldInt(rec_info, "Velocity"))
    data.dist_velocity = WDist{static_cast<std::int32_t>(*v)};
  return data;
}

void FallsToEarthInfoData::RulesetLoaded(const game::Ruleset& rules,
                                         const game::ActorInfo&) {
  if (str_explosion.empty())
    return;

  std::string weapon_to_lower{str_explosion};
  std::transform(weapon_to_lower.begin(), weapon_to_lower.end(),
                 weapon_to_lower.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  const auto* weapon = rules.FindWeapon(weapon_to_lower);
  if (weapon == nullptr)
    throw std::runtime_error(
        "Weapons Ruleset does not contain an entry '" + weapon_to_lower + "'");

  ptr_explosion_weapon = weapon;
}

FallsToEarth::FallsToEarth(ActorInitializer& init,
                           const FallsToEarthInfoData& info)
    : info_{info} {
  p_effective_owner_ =
      init.GetValue<sim::EffectiveOwnerInit, sim::Player*>(
          "", init.Self().Owner());
}

void FallsToEarth::Created(Actor& self) {
  self.QueueActivity(
      false, self.world().Arena().Create<activities::FallToEarth>(self, info_));
}

}  // namespace ora::mods
