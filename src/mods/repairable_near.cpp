#include "mods/repairable_near.hpp"

#include "mods/air_activities.hpp"
#include "mods/air_support.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::string_view> FieldString(const meta::RecordObject& rec,
                                            std::string_view str_name) {
  return sim::RecordFieldString(rec, str_name);
}

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec, str_name);
}

}

RepairableNearInfoData RepairableNearInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RepairableNearInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "RepairActors"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_repair_actors.push_back(*s);
  if (const auto v = FieldInt(rec_info, "CloseEnough"))
    data.dist_close_enough = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = FieldString(rec_info, "Voice"))
    data.str_voice = std::string{*v};
  if (const auto v = FieldString(rec_info, "RequireForceMoveCondition"))
    if (!v->empty())
      data.expr_require_force_move =
          expr::BooleanExpression{std::string{*v}};
  if (const auto v = FieldString(rec_info, "EnterCursor"))
    data.str_enter_cursor = std::string{*v};
  if (const auto v = FieldString(rec_info, "EnterBlockedCursor"))
    data.str_enter_blocked_cursor = std::string{*v};
  return data;
}

RepairableNear::RepairableNear(Actor& self, RepairableNearInfoData info)
    : info_{std::move(info)},
      ptr_self_{&self} {}

bool RepairableNear::CanRepairAt(Actor& target) {
  return std::find(info_.vec_repair_actors.begin(),
                   info_.vec_repair_actors.end(),
                   target.InfoName()) != info_.vec_repair_actors.end();
}

bool RepairableNear::CanRepairAt(Actor& target,
                                 sim::TargetModifiers modifiers) {
  if (b_require_force_move_ &&
      (static_cast<std::int32_t>(modifiers) &
       static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) == 0)
    return false;

  return CanRepairAt(target);
}

bool RepairableNear::ShouldRepair() {
  if (sim::IHealth* health = ptr_self_->TraitOrDefault<sim::IHealth>())
    return health->DamageState() > sim::DamageState::Undamaged;
  return false;
}

std::vector<sim::IOrderTargeter*> RepairableNear::Orders() {
  std::vector<sim::IOrderTargeter*> vec_orders;
  if (!ptr_order_targeter_)
    ptr_order_targeter_ = std::make_unique<RepairNearEnterTargeter>(info_);
  vec_orders.push_back(ptr_order_targeter_.get());
  return vec_orders;
}

std::unique_ptr<net::Order> RepairableNear::IssueOrder(
    Actor& self, sim::IOrderTargeter* order,
    const sim::Target& target, bool queued) {
  if (order->OrderID() == "RepairNear")
    return std::make_unique<net::Order>(order->OrderID(), &self, target,
                                        queued);

  return nullptr;
}

std::string RepairableNear::VoicePhraseForOrder(Actor&,
                                                const net::Order& order) {
  return order.str_order_string == "RepairNear" && ShouldRepair()
             ? info_.str_voice
             : std::string{};
}

void RepairableNear::ResolveOrder(Actor& self, const net::Order& order) {
  if (order.str_order_string != "RepairNear")
    return;
  if (order.target.Type() != sim::TargetType::Actor)
    return;

  auto* target_actor = const_cast<Actor*>(order.target.ActorPtr);
  if (!CanRepairAt(*target_actor) || !ShouldRepair())
    return;

  self.QueueActivity(
      order.b_queued,
      self.world().Arena().Create<activities::Resupply>(
          self, *target_actor, info_.dist_close_enough));
  self.ShowTargetLines();
}

Actor* RepairableNear::FindRepairBuilding(Actor& self) {
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

std::vector<sim::VariableObserver> RepairableNear::GetVariableObservers() {
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

void RepairableNear::RequireForceMoveConditionChanged(
    Actor&, sim::ConditionCacheView conditions) {
  b_require_force_move_ =
      info_.expr_require_force_move->Evaluate(sim::ConditionSymbols(conditions));
}

bool RepairableNear::RepairNearEnterTargeter::TargetOverridesSelection(
    Actor&, const sim::Target&, std::span<Actor* const>, CPos,
    sim::TargetModifiers) {
  return false;
}

bool RepairableNear::RepairNearEnterTargeter::CanTarget(
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

  auto* repairable_near = self.TraitOrDefault<RepairableNear>();
  if (repairable_near == nullptr ||
      !repairable_near->CanRepairAt(*target_actor, modifiers))
    return false;

  cursor = repairable_near->ShouldRepair() ? info_.str_enter_cursor
                                           : info_.str_enter_blocked_cursor;
  return true;
}

}
