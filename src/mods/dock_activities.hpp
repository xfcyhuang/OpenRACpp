// UPSTREAM: OpenRA.Mods.Common/Activities/MoveToDock.cs @b6fc03f L19-150
//          全文 + GenericDockSequence.cs L22-216 全文(逐语义重写;
//          WithDockingOverlay/IDockClientBody 的动画面 = 未移植空集:
//          PlayThen 回调即置 Loop 态 —— 上游无该 trait 时的等价路径)
//          The whole of MoveToDock.cs L19-150 + the whole of
//          GenericDockSequence.cs L22-216 (verbatim-semantics rewrites;
//          the WithDockingOverlay/IDockClientBody animation faces are the
//          unported empty set: the PlayThen callback sets the Loop state
//          at once — upstream's equivalent path without those traits).
//
// 机制对照 / Mechanism mapping:
//  - 嵌套 DockingState 枚举照抄;PlayDockAnimations 的 Action after 闭包
//    链 → std::function(无动画 trait 时直接调用 —— 上游 else 分支)
//    The nested DockingState enum copied; PlayDockAnimations' Action-after
//    closure chain → std::function (invoked directly without the
//    animation traits — upstream's else branches).
//  - INotifyDockClientMoving/INotifyDockClient/INotifyDockHost 的通知面
//    = TraitsImplementing 直查(空集 = 上游无实现者时的等价域)
//    The INotifyDockClientMoving/INotifyDockClient/INotifyDockHost
//    notification faces query TraitsImplementing directly (the empty set
//    = upstream's equivalent domain without implementors).
#pragma once
import std;

#include "core/color.hpp"
#include "mods/dock_client.hpp"
#include "mods/dock_host.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "sim/activity.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::activities {

/// MoveToDock.cs L20-150
class MoveToDock final : public sim::Activity {
 public:
  MoveToDock(sim::Actor& self, Actor* dock_host_actor,
             sim::IDockHost* dock_host, bool force_enter,
             bool ignore_occupancy,
             std::optional<core::Color> dock_line_color = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;
  void Cancel(sim::Actor& self, bool keep_queue = false) override;

 private:
  DockClientManager* p_dock_client_;
  Actor* p_dock_host_actor_;
  sim::IDockHost* p_dock_host_;
  std::vector<sim::INotifyDockClientMoving*> vec_notify_dock_client_moving_;
  std::optional<core::Color> opt_dock_line_color_;
  activities::MoveCooldownHelper move_cooldown_helper_;
  bool b_force_enter_;
  bool b_ignore_occupancy_;

  bool b_docking_cancelled_ = false;
};

/// GenericDockSequence.cs L23-215
class GenericDockSequence : public sim::Activity {
 public:
  /// L25:DockingState | L25: DockingState.
  enum class DockingState {
    Wait,
    Drag,
    Dock,
    Loop,
    Undock,
    Complete,
  };

  GenericDockSequence(sim::Actor& self, DockClientManager& client,
                      Actor& host_actor, sim::IDockHost& host, int dock_wait,
                      bool is_drag_required, WVec drag_offset,
                      int drag_length);

  bool Tick(sim::Actor& self) override;

  /// L127-143:PlayDockAnimations(virtual)
  /// L127-143: PlayDockAnimations (virtual).
  virtual void PlayDockAnimations(sim::Actor& self);

  /// L145-155:PlayDockCientAnimation(virtual;上游拼写)
  /// L145-155: PlayDockCientAnimation (virtual; upstream's spelling).
  virtual void PlayDockCientAnimation(
      sim::Actor& self, std::function<void()> after);

  /// L157-173:PlayUndockAnimations(virtual)
  /// L157-173: PlayUndockAnimations (virtual).
  virtual void PlayUndockAnimations(sim::Actor& self);

  /// L175-184:PlayUndockClientAnimation(virtual)
  /// L175-184: PlayUndockClientAnimation (virtual).
  virtual void PlayUndockClientAnimation(sim::Actor& self,
                                         std::function<void()> after);

 protected:
  Actor* p_dock_host_actor_;
  sim::IDockHost* p_dock_host_;
  DockClientManager* p_dock_client_;
  bool b_is_drag_required_;
  int int4_drag_length_;
  WPos pos_start_drag_;
  WPos pos_end_drag_;

  DockingState docking_state_;

 private:
  /// L187-204:NotifyDocked/NotifyUndocked
  /// L187-204: NotifyDocked/NotifyUndocked.
  void NotifyDocked(sim::Actor& self);
  void NotifyUndocked(sim::Actor& self);

  std::vector<sim::INotifyDockClient*> vec_notify_dock_clients_;
  std::vector<sim::INotifyDockHost*> vec_notify_dock_hosts_;

  bool b_dock_initiated_ = false;
};

}  // namespace ora::mods::activities
