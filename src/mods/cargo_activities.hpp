#pragma once
import std;

#include "core/wdist.hpp"
#include "mods/capture.hpp"
#include "sim/activity.hpp"
#include "sim/target.hpp"

namespace ora::mods {

class Cargo;
class Aircraft;
class Mobile;
class Passenger;

namespace activities {

class RideTransport final : public Enter {
 public:
  RideTransport(sim::Actor& self, const sim::Target& target,
                std::optional<core::Color> target_line_color);

 protected:
  bool TryStartEnter(sim::Actor& self, sim::Actor& target_actor) override;
  void TickInner(sim::Actor& self, const sim::Target& target,
                 bool b_target_is_dead_or_hidden_actor) override;
  void OnEnterComplete(sim::Actor& self, sim::Actor& target_actor) override;
  void OnLastRun(sim::Actor& self) override;
  void Cancel(sim::Actor& self, bool keep_queue = false) override;

 private:
  Passenger& passenger_;

  sim::Actor* p_enter_actor_ = nullptr;
  Cargo* ptr_enter_cargo_ = nullptr;
  Aircraft* ptr_enter_aircraft_ = nullptr;
};

class UnloadCargo final : public sim::Activity {
 public:
  UnloadCargo(sim::Actor& self, WDist unload_range,
              bool unload_all = true);
  UnloadCargo(sim::Actor& self, const sim::Target& destination,
              WDist unload_range, bool unload_all = true);

  std::optional<std::pair<CPos, SubCell>> ChooseExitSubCell(
      sim::Actor& passenger);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  std::vector<CPos> BlockedExitCells(sim::Actor& passenger);

  sim::Actor& self_;
  Cargo& cargo_;
  std::vector<sim::INotifyUnloadCargo*> vec_notifiers_;
  bool b_unload_all_;
  Aircraft* ptr_aircraft_ = nullptr;
  Mobile* ptr_mobile_ = nullptr;
  bool b_assign_target_on_first_run_ = false;
  WDist dist_unload_range_;
  int int4_delay_between_unloads_ = 0;

  sim::Target destination_{};
  bool b_take_off_after_unload_ = false;
};

class Parachute final : public sim::Activity {
 public:
  explicit Parachute(sim::Actor& self);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;

 private:
  sim::IPositionable* ptr_pos_ = nullptr;
  WVec wvec_fall_vector_{0, 0, 0};
  int int4_ground_level_ = 0;
};

}

}
