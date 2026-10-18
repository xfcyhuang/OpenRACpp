// UPSTREAM: OpenRA.Mods.Common/Traits/Passenger.cs +
//          Orders/EnterAlliedActorTargeter.cs @e180e3225
#include "mods/passenger.hpp"

#include "game/actor_info.hpp"
#include "meta/type_registry.hpp"
#include "mods/cargo.hpp"
#include "mods/cargo_activities.hpp"
#include "mods/move_activities.hpp"
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

std::optional<core::Color> FieldColor(const meta::RecordObject& rec_info,
                                      std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* n = std::get_if<std::int64_t>(&gv->val))
    return core::Color::FromArgbRaw(static_cast<std::uint32_t>(*n));
  return std::nullopt;
}
std::vector<std::pair<std::string, std::string>> FieldStringDict(
    const meta::RecordObject& rec_info, std::string_view str_name) {
  std::vector<std::pair<std::string, std::string>> vec_out;
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return vec_out;
  if (const auto* dict =
          std::get_if<std::vector<meta::GenericValue>>(&gv->val))
    for (const auto& element : *dict)
      if (const auto* pair =
              std::get_if<std::vector<meta::GenericValue>>(&element.val)) {
        if (pair->size() != 2)
          continue;
        const auto* key = std::get_if<std::string>(&(*pair)[0].val);
        const auto* value = std::get_if<std::string>(&(*pair)[1].val);
        if (key != nullptr && value != nullptr)
          vec_out.emplace_back(*key, *value);
      }
  return vec_out;
}

bool InfoHasCargoTrait(const Actor& target) {
  for (const meta::RecordObject* rec : target.Info()->TraitsInConstructOrder()) {
    const meta::RecordDesc* desc_cur = &rec->record_desc();
    while (desc_cur != nullptr) {
      if (desc_cur->str_name == std::string_view{"CargoInfo"})
        return true;
      desc_cur = desc_cur->str_base.empty()
                     ? nullptr
                     : meta::TypeRegistry::FindType(desc_cur->str_base);
    }
  }
  return false;
}

}  // namespace

PassengerInfoData PassengerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  PassengerInfoData data;
  if (const auto v = FieldString(rec_info, "CargoType"))
    data.str_cargo_type = *v;
  if (const auto v = FieldString(rec_info, "CustomPipType"))
    data.str_custom_pip_type = *v;
  if (const auto v = FieldInt(rec_info, "Weight"))
    data.int4_weight = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "CargoCondition"))
    data.str_cargo_condition = *v;
  data.vec_cargo_conditions = FieldStringDict(rec_info, "CargoConditions");
  if (const auto v = FieldString(rec_info, "Voice"))
    data.str_voice = *v;
  if (const auto v = FieldColor(rec_info, "TargetLineColor"))
    data.color_target_line = *v;
  if (const auto v = FieldString(rec_info, "RequireForceMoveCondition"))
    if (!v->empty())
      data.expr_require_force_move =
          expr::BooleanExpression{std::string{*v}};
  if (const auto v = FieldString(rec_info, "EnterCursor"))
    data.str_enter_cursor = *v;
  if (const auto v = FieldString(rec_info, "EnterBlockedCursor"))
    data.str_enter_blocked_cursor = *v;
  return data;
}

Passenger::Passenger(const PassengerInfoData& info)
    : info_{info} {}

bool Passenger::EnterTransportTargeter::TargetOverridesSelection(
    Actor&, const sim::Target&, std::span<Actor* const>, CPos,
    sim::TargetModifiers) {
  return true;
}

bool Passenger::EnterTransportTargeter::CanTarget(
    Actor& self, const sim::Target& target, sim::TargetModifiers& modifiers,
    std::string& cursor) {
  const sim::TargetType type = target.type;
  if (type != sim::TargetType::Actor && type != sim::TargetType::FrozenActor)
    return false;

  cursor = info_.str_enter_cursor;
  b_is_queued_ = sim::HasModifier(modifiers, sim::TargetModifiers::ForceQueue);

  if (type == sim::TargetType::FrozenActor)
    return false;

  const Player* owner = target.ActorPtr->Owner();
  const sim::PlayerRelationship relationship =
      self.Owner()->RelationshipWith(owner);
  if (!sim::HasModifier(modifiers, sim::TargetModifiers::ForceAttack) &&
      relationship == sim::PlayerRelationship::Enemy)
    return false;

  auto* target_actor = const_cast<Actor*>(target.ActorPtr);
  if (!self.Owner()->IsAlliedWith(target_actor->Owner()))
    return false;
  if (!InfoHasCargoTrait(*target_actor))
    return false;
  if (!passenger_.IsCorrectCargoType(*target_actor, modifiers))
    return false;

  cursor = passenger_.CanEnter(*target_actor) ? info_.str_enter_cursor
                                             : info_.str_enter_blocked_cursor;
  return true;
}

std::vector<sim::IOrderTargeter*> Passenger::Orders() {
  std::vector<sim::IOrderTargeter*> vec_orders;
  if (!ptr_order_targeter_)
    ptr_order_targeter_ = std::make_unique<EnterTransportTargeter>(
        *this, info_);
  vec_orders.push_back(ptr_order_targeter_.get());
  return vec_orders;
}

net::Order* Passenger::IssueOrder(Actor& self, sim::IOrderTargeter* order,
                                  const sim::Target& target, bool queued) {
  if (order->OrderID() == "EnterTransport")
    return new net::Order(order->OrderID(), &self, target, queued);

  return nullptr;
}

bool Passenger::IsCorrectCargoType(Actor& target,
                                   sim::TargetModifiers modifiers) {
  if (b_require_force_move_ &&
      !sim::HasModifier(modifiers, sim::TargetModifiers::ForceMove))
    return false;

  return IsCorrectCargoType(target);
}

bool Passenger::IsCorrectCargoType(Actor& target) {
  auto* cargo = target.Trait<Cargo>();
  return !cargo->IsTraitDisabled() &&
         std::find(cargo->Info().vec_types.begin(),
                   cargo->Info().vec_types.end(),
                   info_.str_cargo_type) != cargo->Info().vec_types.end();
}

bool Passenger::CanEnter(Cargo* cargo) {
  return cargo != nullptr && !cargo->IsTraitDisabled() &&
         cargo->HasSpace(info_.int4_weight);
}

bool Passenger::CanEnter(Actor& target) {
  return CanEnter(target.TraitOrDefault<Cargo>());
}

std::string Passenger::VoicePhraseForOrder(Actor& self,
                                           const net::Order& order) {
  if (order.str_order_string != "EnterTransport")
    return {};

  if (order.target.Type() != sim::TargetType::Actor ||
      !CanEnter(*const_cast<Actor*>(order.target.ActorPtr)))
    return {};

  return info_.str_voice;
}

void Passenger::ResolveOrder(Actor& self, const net::Order& order) {
  if (order.str_order_string != "EnterTransport")
    return;

  if (order.target.Type() != sim::TargetType::Actor)
    return;

  auto* target_actor = const_cast<Actor*>(order.target.ActorPtr);
  if (!CanEnter(*target_actor))
    return;

  if (!IsCorrectCargoType(*target_actor))
    return;

  self.QueueActivity(
      order.b_queued,
      self.world().Arena().Create<activities::RideTransport>(
          self, order.target,
          std::optional<core::Color>{info_.color_target_line}));
  self.ShowTargetLines();
}

void Passenger::OnEnteredCargo(Actor& self, Actor& cargo) {
  if (int4_any_cargo_token == sim::Actor::InvalidConditionToken)
    int4_any_cargo_token =
        self.GrantCondition(info_.str_cargo_condition);

  if (int4_specific_cargo_token == sim::Actor::InvalidConditionToken) {
    for (const auto& [name, condition] : info_.vec_cargo_conditions)
      if (name == cargo.InfoName()) {
        int4_specific_cargo_token = self.GrantCondition(condition);
        break;
      }
  }

  if (self.IsIdle()) {
    auto* move = self.TraitOrDefault<sim::IMove>();
    if (move != nullptr)
      self.QueueActivity(move->ReturnToCell(&self));
  }
}

void Passenger::OnExitedCargo(Actor& self, Actor&) {
  if (int4_any_cargo_token != sim::Actor::InvalidConditionToken)
    int4_any_cargo_token =
        self.RevokeCondition(int4_any_cargo_token);

  if (int4_specific_cargo_token != sim::Actor::InvalidConditionToken)
    int4_specific_cargo_token =
        self.RevokeCondition(int4_specific_cargo_token);
}

bool Passenger::Reserve(Actor& self, Cargo& cargo) {
  if (&cargo == ptr_reserved_cargo_)
    return true;

  Unreserve(self);
  if (!cargo.ReserveSpace(self))
    return false;

  ptr_reserved_cargo_ = &cargo;
  return true;
}

void Passenger::Unreserve(Actor& self) {
  if (ptr_reserved_cargo_ == nullptr)
    return;

  ptr_reserved_cargo_->UnreserveSpace(self);
  ptr_reserved_cargo_ = nullptr;
}

void Passenger::RemovedFromWorld(Actor& self) { Unreserve(self); }

void Passenger::OnBeforeAddedToWorld(Actor& actor) {
  actor.CancelActivity();
}

void Passenger::OnEjectedFromKilledCargo(Actor& self) {
  if (self.CurrentActivity() != nullptr)
    self.CurrentActivity()->Cancel(self);

  self.QueueActivity(
      self.world().Arena().Create<activities::Nudge>(&self));
}

void Passenger::Killed(Actor& self, const sim::AttackInfo& e) {
  if (p_transport_ == nullptr)
    return;

  if (!p_transport_->IsDead()) {
    Cargo* transport = p_transport_->Trait<Cargo>();
    transport->Unload(*p_transport_, &self);
  }
}

std::vector<sim::VariableObserver> Passenger::GetVariableObservers() {
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

void Passenger::RequireForceMoveConditionChanged(
    Actor&, sim::ConditionCacheView conditions) {
  const std::vector<std::pair<std::string, std::int32_t>> vec_symbols{
      conditions.begin(), conditions.end()};
  b_require_force_move_ =
      info_.expr_require_force_move->EvaluateBool(vec_symbols);
}

}  // namespace ora::mods
