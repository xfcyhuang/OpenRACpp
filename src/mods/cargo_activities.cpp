// UPSTREAM: OpenRA.Mods.Common/Activities/RideTransport.cs +
//          UnloadCargo.cs + Parachute.cs @e180e3225
#include "mods/cargo_activities.hpp"

#include "game/actor_info.hpp"
#include "map/map.hpp"
#include "mods/actor_exts.hpp"
#include "mods/air_activities.hpp"
#include "mods/aircraft.hpp"
#include "mods/cargo.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "mods/passenger.hpp"
#include "mods/production.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods::activities {

namespace {

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec_info,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec_info, str_name);
}

int ParachutableFallRate(sim::Actor& self) {
  for (const meta::RecordObject* rec : self.Info()->TraitsInConstructOrder())
    if (rec->record_desc().str_name == std::string_view{"ParachutableInfo"})
      if (const auto v = FieldInt(*rec, "FallRate"))
        return static_cast<int>(*v);
  return 13;
}

}  // namespace

RideTransport::RideTransport(sim::Actor& self, const sim::Target& target,
                             std::optional<core::Color> target_line_color)
    : Enter(self, target, target_line_color),
      passenger_{*self.Trait<Passenger>()} {}

bool RideTransport::TryStartEnter(sim::Actor& self,
                                  sim::Actor& target_actor) {
  p_enter_actor_ = &target_actor;
  ptr_enter_cargo_ = target_actor.TraitOrDefault<Cargo>();
  ptr_enter_aircraft_ = target_actor.TraitOrDefault<Aircraft>();

  if (ptr_enter_cargo_ == nullptr || ptr_enter_cargo_->IsTraitDisabled() ||
      !passenger_.Reserve(self, *ptr_enter_cargo_)) {
    Cancel(self, true);
    return false;
  }

  if (ptr_enter_aircraft_ != nullptr && !ptr_enter_aircraft_->AtLandAltitude())
    return false;

  return true;
}

void RideTransport::TickInner(sim::Actor& self, const sim::Target&,
                              bool) {
  if (ptr_enter_cargo_ != nullptr && ptr_enter_cargo_->IsTraitDisabled())
    Cancel(self, true);
}

void RideTransport::OnEnterComplete(sim::Actor& self,
                                    sim::Actor& target_actor) {
  Cargo* enter_cargo = ptr_enter_cargo_;
  Actor* enter_actor = p_enter_actor_;
  Actor* self_ptr = &self;
  Actor* target_ptr = &target_actor;
  self.world().AddFrameEndTask(
      [self_ptr, enter_cargo, enter_actor, target_ptr](sim::World& w) {
        if (self_ptr->IsDead())
          return;

        if (target_ptr != enter_actor)
          return;

        if (!enter_cargo->CanLoad(*self_ptr))
          return;

        for (auto* inl :
             target_ptr->TraitsImplementing<sim::INotifyLoadCargo>())
          inl->Loading(*self_ptr);

        enter_cargo->Load(*enter_actor, *self_ptr);
        w.Remove(self_ptr);
      });
}

void RideTransport::OnLastRun(sim::Actor& self) {
  passenger_.Unreserve(self);
}

void RideTransport::Cancel(sim::Actor& self, bool keep_queue) {
  passenger_.Unreserve(self);

  Enter::Cancel(self, keep_queue);
}

UnloadCargo::UnloadCargo(sim::Actor& self, WDist unload_range,
                         bool unload_all)
    : UnloadCargo(self, sim::Target::Invalid(), unload_range, unload_all) {
  b_assign_target_on_first_run_ = true;
}

UnloadCargo::UnloadCargo(sim::Actor& self, const sim::Target& destination,
                         WDist unload_range, bool unload_all)
    : self_{self},
      cargo_{*self.Trait<Cargo>()},
      vec_notifiers_{self.TraitsImplementing<sim::INotifyUnloadCargo>()},
      b_unload_all_{unload_all},
      ptr_aircraft_{self.TraitOrDefault<Aircraft>()},
      ptr_mobile_{self.TraitOrDefault<Mobile>()},
      dist_unload_range_{unload_range},
      destination_{destination} {}

std::optional<std::pair<CPos, SubCell>> UnloadCargo::ChooseExitSubCell(
    sim::Actor& passenger) {
  auto* pos = passenger.Trait<sim::IPositionable>();

  std::vector<CPos> vec_cells = cargo_.CurrentAdjacentCells();
  ShuffleSpan(vec_cells, self_.world().SharedRandom());
  for (const CPos c : vec_cells) {
    const SubCell sub_cell = pos->GetAvailableSubCell(
        c, SubCell::Any, nullptr, sim::BlockedByActor::All);
    if (sub_cell != SubCell::Invalid)
      return std::make_pair(c, sub_cell);
  }
  return std::nullopt;
}

std::vector<CPos> UnloadCargo::BlockedExitCells(sim::Actor& passenger) {
  auto* pos = passenger.Trait<sim::IPositionable>();

  std::vector<CPos> vec_out;
  for (const CPos c : cargo_.CurrentAdjacentCells())
    if (pos->CanEnterCell(c, nullptr, sim::BlockedByActor::All) !=
        pos->CanEnterCell(c, nullptr, sim::BlockedByActor::None))
      vec_out.push_back(c);
  return vec_out;
}

void UnloadCargo::OnFirstRun(sim::Actor& self) {
  if (b_assign_target_on_first_run_)
    destination_ = sim::Target::FromCell(self.world(), self.Location());

  if (ptr_aircraft_ != nullptr) {
    QueueChild(self.world().Arena().Create<Land>(
        self, destination_, dist_unload_range_));
    b_take_off_after_unload_ = !ptr_aircraft_->AtLandAltitude();
  } else if (ptr_mobile_ != nullptr) {
    const CPos cell = self.world().Map().Clamp(
        self.world().Map().CellContaining(destination_.CenterPosition()));
    QueueChild(self.world().Arena().Create<Move>(
        self, cell, dist_unload_range_, nullptr, false,
        std::optional<core::Color>{}));
  }

  QueueChild(self.world().Arena().Create<Wait>(cargo_.Info().int4_before_unload_delay));
}

bool UnloadCargo::Tick(sim::Actor& self) {
  if (IsCanceling() || cargo_.IsEmpty())
    return true;

  if (cargo_.CanUnload()) {
    if (int4_delay_between_unloads_ > 0) {
      int4_delay_between_unloads_--;
      return false;
    }

    int4_delay_between_unloads_ = cargo_.Info().int4_between_unload_delay;
    for (auto* inu : vec_notifiers_)
      inu->Unloading(self);

    Actor* actor = cargo_.Peek();
    const WPos spawn = self.CenterPosition();

    const std::optional<std::pair<CPos, SubCell>> exit_sub_cell =
        ChooseExitSubCell(*actor);
    if (!exit_sub_cell.has_value()) {
      NotifyBlocker(self, BlockedExitCells(*actor));
      QueueChild(self.world().Arena().Create<Wait>(10));
      return false;
    }

    cargo_.Unload(self);
    self.world().AddFrameEndTask(
        [actor, exit_sub_cell, spawn](sim::World& w) {
          if (actor->Disposed())
            return;

          auto* move = actor->Trait<sim::IMove>();
          auto* pos = actor->Trait<sim::IPositionable>();
          auto* passenger = actor->Trait<Passenger>();
          (void)move;

          pos->SetPosition(actor, exit_sub_cell->first,
                           exit_sub_cell->second);
          pos->SetCenterPosition(actor, spawn);

          passenger->OnBeforeAddedToWorld(*actor);
          w.Add(actor);
        });
  }

  if (!b_unload_all_ || !cargo_.CanUnload()) {
    if (cargo_.Info().int4_after_unload_delay > 0)
      QueueChild(self.world().Arena().Create<Wait>(
          cargo_.Info().int4_after_unload_delay, false));

    if (b_take_off_after_unload_)
      QueueChild(self.world().Arena().Create<TakeOff>(self));

    return true;
  }

  return false;
}

Parachute::Parachute(sim::Actor& self)
    : ptr_pos_{self.Trait<sim::IPositionable>()},
      wvec_fall_vector_{0, 0, ParachutableFallRate(self)} {
  b_is_interruptible_ = false;
}

void Parachute::OnFirstRun(sim::Actor& self) {
  int4_ground_level_ = self.world().Map().CenterOfCell(self.Location()).Z;
  for (auto* np : self.TraitsImplementing<sim::INotifyParachute>())
    np->OnParachute(self);
}

bool Parachute::Tick(sim::Actor& self) {
  const WPos next_position = self.CenterPosition() - wvec_fall_vector_;
  if (next_position.Z < int4_ground_level_)
    return true;

  ptr_pos_->SetCenterPosition(&self, next_position);

  return false;
}

void Parachute::OnLastRun(sim::Actor& self) {
  const WPos center_position = self.CenterPosition();
  ptr_pos_->SetPosition(
      &self, center_position +
                 WVec{0, 0, int4_ground_level_ - center_position.Z});

  for (auto* np : self.TraitsImplementing<sim::INotifyParachute>())
    np->OnLanded(self);
}

}  // namespace ora::mods::activities
