#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wangle.hpp"
#include "core/wvec.hpp"
#include "mods/dock_client.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::Player;
using sim::TraitBase;

struct DockHostInfoData {
  core::BitSet<sim::DockType> bitset_type;
  int int4_max_queue_length = 3;
  int int4_dock_wait = 10;
  WAngle angle_dock_angle{0};
  WVec vec_dock_offset{0, 0, 0};
  bool b_is_drag_required = false;
  WVec vec_drag_offset{0, 0, 0};
  int int4_drag_length = 0;
  sim::ConditionalTraitData conditional;

  static DockHostInfoData Parse(const meta::RecordObject& rec_info);
};

class DockHost final : public TraitBase,
                       public sim::ConditionalTraitCore<DockHost>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::IDockHost,
                       public sim::ITick,
                       public sim::INotifySold,
                       public sim::INotifyCapture,
                       public sim::INotifyOwnerChanged,
                       public sim::ISync,
                       public sim::INotifyKilled,
                       public sim::INotifyActorDisposing {
 public:
  DockHost(ActorInitializer& init, const DockHostInfoData& info);

  ORA_TRAIT_INTERFACES(
      DockHost, OpenRA_Mods_Common_Traits_DockHost, sim::IObservesVariables,
      sim::INotifyCreated, sim::IDockHost, sim::ITick, sim::INotifySold,
      sim::INotifyCapture, sim::INotifyOwnerChanged, sim::ISync,
      sim::INotifyKilled, sim::INotifyActorDisposing)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<DockHost>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<DockHost>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  core::BitSet<sim::DockType> GetDockType() override {
    return info_.bitset_type;
  }
  bool IsEnabledAndInWorld() override {
    return !b_prevent_dock && !IsTraitDisabled() && !p_self_->IsDead() &&
           p_self_->IsInWorld();
  }
  int ReservationCount() override {
    return static_cast<int>(vec_reserved_dock_clients_.size());
  }
  bool CanBeReserved() override {
    return ReservationCount() < info_.int4_max_queue_length;
  }
  WPos DockPosition() override {
    return p_self_->CenterPosition() + info_.vec_dock_offset;
  }

  bool IsDockingPossible(Actor& client_actor, sim::IDockClient* client,
                         bool ignore_reservations = false) override;

  bool Reserve(Actor& self, DockClientManager* client) override;

  void UnreserveAll() override;

  void Unreserve(DockClientManager* client) override;

  void OnDockStarted(Actor& self, Actor& client_actor,
                     DockClientManager* client) override;
  void OnDockCompleted(Actor& self, Actor& client_actor,
                       DockClientManager* client) override;

  void Tick(Actor& self) override;

  bool QueueMoveActivity(
      sim::Activity* move_to_dock_activity, Actor& self, Actor& client_actor,
      DockClientManager* client,
      activities::MoveCooldownHelper& move_cooldown_helper) override;

  void QueueDockActivity(sim::Activity* move_to_dock_activity, Actor& self,
                         Actor& client_actor,
                         DockClientManager* client) override;

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) { UnreserveAll(); }
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  void Selling(Actor& self) override;
  void Sold(Actor& self) override;
  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void Disposing(Actor& self) override;

  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;
  void OnCapture(Actor& self, Actor& captor, Player& old_owner,
                 Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types)
      override;

  const DockHostInfoData& Info() const { return info_; }

 private:
  friend class sim::ConditionalTraitCore<DockHost>;
  friend int DockHostSyncHashImpl(const sim::ISync* s);

  const DockHostInfoData info_;
  Actor* p_self_ = nullptr;

  bool b_prevent_dock = false;
  Actor* p_docked_client_actor_ = nullptr;
  DockClientManager* p_docked_client_ = nullptr;

  std::vector<DockClientManager*> vec_reserved_dock_clients_;
};

}
