// UPSTREAM: OpenRA.Mods.Common/Traits/Carryall.cs + Carryable.cs +
//          AutoCarryall.cs + AutoCarryable.cs + CarryableHarvester.cs @e180e3225
#include "mods/carryall.hpp"

#include "game/actor_info.hpp"
#include "gfx/actor_preview.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/type_registry.hpp"
#include "mods/actor_exts.hpp"
#include "mods/air_activities.hpp"
#include "mods/aircraft.hpp"
#include "mods/body_orientation.hpp"
#include "mods/carryall_activities.hpp"
#include "mods/mobile.hpp"
#include "mods/pathfinding/locomotor.hpp"
#include "mods/pathfinding/path_finder.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/screen_map.hpp"
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

std::optional<WVec> FieldWVec(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
    return WVec{static_cast<std::int32_t>(tuple->arr_ints[0]),
                static_cast<std::int32_t>(tuple->arr_ints[1]),
                static_cast<std::int32_t>(tuple->arr_ints[2])};
  return std::nullopt;
}

std::optional<WDist> FieldWDist(const meta::RecordObject& rec_info,
                                std::string_view str_name) {
  const auto v = FieldInt(rec_info, str_name);
  if (!v.has_value())
    return std::nullopt;
  return WDist{static_cast<std::int32_t>(*v)};
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
  if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
    for (const auto& [key, value] : *dict)
      if (const auto* k = std::get_if<std::string>(&key.val))
        if (const auto* v = std::get_if<std::string>(&value.val))
          vec_out.emplace_back(*k, *v);
  return vec_out;
}

std::string ToLowerInvariant(std::string str_value) {
  for (char& c : str_value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return str_value;
}

}  // namespace

// ———— Carryable(L45-148)————

CarryableInfoData CarryableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  CarryableInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldString(rec_info, "ReservedCondition"))
    data.str_reserved_condition = *v;
  if (const auto v = FieldString(rec_info, "CarriedCondition"))
    data.str_carried_condition = *v;
  if (const auto v = FieldString(rec_info, "LockedCondition"))
    data.str_locked_condition = *v;
  if (const auto v = FieldWVec(rec_info, "LocalOffset"))
    data.wvec_local_offset = *v;
  return data;
}

Carryable::Carryable(const CarryableInfoData& info)
    : sim::ConditionalTraitCore<Carryable>(info.conditional), info_{info} {}

void Carryable::Created(Actor& self) {
  p_mobile_ = self.TraitOrDefault<Mobile>();
  vec_delay_pickups_ = self.TraitsImplementing<IDelayCarryallPickup>();

  sim::ConditionalTraitCore<Carryable>::CoreCreated(self);
}

void Carryable::Attached(Actor& self, Actor* carrier) {
  if (b_attached_)
    return;

  b_attached_ = true;
  p_carrier_ = carrier;

  if (int4_carried_token == sim::Actor::InvalidConditionToken)
    int4_carried_token = self.GrantCondition(info_.str_carried_condition);
}

void Carryable::Detached(Actor& self) {
  if (!b_attached_)
    return;

  b_attached_ = false;

  if (int4_carried_token != sim::Actor::InvalidConditionToken)
    int4_carried_token = self.RevokeCondition(int4_carried_token);
}

bool Carryable::Reserve(Actor& self, Actor* carrier) {
  if (Reserved() || IsTraitDisabled())
    return false;

  state_ = CarryableState::Reserved;
  p_carrier_ = carrier;

  if (int4_reserved_token == sim::Actor::InvalidConditionToken)
    int4_reserved_token =
        self.GrantCondition(info_.str_reserved_condition);

  return true;
}

void Carryable::UnReserve(Actor& self) {
  state_ = CarryableState::Free;
  p_carrier_ = nullptr;

  if (int4_reserved_token != sim::Actor::InvalidConditionToken)
    int4_reserved_token = self.RevokeCondition(int4_reserved_token);

  if (int4_locked_token != sim::Actor::InvalidConditionToken)
    int4_locked_token = self.RevokeCondition(int4_locked_token);
}

LockResponse Carryable::LockForPickup(Actor& self, Actor* carrier) {
  if (state_ == CarryableState::Locked && p_carrier_ != carrier)
    return LockResponse::Failed;

  for (IDelayCarryallPickup* d : vec_delay_pickups_) {
    auto* trait_base = dynamic_cast<sim::TraitBase*>(d);
    if (trait_base != nullptr && trait_base->IsTraitEnabled() &&
        !d->TryLockForPickup(self, *carrier))
      return LockResponse::Pending;
  }

  if (p_mobile_ != nullptr && !p_mobile_->CanStayInCell(self.Location()))
    return LockResponse::Pending;

  if (state_ != CarryableState::Locked) {
    state_ = CarryableState::Locked;
    p_carrier_ = carrier;

    if (int4_locked_token == sim::Actor::InvalidConditionToken)
      int4_locked_token = self.GrantCondition(info_.str_locked_condition);
  }

  if (p_mobile_ != nullptr && p_mobile_->IsMovingBetweenCells())
    return LockResponse::Pending;

  return LockResponse::Success;
}

// ———— AutoCarryable(L26-154)————

AutoCarryableInfoData AutoCarryableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AutoCarryableInfoData data;
  data.carryable = CarryableInfoData::Parse(rec_info);
  if (const auto v = FieldWDist(rec_info, "MinDistance"))
    data.dist_min_distance = *v;
  return data;
}

AutoCarryable::AutoCarryable(const AutoCarryableInfoData& info)
    : Carryable{info.carryable}, auto_info_{info} {}

void AutoCarryable::MovementCancelledInner() {

  if (state_ == CarryableState::Locked)
    return;

  opt_destination_ = std::nullopt;
  b_auto_reserved_ = false;
}

void AutoCarryable::RequestTransport(Actor& self, CPos destination) {
  if (!IsValidAutoCarryDistance(self, destination)) {
    opt_destination_ = std::nullopt;
    return;
  }

  opt_destination_ = destination;

  if (state_ != CarryableState::Free)
    return;

  auto vec_carriers = self.world().ActorsWithTrait<AutoCarryall>();
  std::sort(vec_carriers.begin(), vec_carriers.end(),
            [&self](const sim::TraitPair<AutoCarryall>& a,
                    const sim::TraitPair<AutoCarryall>& b) {
              return (self.Location() - a.actor->Location())
                         .LengthSquared() <
                     (self.Location() - b.actor->Location()).LengthSquared();
            });

  for (const auto& carrier : vec_carriers) {
    if (carrier.trait->State() != CarryallState::Idle ||
        carrier.trait->IsTraitDisabled() ||
        !carrier.trait->EnableAutoCarry() || carrier.actor->IsDead() ||
        carrier.actor->Owner() != self.Owner() ||
        !carrier.actor->IsInWorld())
      continue;

    if (carrier.trait->RequestTransportNotify(*carrier.actor, &self))
      return;
  }
}

void AutoCarryable::Detached(Actor& self) {
  if (!b_attached_)
    return;

  opt_destination_ = std::nullopt;

  Carryable::Detached(self);
}

bool AutoCarryable::AutoReserve(Actor& self, Actor* carrier) {
  if (Reserved() || !WantsTransport()) {
    return false;
  }

  if (auto destination = opt_destination_;
      destination.has_value() && !IsValidAutoCarryDistance(self, *destination)) {
    MovementCancelledInner();
    return false;
  }

  if (Reserve(self, carrier)) {
    b_auto_reserved_ = true;
    return true;
  }

  return false;
}

LockResponse AutoCarryable::LockForPickup(Actor& self, Actor* carrier) {
  if (state_ == CarryableState::Locked && p_carrier_ != carrier)
    return LockResponse::Failed;

  if (b_auto_reserved_) {
    if (!WantsTransport()) {
      MovementCancelledInner();
      return LockResponse::Failed;
    }

    if (auto destination = opt_destination_; destination.has_value() &&
        !IsValidAutoCarryDistance(self, *destination)) {
      MovementCancelledInner();
      return LockResponse::Failed;
    }

    b_auto_reserved_ = false;
  }

  return Carryable::LockForPickup(self, carrier);
}

bool AutoCarryable::IsValidAutoCarryDistance(Actor& self, CPos destination) {
  if (p_mobile_ == nullptr)
    return false;

  return (self.world().Map().CenterOfCell(destination) -
          self.CenterPosition())
                 .HorizontalLengthSquared() >=
             auto_info_.dist_min_distance.LengthSquared() ||
         !p_mobile_->PathFinder()->PathExistsForLocomotor(
             p_mobile_->Locomotor(), self.Location(), destination);
}

// ———— Carryall(L87-465)————

CarryallInfoData CarryallInfoData::Parse(const meta::RecordObject& rec_info) {
  CarryallInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldString(rec_info, "InitialActor"))
    data.str_initial_actor = *v;
  if (const auto v = FieldInt(rec_info, "BeforeLoadDelay"))
    data.int4_before_load_delay = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "BeforeUnloadDelay"))
    data.int4_before_unload_delay = static_cast<int>(*v);
  if (const auto v = FieldWVec(rec_info, "LocalOffset"))
    data.wvec_local_offset = *v;
  if (const auto v = FieldWDist(rec_info, "DropRange"))
    data.dist_drop_range = *v;
  if (const auto v = FieldString(rec_info, "UnloadCursor"))
    data.str_unload_cursor = *v;
  if (const auto v = FieldString(rec_info, "UnloadBlockedCursor"))
    data.str_unload_blocked_cursor = *v;
  if (const auto v = FieldBool(rec_info, "AllowDropOff"))
    data.b_allow_drop_off = *v;
  if (const auto v = FieldString(rec_info, "DropOffCursor"))
    data.str_drop_off_cursor = *v;
  if (const auto v = FieldString(rec_info, "DropOffBlockedCursor"))
    data.str_drop_off_blocked_cursor = *v;
  if (const auto v = FieldString(rec_info, "PickUpCursor"))
    data.str_pick_up_cursor = *v;
  if (const auto v = FieldString(rec_info, "CarryCondition"))
    data.str_carry_condition = *v;
  data.vec_carryable_conditions =
      FieldStringDict(rec_info, "CarryableConditions");
  if (const auto v = FieldString(rec_info, "Voice"))
    data.str_voice = *v;
  if (const auto v = FieldColor(rec_info, "TargetLineColor"))
    data.color_target_line = *v;
  return data;
}

Carryall::Carryall(sim::ActorInitializer& init, const CarryallInfoData& info)
    : sim::ConditionalTraitCore<Carryall>(info.conditional),
      info_{info},
      self_{init.Self()} {
  aircraft_ = self_.Trait<Aircraft>();
  body_ = self_.Trait<BodyOrientation>();
  facing_ = self_.Trait<sim::IFacing>();

  if (!info_.str_initial_actor.empty()) {
    sim::TypeDictionary dict;
    dict.Add(std::make_unique<sim::OwnerInit>(self_.Owner()));
    auto* cargo = self_.world().CreateActor(
        false, ToLowerInvariant(info_.str_initial_actor), dict);

    cargo->Trait<Carryable>()->Attached(*cargo, &self_);
    AttachCarryable(self_, cargo);
  }
}

void Carryall::Created(sim::Actor& self) {
  sim::ConditionalTraitCore<Carryall>::CoreCreated(self);
}

void Carryall::Tick(Actor& self) {
  if (carryall_state_ == CarryallState::Carrying &&
      (p_carryable_ == nullptr || p_carryable_->IsDead()))
    DetachCarryable(self);

  if (facing_->Facing() != angle_cached_facing_ &&
      !vec_carryable_preview_.empty()) {
    self.world().ScreenMapFace()->AddOrUpdate(&self);
    angle_cached_facing_ = facing_->Facing();
  }
}

void Carryall::Disposing(Actor& self) {
  if (carryall_state_ == CarryallState::Carrying) {
    p_carryable_->Dispose();
    p_carryable_ = nullptr;
  }

  UnreserveCarryable(self);
}

void Carryall::Killed(Actor& self, const sim::AttackInfo& e) {
  if (carryall_state_ == CarryallState::Carrying) {
    if (!p_carryable_->IsDead()) {
      auto* positionable = p_carryable_->Trait<sim::IPositionable>();
      positionable->SetPosition(p_carryable_, self.Location(),
                                sim::SubCell::Any);
      p_carryable_->Kill(const_cast<Actor*>(e.Attacker));
    }

    p_carryable_ = nullptr;
  }

  UnreserveCarryable(self);
}

WVec Carryall::OffsetForCarryable(Actor& self, Actor& carryable) {
  return info_.wvec_local_offset -
         carryable.Trait<Carryable>()->Info().wvec_local_offset;
}

WVec Carryall::PositionOffset() const {
  const WVec local_offset = wvec_carryable_offset_.Rotate(
      body_->QuantizeOrientation(self_.Orientation()));
  return body_->LocalToWorld(local_offset);
}

std::span<const std::string> Carryall::LandableTerrainTypes() {
  if (opt_landable_terrain_types_.has_value())
    return std::span<const std::string>{*opt_landable_terrain_types_};
  return std::span<const std::string>{
      aircraft_->Info().vec_landable_terrain_types};
}

bool Carryall::AttachCarryable(Actor& self, Actor* carryable) {
  if (carryall_state_ == CarryallState::Carrying)
    return false;

  p_carryable_ = carryable;
  carryall_state_ = CarryallState::Carrying;
  self.world().ScreenMapFace()->AddOrUpdate(&self);
  if (int4_carry_condition_token == sim::Actor::InvalidConditionToken)
    int4_carry_condition_token =
        self.GrantCondition(info_.str_carry_condition);

  for (const auto& [name, condition] : info_.vec_carryable_conditions)
    if (name == carryable->InfoName()) {
      int4_carryable_condition_token = self.GrantCondition(condition);
      break;
    }

  wvec_carryable_offset_ = OffsetForCarryable(self, *carryable);
  opt_landable_terrain_types_ = std::vector<std::string>{};
  for (const auto& [terrain, entry] :
       carryable->Trait<Mobile>()->Locomotor()->Info().TerrainSpeeds())
    opt_landable_terrain_types_->push_back(terrain);

  return true;
}

void Carryall::DetachCarryable(Actor& self) {
  UnreserveCarryable(self);
  self.world().ScreenMapFace()->AddOrUpdate(&self);
  if (int4_carry_condition_token != sim::Actor::InvalidConditionToken)
    int4_carry_condition_token =
        self.RevokeCondition(int4_carry_condition_token);

  if (int4_carryable_condition_token != sim::Actor::InvalidConditionToken)
    int4_carryable_condition_token =
        self.RevokeCondition(int4_carryable_condition_token);

  vec_carryable_preview_.clear();
  opt_landable_terrain_types_ = std::nullopt;
  wvec_carryable_offset_ = WVec{0, 0, 0};
}

bool Carryall::ReserveCarryable(Actor& self, Actor* carryable) {
  if (carryall_state_ == CarryallState::Reserved)
    UnreserveCarryable(self);

  if (carryall_state_ != CarryallState::Idle ||
      !carryable->Trait<Carryable>()->Reserve(*carryable, &self))
    return false;

  p_carryable_ = carryable;
  carryall_state_ = CarryallState::Reserved;
  return true;
}

void Carryall::UnreserveCarryable(Actor& self) {
  if (p_carryable_ != nullptr && p_carryable_->IsInWorld() &&
      !p_carryable_->IsDead()) {
    auto* carryable = p_carryable_->Trait<Carryable>();
    if (carryable->Carrier() == &self)
      carryable->UnReserve(*p_carryable_);
  }

  p_carryable_ = nullptr;
  carryall_state_ = CarryallState::Idle;
}

void Carryall::Render(Actor& self, gfx::WorldRenderer& wr,
                      std::vector<gfx::RenderItem>& vec_out) {
  if (carryall_state_ == CarryallState::Carrying &&
      !p_carryable_->IsDead()) {
    if (vec_carryable_preview_.empty()) {
      sim::TypeDictionary carryable_inits;
      carryable_inits.Add(std::make_unique<sim::OwnerInit>(p_carryable_->Owner()));
      carryable_inits.Add(std::make_unique<sim::DynamicFacingInit>(
          [this]() { return facing_->Facing(); }));

      for (auto* api :
           p_carryable_->TraitsImplementing<sim::IActorPreviewInitModifier>())
        api->ModifyActorPreviewInit(*p_carryable_, carryable_inits);

      gfx::ActorPreviewInitializer init{*p_carryable_->Info(), wr,
                                        carryable_inits};
      vec_carryable_preview_ =
          gfx::RenderActorPreviewOf(*p_carryable_->Info(), init);
    }

    const WVec offset = body_->LocalToWorld(wvec_carryable_offset_.Rotate(
        body_->QuantizeOrientation(self.Orientation())));
    for (auto& p : vec_carryable_preview_)
      p->Render(wr, self.CenterPosition() + offset, vec_out);
  }
}

std::vector<Rectangle> Carryall::ScreenBounds(Actor& self,
                                              gfx::WorldRenderer& wr) {
  std::vector<Rectangle> vec_out;
  if (vec_carryable_preview_.empty())
    return vec_out;

  const WPos pos = self.CenterPosition();
  for (auto& p : vec_carryable_preview_)
    p->ScreenBounds(wr, pos, vec_out);
  return vec_out;
}

bool Carryall::CanUnload() {
  if (IsTraitDisabled())
    return false;

  return p_carryable_ != nullptr &&
         aircraft_->CanLand(self_.Location(), nullptr, false);
}

std::vector<sim::IOrderTargeter*> Carryall::Orders() {
  std::vector<sim::IOrderTargeter*> vec_orders;
  if (IsTraitDisabled())
    return vec_orders;

  if (!ptr_pickup_targeter_)
    ptr_pickup_targeter_ =
        std::make_unique<CarryallPickupOrderTargeter>(info_);
  if (!ptr_unload_targeter_)
    ptr_unload_targeter_ = std::make_unique<DeployOrderTargeter>(
        "Unload", 10, [this]() {
          return CanUnload() ? info_.str_unload_cursor
                             : info_.str_unload_blocked_cursor;
        });
  if (!ptr_deliver_targeter_)
    ptr_deliver_targeter_ =
        std::make_unique<CarryallDeliverUnitTargeter>(
            aircraft_->Info().b_move_into_shroud, info_);
  vec_orders.push_back(ptr_pickup_targeter_.get());
  vec_orders.push_back(ptr_unload_targeter_.get());
  vec_orders.push_back(ptr_deliver_targeter_.get());
  return vec_orders;
}

std::unique_ptr<net::Order> Carryall::IssueOrder(
    Actor& self, sim::IOrderTargeter* order,
    const sim::Target& target, bool queued) {
  if (order->OrderID() == "PickupUnit" ||
      order->OrderID() == "DeliverUnit" || order->OrderID() == "Unload")
    return std::make_unique<net::Order>(order->OrderID(), &self, target,
                                        queued);

  return nullptr;
}

net::Order Carryall::IssueDeployOrder(Actor& self, bool queued) {
  return net::Order("Unload", &self, queued);
}

bool Carryall::CanIssueDeployOrder(Actor&, bool) {
  return !IsTraitDisabled();
}

void Carryall::ResolveOrder(Actor& self, const net::Order& order) {
  if (order.str_order_string == "DeliverUnit") {
    if (!order.target.IsValidFor(&self))
      return;

    const CPos cell = self.world().Map().Clamp(
        self.world().Map().CellContaining(order.target.CenterPosition()));
    if (!aircraft_->Info().b_move_into_shroud &&
        !self.Owner()->GetShroud()->IsExplored(cell))
      return;

    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::DeliverUnit>(
            self, order.target, info_.dist_drop_range,
            std::optional<core::Color>{info_.color_target_line}));
    self.ShowTargetLines();
  } else if (order.str_order_string == "Unload") {
    if (!order.b_queued && !CanUnload())
      return;

    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::DeliverUnit>(
            self, info_.dist_drop_range,
            std::optional<core::Color>{info_.color_target_line}));
  } else if (order.str_order_string == "PickupUnit") {
    if (order.target.Type() != sim::TargetType::Actor)
      return;

    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::PickupUnit>(
            self, const_cast<Actor*>(order.target.ActorPtr),
            info_.int4_before_load_delay,
            std::optional<core::Color>{info_.color_target_line}));
    self.ShowTargetLines();
  }
}

std::string Carryall::VoicePhraseForOrder(Actor& self,
                                          const net::Order& order) {
  if (order.str_order_string == "DeliverUnit" ||
      order.str_order_string == "Unload" ||
      order.str_order_string == "PickupUnit")
    return info_.str_voice;

  return {};
}

bool Carryall::CarryallPickupOrderTargeter::CanTarget(
    Actor& self, const sim::Target& target, sim::TargetModifiers& modifiers,
    std::string& cursor) {
  const sim::TargetType type = target.type;
  if (type != sim::TargetType::Actor && type != sim::TargetType::FrozenActor)
    return false;

  cursor = info_.str_pick_up_cursor;
  b_is_queued_ = sim::HasModifier(modifiers, sim::TargetModifiers::ForceQueue);

  const Player* owner =
      type == sim::TargetType::FrozenActor ? target.FrozenActorPtr->Owner()
                                           : target.ActorPtr->Owner();
  const sim::PlayerRelationship relationship =
      self.Owner()->RelationshipWith(owner);
  if (!sim::HasModifier(modifiers, sim::TargetModifiers::ForceAttack) &&
      (relationship == sim::PlayerRelationship::Ally ||
       relationship == sim::PlayerRelationship::Enemy))
    return false;

  const Actor* target_actor =
      type == sim::TargetType::FrozenActor ? target.FrozenActorPtr->ActorPtr()
                                           : target.ActorPtr;
  if (target_actor == nullptr ||
      !AppearsFriendlyTo(const_cast<Actor&>(*target_actor), self))
    return false;

  auto* carryable =
      const_cast<Actor*>(target_actor)->TraitOrDefault<Carryable>();
  if (carryable == nullptr || carryable->IsTraitDisabled())
    return false;

  if (carryable->Reserved() && carryable->Carrier() != &self)
    return false;

  return true;
}

bool Carryall::CarryallDeliverUnitTargeter::CanTarget(
    Actor& self, const sim::Target& target, sim::TargetModifiers& modifiers,
    std::string& cursor) {
  if (!info_.b_allow_drop_off ||
      !sim::HasModifier(modifiers, sim::TargetModifiers::ForceMove))
    return false;

  const sim::TargetType type = target.type;
  const game::ActorInfo* target_info = nullptr;
  if (type == sim::TargetType::Actor)
    target_info = target.ActorPtr->Info();
  else if (type == sim::TargetType::FrozenActor)
    target_info = &target.FrozenActorPtr->Info();
  if (target_info != nullptr &&
      target_info->HasTraitInfoOfInterface(
          "OpenRA.Mods.Common.Traits.BuildingInfo")) {
    cursor = info_.str_drop_off_blocked_cursor;
    return true;
  }

  const CPos location =
      self.world().Map().CellContaining(target.CenterPosition());
  const bool b_explored = self.Owner()->GetShroud()->IsExplored(location);
  cursor = self.world().Map().Contains(location) ? info_.str_drop_off_cursor
                                                 : info_.str_drop_off_blocked_cursor;

  b_is_queued_ = sim::HasModifier(modifiers, sim::TargetModifiers::ForceQueue);

  if (!b_explored && !b_move_into_shroud_)
    cursor = info_.str_drop_off_blocked_cursor;

  return true;
}

// ———— AutoCarryall(L31-150)————

AutoCarryallInfoData AutoCarryallInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AutoCarryallInfoData data;
  data.carryall = CarryallInfoData::Parse(rec_info);
  if (const auto v = FieldString(rec_info, "AutoCarryCondition"))
    if (!v->empty())
      data.expr_auto_carry_condition =
          expr::BooleanExpression{std::string{*v}};
  return data;
}

namespace {

bool Busy(Actor& self) {
  return self.CurrentActivity() != nullptr &&
         dynamic_cast<activities::FlyIdle*>(self.CurrentActivity()) ==
             nullptr;
}

Actor* ClosestToIgnoringPath(std::span<Actor* const> actors, WPos position) {
  Actor* best = nullptr;
  std::int64_t best_distance = 0;
  for (Actor* a : actors) {
    const std::int64_t distance =
        (a->CenterPosition() - position).LengthSquared();
    if (best == nullptr || distance < best_distance) {
      best = a;
      best_distance = distance;
    }
  }
  return best;
}

}  // namespace

AutoCarryall::AutoCarryall(sim::ActorInitializer& init,
                           const AutoCarryallInfoData& info)
    : Carryall{init, info.carryall}, auto_info_{info} {
  b_enable_auto_carry_ = true;
}

std::vector<sim::VariableObserver> AutoCarryall::GetVariableObservers() {
  auto vec_observers = Carryall::GetVariableObservers();
  if (auto_info_.expr_auto_carry_condition.has_value()) {
    sim::VariableObserver observer;
    observer.fn_notifier =
        [this](Actor& self, sim::ConditionCacheView conditions) {
          AutoCarryConditionsChanged(self, conditions);
        };
    observer.vec_variables.assign(
        auto_info_.expr_auto_carry_condition->Variables().begin(),
        auto_info_.expr_auto_carry_condition->Variables().end());
    vec_observers.push_back(std::move(observer));
  }
  return vec_observers;
}

void AutoCarryall::OnBecomingIdle(Actor& self) {
  if (!b_enable_auto_carry_ || IsTraitDisabled())
    return;

  FindCarryableForTransport(self);
}

bool AutoCarryall::RequestTransportNotify(Actor& self, Actor* carryable) {
  if (Busy(self) || IsTraitDisabled() || !b_enable_auto_carry_)
    return false;

  if (AutoReserveCarryable(self, carryable)) {
    self.QueueActivity(
        false, self.world().Arena().Create<activities::FerryUnit>(
                   self, carryable));
    return true;
  }

  return false;
}

bool AutoCarryall::AutoReserveCarryable(Actor& self, Actor* carryable) {
  if (carryall_state_ == CarryallState::Reserved)
    UnreserveCarryable(self);

  if (carryall_state_ != CarryallState::Idle)
    return false;

  auto* act = carryable->TraitOrDefault<AutoCarryable>();

  if (act == nullptr || !act->AutoReserve(*carryable, &self))
    return false;

  p_carryable_ = carryable;
  carryall_state_ = CarryallState::Reserved;
  return true;
}

bool AutoCarryall::IsBestAutoCarryallForCargo(Actor& self,
                                              Actor* candidate_cargo) {
  std::vector<Actor*> vec_carriers;
  for (const auto& pair :
       self.world().ActorsWithTrait<AutoCarryall>()) {
    if (!Busy(self) && pair.trait->EnableAutoCarry() &&
        pair.actor->Owner() == self.Owner() && pair.actor->IsInWorld())
      vec_carriers.push_back(pair.actor);
  }

  return ClosestToIgnoringPath(
             vec_carriers, candidate_cargo->CenterPosition()) == &self;
}

void AutoCarryall::FindCarryableForTransport(Actor& self) {
  if (!self.IsInWorld() || IsTraitDisabled())
    return;

  auto vec_pairs = self.world().ActorsWithTrait<AutoCarryable>();
  std::vector<sim::TraitPair<AutoCarryable>> vec_carryables;
  for (auto& pair : vec_pairs) {
    Actor* actor = pair.actor;
    if (actor == nullptr)
      continue;

    if (actor->Owner() != self.Owner())
      continue;

    if (actor->IsDead())
      continue;

    if (pair.trait->Reserved())
      continue;

    if (!pair.trait->WantsTransport())
      continue;

    if (actor->IsIdle())
      continue;

    vec_carryables.push_back(pair);
  }

  std::stable_sort(vec_carryables.begin(), vec_carryables.end(),
                   [&self](const sim::TraitPair<AutoCarryable>& a,
                           const sim::TraitPair<AutoCarryable>& b) {
                     return (self.Location() - a.actor->Location())
                                .LengthSquared() <
                            (self.Location() - b.actor->Location())
                                .LengthSquared();
                   });

  for (const auto& pair : vec_carryables) {
    if (IsBestAutoCarryallForCargo(self, pair.actor) &&
        AutoReserveCarryable(self, pair.actor)) {
      self.QueueActivity(
          false, self.world().Arena().Create<activities::FerryUnit>(
                     self, pair.actor));
      break;
    }
  }
}

void AutoCarryall::AutoCarryConditionsChanged(
    Actor& self, sim::ConditionCacheView conditions) {
  b_enable_auto_carry_ =
      auto_info_.expr_auto_carry_condition->Evaluate(
          sim::ConditionSymbols(conditions));
}

// ———— CarryableHarvester(L17-61)————

CarryableHarvesterInfoData CarryableHarvesterInfoData::Parse(
    const meta::RecordObject& rec_info) {
  (void)rec_info;
  return CarryableHarvesterInfoData{};
}

CarryableHarvester::CarryableHarvester(const CarryableHarvesterInfoData& info)
    : info_{info} {}

void CarryableHarvester::Created(Actor& self) {
  vec_transports_ = self.TraitsImplementing<sim::ICallForTransport>();
}

void CarryableHarvester::MovingToResources(Actor& self, CPos target_cell) {
  for (auto* t : vec_transports_)
    t->RequestTransport(self, target_cell);
}

void CarryableHarvester::MovementCancelled(Actor& self) {
  for (auto* t : vec_transports_)
    t->MovementCancelled(self);
}

void CarryableHarvester::Harvested(Actor& self,
                                   const std::string& str_resource_type) {
  (void)str_resource_type;
  for (auto* t : vec_transports_)
    t->MovementCancelled(self);
}

void CarryableHarvester::MovingToDock(Actor& self, Actor& host_actor,
                                      sim::IDockHost* host) {
  for (auto* t : vec_transports_)
    t->RequestTransport(self,
                        self.world().Map().CellContaining(host->DockPosition()));
}

// ———— [VerifySync] 哈希注册(gen/sync_gen.cpp 成员表:Carryall/
//      AutoCarryall {Carryable})————

namespace {

int CarryallSyncHash(const sim::ISync* s) {
  const auto* carryall = static_cast<const Carryall*>(s);
  return sim::sync::CombineSyncHash(
      0, sim::sync::HashActor(carryall->CarryableActor()));
}

const bool b_registered_carryall_sync_hash = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Carryall",
                                &CarryallSyncHash);
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.AutoCarryall",
                                &CarryallSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_registered_carryall_sync_hash_anchor =
    &b_registered_carryall_sync_hash;

}  // namespace

}  // namespace ora::mods
