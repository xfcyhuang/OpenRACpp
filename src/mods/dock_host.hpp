// UPSTREAM: OpenRA.Mods.Common/Traits/DockHost.cs @b6fc03f L16-185 全文
//          (逐语义重写;DockType 位标签在 sim/trait_interfaces.hpp)
//          The whole of DockHost.cs L16-185 (a verbatim-semantics rewrite;
//          the DockType bit tag lives in sim/trait_interfaces.hpp).
//
// 机制对照 / Mechanism mapping:
//  - ReservedDockClients 的 List → vector;UnreserveAll = 逐个 Unreserve
//    的上游循环
//    ReservedDockClients' List → a vector; UnreserveAll keeps upstream's
//    per-item Unreserve loop.
//  - [VerifySync] {preventDock, dockedClientActor}:Actor 键 → HashActor
//    (ActorID);哈希注册于 dock_host.cpp
//    The [VerifySync] {preventDock, dockedClientActor}: the Actor key →
//    HashActor (ActorID); the hash registers in dock_host.cpp.
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

/// DockHostInfo(L21-49)的解析面
/// The parsed face of DockHostInfo (L21-49).
struct DockHostInfoData {
  core::BitSet<sim::DockType> bitset_type;  // L25(Type;无缺省 = 空集)
  int int4_max_queue_length = 3;            // L28
  int int4_dock_wait = 10;                  // L31
  WAngle angle_dock_angle{0};               // L34
  WVec vec_dock_offset{0, 0, 0};            // L37
  bool b_is_drag_required = false;          // L40
  WVec vec_drag_offset{0, 0, 0};            // L43
  int int4_drag_length = 0;                 // L46
  sim::ConditionalTraitData conditional;

  static DockHostInfoData Parse(const meta::RecordObject& rec_info);
};

/// DockHost(L50-184)
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

  // ———— IDockHost ————
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

  /// L77-80:IsDockingPossible | L77-80: IsDockingPossible.
  bool IsDockingPossible(Actor& client_actor, sim::IDockClient* client,
                         bool ignore_reservations = false) override;

  /// L82-92:Reserve | L82-92: Reserve.
  bool Reserve(Actor& self, DockClientManager* client) override;

  /// L94-98:UnreserveAll | L94-98: UnreserveAll.
  void UnreserveAll() override;

  /// L100-104:Unreserve | L100-104: Unreserve.
  void Unreserve(DockClientManager* client) override;

  /// L106-110/112-116:OnDock 族 | L106-110/112-116: the OnDock family.
  void OnDockStarted(Actor& self, Actor& client_actor,
                     DockClientManager* client) override;
  void OnDockCompleted(Actor& self, Actor& client_actor,
                       DockClientManager* client) override;

  /// L118-128:ITick.Tick | L118-128: ITick.Tick.
  void Tick(Actor& self) override;

  /// L130-146:QueueMoveActivity(定位/朝向对齐)
  /// L130-146: QueueMoveActivity (the position/facing alignment).
  bool QueueMoveActivity(
      sim::Activity* move_to_dock_activity, Actor& self, Actor& client_actor,
      DockClientManager* client,
      activities::MoveCooldownHelper& move_cooldown_helper) override;

  /// L148-159:QueueDockActivity | L148-159: QueueDockActivity.
  void QueueDockActivity(sim::Activity* move_to_dock_activity, Actor& self,
                         Actor& client_actor,
                         DockClientManager* client) override;

  /// L161:TraitDisabled → UnreserveAll | L161: TraitDisabled →
  /// UnreserveAll.
  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) { UnreserveAll(); }
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  void Selling(Actor& self) override;   // L177
  void Sold(Actor& self) override;      // L179
  void Killed(Actor& self, const sim::AttackInfo& e) override;  // L181
  void Disposing(Actor& self) override;  // L183

  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;  // L163
  void OnCapture(Actor& self, Actor& captor, Player& old_owner,
                 Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types)
      override;  // L165-175

  const DockHostInfoData& Info() const { return info_; }

 private:
  friend class sim::ConditionalTraitCore<DockHost>;
  friend int DockHostSyncHashImpl(const sim::ISync* s);

  const DockHostInfoData info_;
  Actor* p_self_ = nullptr;  // L53

  // ———— [VerifySync] L65-68 ————
  bool b_prevent_dock = false;
  Actor* p_docked_client_actor_ = nullptr;
  DockClientManager* p_docked_client_ = nullptr;

  std::vector<DockClientManager*> vec_reserved_dock_clients_;  // L60
};

}  // namespace ora::mods
