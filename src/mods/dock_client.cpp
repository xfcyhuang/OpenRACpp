// UPSTREAM: OpenRA.Mods.Common/Traits/DockClientManager.cs 实现部分
//          The implementation half.
#include "mods/dock_client.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/dock_activities.hpp"
#include "mods/pathfinding/path_finder.hpp"
#include "net/order.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— DockClientManagerInfoData(L25-58)————

DockClientManagerInfoData DockClientManagerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  DockClientManagerInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "SearchForDockDelay") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_search_for_dock_delay = static_cast<int>(*n);
      } else if (name == "OccupancyCostModifier") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_occupancy_cost_modifier = static_cast<int>(*n);
      } else if (name == "RequireForceMoveCondition") {
        if (auto* s = std::get_if<std::string>(&v.val))
          if (!s->empty())
            data.expr_require_force_move = expr::BooleanExpression{*s};
      } else if (name == "EnterCursor") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_enter_cursor = *s;
      } else if (name == "EnterCursorOverrides") {
        if (auto* dict = std::get_if<meta::GenericDict>(&v.val))
          for (const auto& [key, value] : *dict) {
            const std::string* str_key =
                std::get_if<std::string>(&key.val);
            const std::string* str_value =
                std::get_if<std::string>(&value.val);
            if (str_key != nullptr && str_value != nullptr)
              data.vec_enter_cursor_overrides.emplace_back(*str_key,
                                                           *str_value);
          }
      } else if (name == "EnterBlockedCursor") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_enter_blocked_cursor = *s;
      } else if (name == "Voice") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_voice = *s;
      } else if (name == "DockLineColor") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.color_dock_line =
              core::Color::FromArgbRaw(static_cast<std::uint32_t>(*n));
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

// ———— DockClientManager(L60-335)————

DockClientManager::DockClientManager(ActorInitializer& init,
                                     const DockClientManagerInfoData& info)
    : sim::ConditionalTraitCore<DockClientManager>(info.conditional),
      info_{info} {
  p_self_ = &init.Self();
}

void DockClientManager::Created(Actor& self) {
  // L74-78
  vec_dock_clients_ = self.TraitsImplementing<sim::IDockClient>();
  CoreCreated(self);
}

std::vector<sim::VariableObserver> DockClientManager::GetVariableObservers() {
  // L246-253(base 先;RequireForceMove 追加)
  // L246-253 (the base first; RequireForceMove appended).
  std::vector<sim::VariableObserver> vec_observers = CollectObservers();
  if (info_.expr_require_force_move.has_value()) {
    sim::VariableObserver observer;
    observer.fn_notifier = [this](Actor& self,
                                  sim::ConditionCacheView conditions) {
      RequireForceMoveConditionChanged(self, conditions);
    };
    const auto vec_variables = info_.expr_require_force_move->Variables();
    observer.vec_variables.assign(vec_variables.begin(),
                                  vec_variables.end());
    vec_observers.push_back(std::move(observer));
  }
  return vec_observers;
}

sim::IDockHost* DockClientManager::LastReservedHost() {
  // L83-98
  if (p_last_reserved_dock_host_ != nullptr) {
    if (!p_last_reserved_dock_host_->IsEnabledAndInWorld())
      p_last_reserved_dock_host_ = nullptr;
    else
      return p_last_reserved_dock_host_;
  }

  return p_reserved_host_;
}

void DockClientManager::UnreserveHost() {
  // L100-109
  if (p_reserved_host_ != nullptr) {
    p_last_reserved_dock_host_ = p_reserved_host_;
    p_reserved_host_ = nullptr;
    p_reserved_host_actor_ = nullptr;
    p_last_reserved_dock_host_->Unreserve(this);
  }
}

bool DockClientManager::ReserveHost(Actor* host_actor,
                                    sim::IDockHost* host) {
  // L112-132
  if (host == nullptr)
    return false;

  if (p_reserved_host_ == host)
    return true;

  UnreserveHost();
  if (host->Reserve(*host_actor, this)) {
    p_reserved_host_ = host;
    p_reserved_host_actor_ = host_actor;

    // After we have reserved a new Host we want to forget our old host.
    // (上游注释)
    p_last_reserved_dock_host_ = nullptr;
    return true;
  }

  return false;
}

void DockClientManager::OnDockStarted(Actor& self, Actor& host_actor,
                                      sim::IDockHost* host) {
  // L134-138
  for (sim::IDockClient* client : vec_dock_clients_)
    client->OnDockStarted(self, host_actor, host);
}

bool DockClientManager::OnDockTick(Actor& self, Actor& host_actor,
                                   sim::IDockHost* host) {
  // L140-151
  if (IsTraitDisabled())
    return true;

  bool b_cancel = true;
  for (sim::IDockClient* client : vec_dock_clients_)
    if (!client->OnDockTick(self, host_actor, host))
      b_cancel = false;

  return b_cancel;
}

void DockClientManager::OnDockCompleted(Actor& self, Actor& host_actor,
                                        sim::IDockHost* host) {
  // L153-159
  for (sim::IDockClient* client : vec_dock_clients_)
    client->OnDockCompleted(self, host_actor, host);

  UnreserveHost();
}

std::vector<sim::IOrderTargeter*> DockClientManager::Orders() {
  // L161-198(每次枚举新 targeter;游标判定闭包)
  // L161-198 (a fresh targeter per enumeration; the cursor-predicate
  // closure).
  vec_order_targeters_.clear();
  vec_order_targeters_.push_back(
      std::make_unique<DockActorTargeter>(6, [this](
                                                 const DockActorTargeter::
                                                     CanTargetContext&
                                                         context) {
        if (b_require_force_move_ && !context.b_force_enter)
          return DockActorTargeter::CanTargetResult::Blocked(
              info_.str_enter_cursor);

        if (IsTraitDisabled())
          return DockActorTargeter::CanTargetResult::Blocked(
              info_.str_enter_blocked_cursor);

        std::vector<sim::IDockHost*> vec_available =
            GetDockableHosts(const_cast<Actor&>(*context.target.ActorPtr),
                         context.b_force_enter,
                             context.b_is_queued);
        if (vec_available.empty())
          return DockActorTargeter::CanTargetResult::Blocked(
              info_.str_enter_cursor);

        bool b_can_dock = false;
        for (sim::IDockHost* host : vec_available)
          for (sim::IDockClient* client : vec_dock_clients_)
            if (client->CanDockAt(
                    const_cast<Actor&>(*context.target.ActorPtr), host,
                    context.b_force_enter, true)) {
              b_can_dock = true;
              break;
            }

        std::string str_cursor_override;
        for (sim::IDockHost* dock_host : vec_available)
          for (const std::string& dock_type :
               dock_host->GetDockType().ToStrings())
            for (const auto& [key, cursor] :
                 info_.vec_enter_cursor_overrides)
              if (key == dock_type) {
                str_cursor_override = cursor;
                break;
              }

        const std::string str_cursor =
            context.b_is_queued || b_can_dock
                ? (!str_cursor_override.empty() ? str_cursor_override
                                                : info_.str_enter_cursor)
                : info_.str_enter_blocked_cursor;

        return DockActorTargeter::CanTargetResult::Allowed(str_cursor);
      }));
  std::vector<sim::IOrderTargeter*> vec_out;
  vec_out.push_back(vec_order_targeters_[0].get());
  return vec_out;
}

net::Order* DockClientManager::IssueOrder(
    Actor& self, sim::IOrderTargeter* order, const sim::Target& target,
    bool queued) {
  // L238-244
  if (dynamic_cast<DockActorTargeter*>(order) != nullptr)
    return new net::Order(order->OrderID(), &self, target, queued);

  return nullptr;
}

void DockClientManager::ResolveOrder(Actor& self,
                                     const net::Order& order) {
  // L200-222
  if (order.str_order_string == "Dock" ||
      order.str_order_string == "ForceDock") {
    const sim::Target& target = order.target;

    // Deliver orders are only valid for own/allied actors, which are
    // guaranteed to never be frozen. TODO: support frozen actors
    // (上游注释)
    if (target.Type() != sim::TargetType::Actor)
      return;

    self.QueueActivity(
        order.b_queued,
        activities::NewActivity<activities::MoveToDock>(
            self, const_cast<Actor*>(target.ActorPtr),
            static_cast<sim::IDockHost*>(nullptr),
            order.str_order_string == "ForceDock", true,
            std::optional<core::Color>{info_.color_dock_line}));

    self.ShowTargetLines();
  }
}

std::string DockClientManager::VoicePhraseForOrder(
    Actor& /*self*/, const net::Order& order) {
  // L224-236
  if (order.target.Type() != sim::TargetType::Actor ||
      IsTraitDisabled())
    return {};

  if (order.str_order_string != "Dock" &&
      order.str_order_string != "ForceDock")
    return {};

  if (CanQueueDockAt(const_cast<Actor&>(*order.target.ActorPtr),
                     order.str_order_string == "ForceDock",
                     order.b_queued))
    return info_.str_voice;

  return {};
}

bool DockClientManager::CanDock(
    const core::BitSet<sim::DockType>& type, bool /*force_enter*/) {
  // L261-264
  if (IsTraitDisabled())
    return false;
  for (sim::IDockClient* client : vec_dock_clients_)
    if (client->CanDock(type))
      return true;
  return false;
}

bool DockClientManager::CanDock(Actor& target, bool force_enter) {
  // L267-272
  if (IsTraitDisabled())
    return false;
  for (sim::IDockHost* host :
       target.TraitsImplementing<sim::IDockHost>())
    for (sim::IDockClient* client : vec_dock_clients_)
      if (client->CanDock(host->GetDockType(), force_enter))
        return true;
  return false;
}

bool DockClientManager::CanDockAt(Actor& host_actor, sim::IDockHost* host,
                                  bool force_enter,
                                  bool ignore_occupancy) {
  // L275-279
  if (IsTraitDisabled())
    return false;
  for (sim::IDockClient* client : vec_dock_clients_)
    if (client->CanDockAt(host_actor, host, force_enter, ignore_occupancy))
      return true;
  return false;
}

bool DockClientManager::CanDockAt(Actor& target, bool force_enter,
                                  bool ignore_occupancy) {
  // L282-286
  if (IsTraitDisabled())
    return false;
  for (sim::IDockHost* host :
       target.TraitsImplementing<sim::IDockHost>())
    for (sim::IDockClient* client : vec_dock_clients_)
      if (client->CanDockAt(target, host, force_enter, ignore_occupancy))
        return true;
  return false;
}

bool DockClientManager::CanQueueDockAt(Actor& target, bool force_enter,
                                       bool is_queued) {
  // L289-294
  if (IsTraitDisabled())
    return false;
  for (sim::IDockHost* host :
       target.TraitsImplementing<sim::IDockHost>())
    for (sim::IDockClient* client : vec_dock_clients_)
      if (client->CanQueueDockAt(target, host, force_enter, is_queued))
        return true;
  return false;
}

std::optional<sim::TraitPair<sim::IDockHost>>
DockClientManager::ClosestDock(sim::IDockHost* ignore,
                               core::BitSet<sim::DockType> type,
                               bool force_enter,
                               bool ignore_occupancy) const {
  // L304-312
  std::vector<sim::IDockClient*> vec_clients;
  if (type.IsEmpty())
    vec_clients = vec_dock_clients_;
  else
    vec_clients = AvailableDockClients(type, force_enter);

  std::vector<sim::TraitPair<sim::IDockHost>> vec_docks;
  for (auto& [actor, trait] :
       p_self_->world().ActorsWithTrait<sim::IDockHost>())
    if (trait != ignore) {
      bool b_viable = false;
      for (sim::IDockClient* client : vec_clients)
        if (client->CanDockAt(*actor, trait, force_enter,
                              ignore_occupancy)) {
          b_viable = true;
          break;
        }
      if (b_viable)
        vec_docks.push_back(sim::TraitPair<sim::IDockHost>{actor, trait});
    }

  // 成员名遮蔽:限定自由函数(DockExts.ClosestDock)
  // The member name shadows: qualify the free function
  // (DockExts.ClosestDock).
  return ora::mods::ClosestDock(vec_docks, *p_self_, *this);
}

std::vector<sim::TraitPair<sim::IDockHost>>
DockClientManager::AvailableDockHosts(
    Actor& target, core::BitSet<sim::DockType> type, bool force_enter,
    bool ignore_occupancy) const {
  // L316-323
  std::vector<sim::IDockClient*> vec_clients =
      type.IsEmpty() ? vec_dock_clients_
                     : AvailableDockClients(type, force_enter);

  std::vector<sim::TraitPair<sim::IDockHost>> vec_out;
  for (sim::IDockHost* host :
       target.TraitsImplementing<sim::IDockHost>()) {
    for (sim::IDockClient* client : vec_clients)
      if (client->CanDockAt(target, host, force_enter, ignore_occupancy)) {
        vec_out.push_back(
            sim::TraitPair<sim::IDockHost>{&target, host});
        break;
      }
  }
  return vec_out;
}

std::vector<sim::IDockClient*> DockClientManager::AvailableDockClients(
    const core::BitSet<sim::DockType>& type, bool force_enter) const {
  // L327-330
  std::vector<sim::IDockClient*> vec_out;
  for (sim::IDockClient* client : vec_dock_clients_)
    if (client->CanDock(type, force_enter))
      vec_out.push_back(client);
  return vec_out;
}

void DockClientManager::Killed(Actor& /*self*/,
                               const sim::AttackInfo& /*e*/) {
  // L332
  UnreserveHost();
}

void DockClientManager::Disposing(Actor& /*self*/) {
  // L334
  UnreserveHost();
}

std::vector<sim::IDockHost*> DockClientManager::GetDockableHosts(
    Actor& target, bool force_enter, bool is_queued) const {
  // L296-300
  std::vector<sim::IDockHost*> vec_out;
  for (sim::IDockHost* host :
       target.TraitsImplementing<sim::IDockHost>())
    for (sim::IDockClient* client : vec_dock_clients_)
      if (client->CanQueueDockAt(target, host, force_enter, is_queued)) {
        vec_out.push_back(host);
        break;
      }
  return vec_out;
}

void DockClientManager::RequireForceMoveConditionChanged(
    Actor& /*self*/, sim::ConditionCacheView conditions) {
  // L255-258
  b_require_force_move_ =
      info_.expr_require_force_move->Evaluate(
          sim::ConditionSymbols(conditions));
}

// ———— DockActorTargeter(L337-394)————

DockActorTargeter::DockActorTargeter(
    int priority,
    std::function<CanTargetResult(const CanTargetContext&)> fn_can_target)
    : int4_priority_{priority}, fn_can_target_{std::move(fn_can_target)} {}

bool DockActorTargeter::TargetOverridesSelection(
    Actor& /*self*/, const sim::Target& /*target*/,
    std::span<Actor* const> /*actors_at*/, CPos /*xy*/,
    sim::TargetModifiers /*modifiers*/) {
  return true;
}

bool DockActorTargeter::CanTarget(Actor& /*self*/,
                                  const sim::Target& target,
                                  sim::TargetModifiers& modifiers,
                                  std::string& cursor) {
  // L352-366
  // TODO: support frozen actors(上游注释)
  if (target.Type() != sim::TargetType::Actor)
    return false;

  const bool b_force_enter =
      sim::HasModifier(modifiers, sim::TargetModifiers::ForceMove);
  b_is_queued_ =
      sim::HasModifier(modifiers, sim::TargetModifiers::ForceQueue);
  str_order_id_ = b_force_enter ? "ForceDock" : "Dock";

  CanTargetContext context;
  context.target = target;
  context.b_is_queued = b_is_queued_;
  context.b_force_enter = b_force_enter;
  const CanTargetResult result = fn_can_target_(context);
  cursor = result.str_cursor;
  return result.b_can_target;
}

// ———— DockExts.ClosestDock(L397-437)————

std::optional<sim::TraitPair<sim::IDockHost>> ClosestDock(
    std::span<const sim::TraitPair<sim::IDockHost>> vec_docks,
    Actor& client_actor, const DockClientManager& client) {
  // L399-436
  if (Mobile* mobile = client_actor.TraitOrDefault<Mobile>();
      mobile != nullptr) {
    // Overlapping hosts can become hidden.(上游注释)
    // AggregateBy(dock → CellContaining(dock.DockPosition)) → 首值 map
    // AggregateBy (dock → CellContaining(dock.DockPosition)) → the
    // first-value map.
    std::map<CPos, sim::TraitPair<sim::IDockHost>> map_lookup;
    for (const auto& dock : vec_docks) {
      const CPos cell = client_actor.world().Map().CellContaining(
          dock.trait->DockPosition());
      map_lookup.emplace(cell, dock);
    }

    // Start a search from each client actor:(上游注释)
    std::vector<CPos> vec_targets;
    vec_targets.reserve(map_lookup.size());
    for (const auto& [cell, dock] : map_lookup)
      vec_targets.push_back(cell);

    const int occupancy_cost_modifier = client.OccupancyCostModifier();
    const std::vector<CPos> vec_path =
        mobile->PathFinder()->FindPathToTargetCells(
            &client_actor, client_actor.Location(), vec_targets,
            sim::BlockedByActor::None,
            [&map_lookup, occupancy_cost_modifier](CPos location) {
              const auto it_find = map_lookup.find(location);
              if (it_find == map_lookup.end())
                return 0;

              // Prefer docks with less occupancy (multiplier is to offset
              // distance cost): TODO: add custom weights. E.g. owner vs
              // allied.(上游注释)
              return it_find->second.trait->ReservationCount() *
                     occupancy_cost_modifier;
            });

    if (!vec_path.empty()) {
      const auto it_find = map_lookup.find(vec_path[0]);
      if (it_find != map_lookup.end())
        return it_find->second;
    }
  } else {
    // OrderBy(...).FirstOrDefault 的稳定序单遍择优
    // OrderBy(...).FirstOrDefault's stable-order single-pass pick.
    const sim::TraitPair<sim::IDockHost>* best = nullptr;
    std::int64_t best_cost = 0;
    for (const auto& dock : vec_docks) {
      const std::int64_t cost =
          (client_actor.Location() -
           client_actor.world().Map().CellContaining(
               dock.trait->DockPosition()))
              .LengthSquared() +
          dock.trait->ReservationCount() * client.OccupancyCostModifier();
      if (best == nullptr || cost < best_cost) {
        best = &dock;
        best_cost = cost;
      }
    }
    if (best != nullptr)
      return *best;
  }

  return std::nullopt;
}

}  // namespace ora::mods
