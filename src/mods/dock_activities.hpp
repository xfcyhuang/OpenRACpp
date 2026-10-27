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

class GenericDockSequence : public sim::Activity {
 public:

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

  virtual void PlayDockAnimations(sim::Actor& self);

  virtual void PlayDockCientAnimation(
      sim::Actor& self, std::function<void()> after);

  virtual void PlayUndockAnimations(sim::Actor& self);

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

  void NotifyDocked(sim::Actor& self);
  void NotifyUndocked(sim::Actor& self);

  std::vector<sim::INotifyDockClient*> vec_notify_dock_clients_;
  std::vector<sim::INotifyDockHost*> vec_notify_dock_hosts_;

  bool b_dock_initiated_ = false;
};

}
