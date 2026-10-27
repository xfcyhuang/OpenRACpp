#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "mods/carryall.hpp"
#include "sim/activity.hpp"
#include "sim/actor.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::activities {

class FerryUnit final : public sim::Activity {
 public:
  FerryUnit(sim::Actor& self, sim::Actor* cargo);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  sim::Actor* p_cargo_;
  AutoCarryable* ptr_carryable_;
  AutoCarryall* ptr_carryall_;
};

class PickupUnit final : public sim::Activity {
 public:
  PickupUnit(sim::Actor& self, sim::Actor* cargo, int delay,
             std::optional<core::Color> target_line_color);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;
  void Cancel(sim::Actor& self, bool keep_queue = false) override;

 private:
  class AttachUnit final : public sim::Activity {
   public:
    AttachUnit(sim::Actor& self, sim::Actor* cargo);

    void OnFirstRun(sim::Actor& self) override;

   private:
    sim::Actor* p_cargo_;
    Carryable* ptr_carryable_;
    Carryall* ptr_carryall_;
  };

  sim::Actor* p_cargo_;
  Carryall* ptr_carryall_;
  Carryable* ptr_carryable_;
  sim::IFacing* ptr_carryable_facing_;
  BodyOrientation* ptr_carryable_body_;

  int int4_delay_;
  std::optional<core::Color> opt_target_line_color_;

  WDist dist_target_lock_range_ = WDist::FromCells(4);

  enum class PickupState { Intercept, LockCarryable, Pickup };
  PickupState state_ = PickupState::Intercept;
  bool b_reserve_failed_ = false;
};

class DeliverUnit final : public sim::Activity {
 public:
  DeliverUnit(sim::Actor& self, WDist deliver_range,
              std::optional<core::Color> target_line_color);
  DeliverUnit(sim::Actor& self, const sim::Target& destination,
              WDist deliver_range,
              std::optional<core::Color> target_line_color);

  void OnFirstRun(sim::Actor& self) override;

 private:
  class ReleaseUnit final : public sim::Activity {
   public:
    explicit ReleaseUnit(sim::Actor& self);

    void OnFirstRun(sim::Actor& self) override;

   private:
    Carryall* ptr_carryall_;
    BodyOrientation* ptr_body_;
    sim::IFacing* ptr_facing_;
  };

  Carryall* ptr_carryall_;
  bool b_assign_target_on_first_run_ = false;
  WDist dist_deliver_range_;
  std::optional<core::Color> opt_target_line_color_;

  sim::Target target_destination_;
};

}
