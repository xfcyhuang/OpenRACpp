// UPSTREAM: OpenRA.Mods.Common/Activities/PickupUnit.cs + DeliverUnit.cs +
//          AutoCarryall.cs 的 FerryUnit 嵌套活动 @1f594e320
#include "mods/carryall_activities.hpp"

#include "mods/actor_exts.hpp"
#include "mods/air_activities.hpp"
#include "mods/aircraft.hpp"
#include "mods/body_orientation.hpp"
#include "mods/move_activities.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods::activities {

// ———— FerryUnit(AutoCarryall.cs L152-199)————

FerryUnit::FerryUnit(sim::Actor& self, sim::Actor* cargo)
    : p_cargo_{cargo} {
  ptr_carryable_ = cargo->Trait<AutoCarryable>();
  ptr_carryall_ = self.Trait<AutoCarryall>();
}

void FerryUnit::OnFirstRun(sim::Actor& self) {
  if (!ptr_carryall_->IsTraitDisabled() &&
      ptr_carryall_->CarryableActor() != nullptr &&
      !ptr_carryall_->CarryableActor()->IsDead())
    QueueChild(self.world().Arena().Create<PickupUnit>(
        self, p_cargo_, 0,
        std::optional<core::Color>{
            ptr_carryall_->Info().color_target_line}));
}

bool FerryUnit::Tick(sim::Actor& self) {
  if (IsCanceling() || ptr_carryall_->IsTraitDisabled() ||
      ptr_carryall_->CarryableActor() == nullptr ||
      ptr_carryall_->CarryableActor()->IsDead())
    return true;

  const WDist drop_range = ptr_carryall_->Info().dist_drop_range;
  const CPos destination =
      ptr_carryable_->Destination().value_or(self.Location());
  QueueChild(self.world().Arena().Create<DeliverUnit>(
      self, sim::Target::FromCell(self.world(), destination), drop_range,
      std::optional<core::Color>{ptr_carryall_->Info().color_target_line}));

  return true;
}

// ———— PickupUnit(L20-181)————

PickupUnit::PickupUnit(sim::Actor& self, sim::Actor* cargo, int delay,
                       std::optional<core::Color> target_line_color)
    : p_cargo_{cargo},
      int4_delay_{delay},
      opt_target_line_color_{target_line_color} {
  ptr_carryable_ = cargo->Trait<Carryable>();
  ptr_carryable_facing_ = cargo->Trait<sim::IFacing>();
  ptr_carryable_body_ = cargo->Trait<BodyOrientation>();

  ptr_carryall_ = self.Trait<Carryall>();

  b_child_has_priority_ = false;
}

void PickupUnit::OnFirstRun(sim::Actor& self) {
  if (p_cargo_->IsDead() || ptr_carryable_->IsTraitDisabled() ||
      ptr_carryall_->IsTraitDisabled() ||
      !AppearsFriendlyTo(*p_cargo_, self)) {
    b_reserve_failed_ = true;
    return;
  }

  if (ptr_carryall_->ReserveCarryable(self, p_cargo_)) {
    QueueChild(self.world().Arena().Create<Fly>(
        self, sim::Target::FromActor(p_cargo_)));
    QueueChild(self.world().Arena().Create<FlyIdle>(self, -1, false));
  } else {
    b_reserve_failed_ = true;
  }
}

bool PickupUnit::Tick(sim::Actor& self) {
  if (IsCanceling() || b_reserve_failed_)
    return true;

  if (p_cargo_->IsDead() || ptr_carryable_->IsTraitDisabled() ||
      ptr_carryall_->IsTraitDisabled() ||
      !AppearsFriendlyTo(*p_cargo_, self) ||
      p_cargo_ != ptr_carryall_->CarryableActor()) {
    Cancel(self, true);
    return false;
  }

  if (state_ == PickupState::Intercept &&
      (p_cargo_->CenterPosition() - self.CenterPosition())
              .HorizontalLengthSquared() <=
          dist_target_lock_range_.LengthSquared())
    state_ = PickupState::LockCarryable;
  {
    const auto* ca = ChildActivity();
    std::puts((std::string{"PU state="} +
               std::to_string(static_cast<int>(state_)) + " child=" +
               (ca != nullptr ? typeid(*ca).name() : "null") + " z=" +
               std::to_string(self.CenterPosition().Z))
                  .c_str());
  }

  if (state_ == PickupState::LockCarryable) {
    const LockResponse lock_response =
        ptr_carryable_->LockForPickup(*p_cargo_, &self);
    if (lock_response == LockResponse::Failed) {
      Cancel(self, true);
      return false;
    } else if (lock_response == LockResponse::Success) {
      if (auto* child = ChildActivity())
        child->Cancel(self);

      const WVec local_offset =
          ptr_carryall_->OffsetForCarryable(self, *p_cargo_)
              .Rotate(ptr_carryable_body_->QuantizeOrientation(
                  p_cargo_->Orientation()));
      QueueChild(self.world().Arena().Create<Land>(
          self, sim::Target::FromActor(p_cargo_),
          WVec{-local_offset.X, -local_offset.Y, -local_offset.Z},
          ptr_carryable_facing_->Facing()));

      if (int4_delay_ > 0)
        QueueChild(self.world().Arena().Create<Wait>(int4_delay_, false));

      QueueChild(
          self.world().Arena().Create<AttachUnit>(self, p_cargo_));
      QueueChild(self.world().Arena().Create<TakeOff>(self));

      state_ = PickupState::Pickup;
    }
  }

  return TickChild(self) && state_ == PickupState::Pickup;
}

void PickupUnit::Cancel(sim::Actor& self, bool keep_queue) {
  sim::Activity::Cancel(self, keep_queue);

  if (!IsInterruptible())
    return;

  if (ptr_carryall_->State() == CarryallState::Reserved)
    ptr_carryall_->UnreserveCarryable(self);

  if (dynamic_cast<TakeOff*>(ChildActivity()) != nullptr) {
    b_child_has_priority_ = true;
    return;
  }

  if (self.Trait<Aircraft>()->HasInfluence()) {
    b_child_has_priority_ = true;
    QueueChild(self.world().Arena().Create<TakeOff>(self));
  }
}

PickupUnit::AttachUnit::AttachUnit(sim::Actor& self, sim::Actor* cargo)
    : p_cargo_{cargo} {
  ptr_carryable_ = cargo->Trait<Carryable>();
  ptr_carryall_ = self.Trait<Carryall>();
}

void PickupUnit::AttachUnit::OnFirstRun(sim::Actor& self) {
  if (p_cargo_ == nullptr || p_cargo_->IsDead() ||
      ptr_carryable_->IsTraitDisabled() ||
      ptr_carryall_->IsTraitDisabled() ||
      ptr_carryall_->CarryableActor() != p_cargo_ ||
      !AppearsFriendlyTo(*p_cargo_, self))
    return;

  sim::Actor* self_ptr = &self;
  sim::Actor* cargo = p_cargo_;
  Carryable* carryable = ptr_carryable_;
  Carryall* carryall = ptr_carryall_;
  self.world().AddFrameEndTask(
      [self_ptr, cargo, carryable, carryall](sim::World& w) {
        (void)w;
        cargo->world().Remove(cargo);
        carryable->Attached(*cargo, self_ptr);
        carryall->AttachCarryable(*self_ptr, cargo);
      });
}

// ———— DeliverUnit(L20-112)————

DeliverUnit::DeliverUnit(sim::Actor& self, WDist deliver_range,
                         std::optional<core::Color> target_line_color)
    : DeliverUnit{self, sim::Target{}, deliver_range, target_line_color} {
  b_assign_target_on_first_run_ = true;
}

DeliverUnit::DeliverUnit(sim::Actor& self, const sim::Target& destination,
                         WDist deliver_range,
                         std::optional<core::Color> target_line_color)
    : dist_deliver_range_{deliver_range},
      opt_target_line_color_{target_line_color},
      target_destination_{destination} {
  ptr_carryall_ = self.Trait<Carryall>();
}

void DeliverUnit::OnFirstRun(sim::Actor& self) {
  if (ptr_carryall_->CarryableActor() == nullptr ||
      ptr_carryall_->State() != CarryallState::Carrying)
    return;

  if (b_assign_target_on_first_run_)
    target_destination_ =
        sim::Target::FromCell(self.world(), self.Location());

  QueueChild(self.world().Arena().Create<Land>(
      self, target_destination_, dist_deliver_range_));
  QueueChild(self.world().Arena().Create<Wait>(
      ptr_carryall_->Info().int4_before_unload_delay, false));
  QueueChild(self.world().Arena().Create<ReleaseUnit>(self));
  QueueChild(self.world().Arena().Create<TakeOff>(self));
}

DeliverUnit::ReleaseUnit::ReleaseUnit(sim::Actor& self) {
  ptr_facing_ = self.Trait<sim::IFacing>();
  ptr_carryall_ = self.Trait<Carryall>();
  ptr_body_ = self.Trait<BodyOrientation>();
}

void DeliverUnit::ReleaseUnit::OnFirstRun(sim::Actor& self) {
  if (ptr_carryall_->CarryableActor() == nullptr)
    return;

  Actor* carryable_actor = ptr_carryall_->CarryableActor();
  const WVec local_offset = ptr_carryall_->CarryableOffset().Rotate(
      ptr_body_->QuantizeOrientation(self.Orientation()));
  const WPos target_position = self.CenterPosition() +
                               ptr_body_->LocalToWorld(local_offset);
  const CPos target_location =
      self.world().Map().CellContaining(target_position);
  carryable_actor->Trait<sim::IPositionable>()->SetPosition(
      carryable_actor, target_location, SubCell::FullCell);
  carryable_actor->Trait<sim::IFacing>()->SetFacing(ptr_facing_->Facing());

  Carryall* carryall = ptr_carryall_;
  self.world().AddFrameEndTask(
      [carryall, &self](sim::World& w) {
        if (self.IsDead())
          return;

        Actor* cargo = carryall->CarryableActor();
        if (cargo == nullptr)
          return;

        auto* carryable = cargo->Trait<Carryable>();
        w.Add(cargo);
        carryall->DetachCarryable(self);
        carryable->UnReserve(*cargo);
        carryable->Detached(*cargo);
      });
}

}  // namespace ora::mods::activities
