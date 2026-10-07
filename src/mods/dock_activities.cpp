// UPSTREAM: OpenRA.Mods.Common/Activities/MoveToDock.cs 实现部分
//          + GenericDockSequence.cs | The implementation half.
#include "mods/dock_activities.hpp"

#include "sim/world.hpp"

namespace ora::mods::activities {

// ———— MoveToDock(L20-150)————

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
  // L34-48
  move_cooldown_helper_.SetRetryIfDestinationBlocked(true);
}

void MoveToDock::OnFirstRun(sim::Actor& self) {
  // L47-69
  if (p_dock_client_->IsTraitDisabled())
    return;

  // We were ordered to dock to an actor but host was unspecified.
  // (上游注释)
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
  // L71-125
  if (IsCanceling())
    return true;

  if (b_docking_cancelled_ || p_dock_client_->IsTraitDisabled()) {
    Cancel(self, true);
    return true;
  }

  // Find the nearest DockHost if not explicitly ordered to a specific
  // dock.(上游注释)
  if (p_dock_host_ == nullptr || !p_dock_host_->IsEnabledAndInWorld()) {
    const std::optional<sim::TraitPair<sim::IDockHost>> host =
        p_dock_client_->ClosestDock(nullptr);
    if (host.has_value()) {
      p_dock_host_ = host->trait;
      p_dock_host_actor_ = host->actor;
    } else {
      // No docks exist; check again after delay defined in dockClient.
      // (上游注释)
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

    // The dock explicitly chosen by the user is currently occupied. Wait
    // and check again.(上游注释)
    QueueChild(self.world().Arena().Create<Wait>(
        p_dock_client_->Info().int4_search_for_dock_delay));
    return false;
  }
}

void MoveToDock::Cancel(sim::Actor& self, bool keep_queue) {
  // L127-134
  p_dock_client_->UnreserveHost();
  for (sim::INotifyDockClientMoving* ndcm : vec_notify_dock_client_moving_)
    ndcm->MovementCancelled(self);

  sim::Activity::Cancel(self, keep_queue);
}

// ———— GenericDockSequence(L23-215)————

GenericDockSequence::GenericDockSequence(
    sim::Actor& self, DockClientManager& client, Actor& host_actor,
    sim::IDockHost& host, int dock_wait, bool is_drag_required,
    WVec drag_offset, int drag_length)
    : b_is_drag_required_{is_drag_required},
      int4_drag_length_{drag_length} {
  // L44-64
  docking_state_ = DockingState::Drag;

  p_dock_client_ = &client;
  // IDockClientBody trait 未移植:null = 上游 else 分支(动画直通)
  // The IDockClientBody trait is unported: null = upstream's else
  // branch (the animation passes straight through).
  vec_notify_dock_clients_ =
      self.TraitsImplementing<sim::INotifyDockClient>();

  p_dock_host_ = &host;
  p_dock_host_actor_ = &host_actor;
  // WithDockingOverlay trait 未移植:DockHostSpriteOverlay 恒 null(同上)
  // The WithDockingOverlay trait is unported:
  // DockHostSpriteOverlay stays null (ditto).
  vec_notify_dock_hosts_ =
      host_actor.TraitsImplementing<sim::INotifyDockHost>();

  pos_start_drag_ = self.CenterPosition();
  pos_end_drag_ = host_actor.CenterPosition() + drag_offset;

  QueueChild(self.world().Arena().Create<Wait>(dock_wait));
}

bool GenericDockSequence::Tick(sim::Actor& self) {
  // L66-125
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
  // L127-143(WithDockingOverlay 恒 null → after 的 else 直通)
  // L127-143 (WithDockingOverlay stays null → the else pass-through of
  // after).
  PlayDockCientAnimation(self, [this]() {
    docking_state_ = DockingState::Loop;
  });
}

void GenericDockSequence::PlayDockCientAnimation(
    sim::Actor& /*self*/, std::function<void()> after) {
  // L145-155(IDockClientBody 恒 null → 直接调用)
  // L145-155 (IDockClientBody stays null → invoked directly).
  after();
}

void GenericDockSequence::PlayUndockAnimations(sim::Actor& self) {
  // L157-173(WithDockingOverlay 恒 null → else 分支)
  // L157-173 (WithDockingOverlay stays null → the else branch).
  PlayUndockClientAnimation(
      self, [this]() { docking_state_ = DockingState::Complete; });
}

void GenericDockSequence::PlayUndockClientAnimation(
    sim::Actor& /*self*/, std::function<void()> after) {
  // L175-184(同上)
  // L175-184 (ditto).
  after();
}

void GenericDockSequence::NotifyDocked(sim::Actor& self) {
  // L187-194
  for (sim::INotifyDockClient* nd : vec_notify_dock_clients_)
    nd->Docked(self, *p_dock_host_actor_);

  for (sim::INotifyDockHost* nd : vec_notify_dock_hosts_)
    nd->Docked(*p_dock_host_actor_, self);
}

void GenericDockSequence::NotifyUndocked(sim::Actor& self) {
  // L196-204
  for (sim::INotifyDockClient* nd : vec_notify_dock_clients_)
    nd->Undocked(self, *p_dock_host_actor_);

  if (!p_dock_host_actor_->IsDead())
    for (sim::INotifyDockHost* nd : vec_notify_dock_hosts_)
      nd->Undocked(*p_dock_host_actor_, self);
}

}  // namespace ora::mods::activities
