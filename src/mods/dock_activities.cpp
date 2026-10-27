#include "mods/dock_activities.hpp"

#include "sim/world.hpp"

namespace ora::mods::activities {

MoveToDock::MoveToDock(sim::Actor& self, Actor* dock_host_actor,
                       sim::IDockHost* dock_host, bool force_enter,
                       bool ignore_occupancy,
                       std::optional<core::Color> dock_line_color)
    : p_dock_client_(self.Trait<DockClientManager>()),
      p_dock_host_actor_(dock_host_actor),
      p_dock_host_(dock_host),
      vec_notify_dock_client_moving_(
          self.TraitsImplementing<sim::INotifyDockClientMoving>()),
      opt_dock_line_color_(dock_line_color),
      move_cooldown_helper_(
          self.world(), dynamic_cast<Mobile*>(self.Trait<sim::IMove>())),
      b_force_enter_(force_enter),
      b_ignore_occupancy_(ignore_occupancy) {

  move_cooldown_helper_.SetRetryIfDestinationBlocked(true);
}

void MoveToDock::OnFirstRun(sim::Actor& self) {

  if (p_dock_client_->IsTraitDisabled())
    return;

  if (p_dock_host_actor_ != nullptr && p_dock_host_ == nullptr) {
    if (p_dock_host_actor_->IsDead() || !p_dock_host_actor_->IsInWorld()) {
      b_docking_cancelled_ = true;
      return;
    }

    const std::optional<sim::TraitPair<sim::IDockHost>> link =
        ClosestDock(p_dock_client_->AvailableDockHosts(
                        *p_dock_host_actor_, {}, b_force_enter_,
                        b_ignore_occupancy_),
                    self, *p_dock_client_);

    if (link.has_value())
      p_dock_host_ = link->trait;
    else
      b_docking_cancelled_ = true;
  }
}

bool MoveToDock::Tick(sim::Actor& self) {

  if (IsCanceling())
    return true;

  if (b_docking_cancelled_ || p_dock_client_->IsTraitDisabled()) {
    Cancel(self, true);
    return true;
  }

  if (p_dock_host_ == nullptr || !p_dock_host_->IsEnabledAndInWorld()) {
    const std::optional<sim::TraitPair<sim::IDockHost>> host =
        p_dock_client_->ClosestDock(nullptr);
    if (host.has_value()) {
      p_dock_host_ = host->trait;
      p_dock_host_actor_ = host->actor;
    } else {

      QueueChild(self.world().Arena().Create<Wait>(
          p_dock_client_->Info().int4_search_for_dock_delay));
      return false;
    }
  }

  const std::optional<bool> result = move_cooldown_helper_.Tick(false);
  if (result.has_value())
    return *result;

  if (p_dock_client_->ReserveHost(p_dock_host_actor_, p_dock_host_)) {
    if (p_dock_host_->QueueMoveActivity(this, *p_dock_host_actor_, self,
                                        p_dock_client_,
                                        move_cooldown_helper_)) {
      for (sim::INotifyDockClientMoving* ndcm :
           vec_notify_dock_client_moving_)
        ndcm->MovingToDock(self, *p_dock_host_actor_, p_dock_host_);

      return false;
    }

    p_dock_host_->QueueDockActivity(this, *p_dock_host_actor_, self,
                                    p_dock_client_);
    return true;
  } else {
    for (sim::INotifyDockClientMoving* ndcm :
         vec_notify_dock_client_moving_)
      ndcm->MovementCancelled(self);

    QueueChild(self.world().Arena().Create<Wait>(
        p_dock_client_->Info().int4_search_for_dock_delay));
    return false;
  }
}

void MoveToDock::Cancel(sim::Actor& self, bool keep_queue) {

  p_dock_client_->UnreserveHost();
  for (sim::INotifyDockClientMoving* ndcm : vec_notify_dock_client_moving_)
    ndcm->MovementCancelled(self);

  sim::Activity::Cancel(self, keep_queue);
}

GenericDockSequence::GenericDockSequence(
    sim::Actor& self, DockClientManager& client, Actor& host_actor,
    sim::IDockHost& host, int dock_wait, bool is_drag_required,
    WVec drag_offset, int drag_length)
    : b_is_drag_required_{is_drag_required},
      int4_drag_length_{drag_length} {

  docking_state_ = DockingState::Drag;

  p_dock_client_ = &client;

  vec_notify_dock_clients_ =
      self.TraitsImplementing<sim::INotifyDockClient>();

  p_dock_host_ = &host;
  p_dock_host_actor_ = &host_actor;

  vec_notify_dock_hosts_ =
      host_actor.TraitsImplementing<sim::INotifyDockHost>();

  pos_start_drag_ = self.CenterPosition();
  pos_end_drag_ = host_actor.CenterPosition() + drag_offset;

  QueueChild(self.world().Arena().Create<Wait>(dock_wait));
}

bool GenericDockSequence::Tick(sim::Actor& self) {

  switch (docking_state_) {
    case DockingState::Wait:
      return false;

    case DockingState::Drag:
      if (IsCanceling() || p_dock_host_actor_->IsDead() ||
          !p_dock_host_actor_->IsInWorld() ||
          !p_dock_client_->CanDockAt(*p_dock_host_actor_, p_dock_host_,
                                     false, true)) {
        p_dock_client_->UnreserveHost();
        return true;
      }

      docking_state_ = DockingState::Dock;
      if (b_is_drag_required_)
        QueueChild(NewActivity<Drag>(self, pos_start_drag_, pos_end_drag_,
                                     int4_drag_length_));

      return false;

    case DockingState::Dock:
      if (!IsCanceling() && !p_dock_host_actor_->IsDead() &&
          p_dock_host_actor_->IsInWorld() &&
          p_dock_client_->CanDockAt(*p_dock_host_actor_, p_dock_host_,
                                    false, true)) {
        b_dock_initiated_ = true;
        PlayDockAnimations(self);
        p_dock_host_->OnDockStarted(*p_dock_host_actor_, self,
                                    p_dock_client_);
        p_dock_client_->OnDockStarted(self, *p_dock_host_actor_,
                                      p_dock_host_);
        NotifyDocked(self);
      } else {
        docking_state_ = DockingState::Undock;
      }

      return false;

    case DockingState::Loop:
      if (IsCanceling() || p_dock_host_actor_->IsDead() ||
          !p_dock_host_actor_->IsInWorld() ||
          p_dock_client_->OnDockTick(self, *p_dock_host_actor_,
                                     p_dock_host_))
        docking_state_ = DockingState::Undock;

      return false;

    case DockingState::Undock:
      if (b_dock_initiated_)
        PlayUndockAnimations(self);
      else
        docking_state_ = DockingState::Complete;

      return false;

    case DockingState::Complete:
      p_dock_host_->OnDockCompleted(*p_dock_host_actor_, self,
                                    p_dock_client_);
      p_dock_client_->OnDockCompleted(self, *p_dock_host_actor_,
                                      p_dock_host_);
      NotifyUndocked(self);
      if (b_is_drag_required_)
        QueueChild(NewActivity<Drag>(self, pos_end_drag_, pos_start_drag_,
                                     int4_drag_length_));

      return true;
  }

  throw std::runtime_error("Invalid harvester dock state");
}

void GenericDockSequence::PlayDockAnimations(sim::Actor& self) {

  PlayDockCientAnimation(self, [this]() {
    docking_state_ = DockingState::Loop;
  });
}

void GenericDockSequence::PlayDockCientAnimation(
    sim::Actor& , std::function<void()> after) {

  after();
}

void GenericDockSequence::PlayUndockAnimations(sim::Actor& self) {

  PlayUndockClientAnimation(
      self, [this]() { docking_state_ = DockingState::Complete; });
}

void GenericDockSequence::PlayUndockClientAnimation(
    sim::Actor& , std::function<void()> after) {

  after();
}

void GenericDockSequence::NotifyDocked(sim::Actor& self) {

  for (sim::INotifyDockClient* nd : vec_notify_dock_clients_)
    nd->Docked(self, *p_dock_host_actor_);

  for (sim::INotifyDockHost* nd : vec_notify_dock_hosts_)
    nd->Docked(*p_dock_host_actor_, self);
}

void GenericDockSequence::NotifyUndocked(sim::Actor& self) {

  for (sim::INotifyDockClient* nd : vec_notify_dock_clients_)
    nd->Undocked(self, *p_dock_host_actor_);

  if (!p_dock_host_actor_->IsDead())
    for (sim::INotifyDockHost* nd : vec_notify_dock_hosts_)
      nd->Undocked(*p_dock_host_actor_, self);
}

}
