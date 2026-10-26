// UPSTREAM: OpenRA.Mods.Common/Activities/Air/Fly.cs @1f594e320 +
//          TakeOff.cs + Land.cs + FlyOffMap.cs + FlyIdle.cs +
//          FlyForward.cs + FlyFollow.cs + ReturnToBase.cs + FallToEarth.cs
//          + ../Resupply.cs
#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "mods/air_support.hpp"
#include "mods/move_activities.hpp"
#include "sim/activity.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Aircraft;
class Rearmable;
class Repairable;
class RepairableNear;
class RepairsUnits;
class AmmoPool;
class PlayerResources;
struct FallsToEarthInfoData;

namespace activities {

class CallFunc final : public sim::Activity {
 public:
  explicit CallFunc(std::function<void()> fn_action)
      : fn_action_{std::move(fn_action)} {}

  void OnFirstRun(sim::Actor&) override {
    if (fn_action_)
      fn_action_();
  }
  bool Tick(sim::Actor&) override { return true; }

 private:
  std::function<void()> fn_action_;
};

class Fly final : public sim::Activity {
 public:
  Fly(sim::Actor& self, const sim::Target& t, WDist near_enough,
      std::optional<WPos> initial_target_position = std::nullopt,
      std::optional<core::Color> target_line_color = std::nullopt);
  Fly(sim::Actor& self, const sim::Target& t,
      std::optional<WPos> initial_target_position = std::nullopt,
      std::optional<core::Color> target_line_color = std::nullopt);
  Fly(sim::Actor& self, const sim::Target& t, WDist min_range,
      WDist max_range, std::optional<WPos> initial_target_position = std::nullopt,
      std::optional<core::Color> target_line_color = std::nullopt);

  bool Tick(sim::Actor& self) override;

  static void FlyTick(sim::Actor& self, Aircraft& aircraft,
                      WAngle desired_facing, WDist desired_altitude,
                      const WVec& move_override, bool b_idle_turn = false);
  static void FlyTick(sim::Actor& self, Aircraft& aircraft,
                      WAngle desired_facing, WDist desired_altitude,
                      bool b_idle_turn = false);
  static bool VerticalTakeOffOrLandTick(sim::Actor& self, Aircraft& aircraft,
                                        WAngle desired_facing,
                                        WDist desired_altitude,
                                        bool b_idle_turn = false);
  static int CalculateTurnRadius(int speed, WAngle turn_speed);

 private:
  Aircraft* aircraft_ = nullptr;
  WDist dist_max_range_{0};
  WDist dist_min_range_{0};
  std::optional<core::Color> opt_target_line_color_;
  WDist dist_near_enough_{0};
  sim::Target target_;
  sim::Target last_visible_target_;
  bool b_use_last_visible_target_ = false;
  std::array<WPos, 5> arr_previous_positions_{};
  std::size_t n_previous_positions_count_ = 0;
  std::size_t n_previous_positions_head_ = 0;
};

class TakeOff final : public sim::Activity {
 public:
  explicit TakeOff(sim::Actor& self);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  Aircraft* aircraft_ = nullptr;
};

class Land final : public sim::Activity {
 public:
  Land(sim::Actor& self, std::optional<WAngle> facing,
       std::optional<core::Color> target_line_color);
  Land(sim::Actor& self, const sim::Target& target,
       std::optional<WAngle> facing = std::nullopt,
       std::optional<core::Color> target_line_color = std::nullopt);
  Land(sim::Actor& self, const sim::Target& target, WDist land_range,
       std::optional<WAngle> facing = std::nullopt,
       std::optional<core::Color> target_line_color = std::nullopt);
  Land(sim::Actor& self, const sim::Target& target, const WVec& offset,
       std::optional<WAngle> facing = std::nullopt,
       std::optional<core::Color> target_line_color = std::nullopt);
  Land(sim::Actor& self, const sim::Target& target, WDist land_range,
       const WVec& offset, std::optional<WAngle> facing = std::nullopt,
       std::vector<CPos> clear_cells = {},
       std::optional<core::Color> target_line_color = std::nullopt);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  Aircraft* aircraft_ = nullptr;
  WVec wvec_offset_{0, 0, 0};
  std::optional<WAngle> opt_desired_facing_;
  bool b_assign_target_on_first_run_ = false;
  std::vector<CPos> vec_clear_cells_;
  WDist dist_land_range_;
  std::optional<core::Color> opt_target_line_color_;
  sim::Target target_;
  WPos wpos_target_position_;
  CPos cell_landing_;
  bool b_landing_initiated_ = false;
  bool b_finished_approach_ = false;
};

class FlyOffMap final : public sim::Activity {
 public:
  explicit FlyOffMap(sim::Actor& self, int ending_delay = 25);
  FlyOffMap(sim::Actor& self, const sim::Target& target,
            int ending_delay = 25);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  Aircraft* aircraft_ = nullptr;
  sim::Target target_;
  bool b_has_target_ = false;
  int int4_ending_delay_;
};

class FlyIdle final : public sim::Activity {
 public:
  explicit FlyIdle(sim::Actor& self, int ticks = -1, bool idle_turn = true);

  bool Tick(sim::Actor& self) override;

 private:
  Aircraft* aircraft_ = nullptr;
  std::vector<sim::INotifyIdle*> vec_tick_idles_;
  bool b_tick_idles_loaded_ = false;
  bool b_idle_turn_ = true;
  int int4_remaining_ticks_;
  bool b_is_idle_turner_ = false;
};

class FlyForward final : public sim::Activity {
 public:
  explicit FlyForward(sim::Actor& self, int ticks = -1);
  FlyForward(sim::Actor& self, WDist distance);

  bool Tick(sim::Actor& self) override;

 private:
  Aircraft* aircraft_ = nullptr;
  WDist dist_cruise_altitude_;
  int int4_fly_ticks_;
  int int4_remaining_distance_ = 0;
  int int4_ticks_ = 0;
};

class FlyFollow final : public sim::Activity {
 public:
  FlyFollow(sim::Actor& self, const sim::Target& target, WDist min_range,
            WDist max_range, std::optional<WPos> initial_target_position,
            std::optional<core::Color> target_line_color = std::nullopt);

  bool Tick(sim::Actor& self) override;

 private:
  Aircraft* aircraft_ = nullptr;
  WDist dist_min_range_{0};
  WDist dist_max_range_{0};
  std::optional<core::Color> opt_target_line_color_;
  sim::Target target_;
  sim::Target last_visible_target_;
  bool b_use_last_visible_target_ = false;
  bool b_was_moving_within_range_ = false;
};

class ReturnToBase final : public sim::Activity {
 public:
  ReturnToBase(sim::Actor& self, Actor* dest = nullptr,
               bool always_land = false);

  static Actor* ChooseResupplier(sim::Actor& self, bool unreserved_only);

  bool Tick(sim::Actor& self) override;

 private:
  bool ShouldLandAtBuilding(sim::Actor& self, Actor& dest);

  Aircraft* aircraft_ = nullptr;
  bool has_repairable_info_ = false;
  std::vector<std::string> vec_repair_actors_info_;
  Rearmable* rearmable_ = nullptr;
  bool b_always_land_ = false;
  Actor* p_dest_ = nullptr;
  std::optional<WAngle> opt_facing_;
};

class FallToEarth final : public sim::Activity {
 public:
  FallToEarth(sim::Actor& self,
              const ora::mods::FallsToEarthInfoData& info);

  bool Tick(sim::Actor& self) override;

 private:
  Aircraft* aircraft_ = nullptr;
  ora::mods::FallsToEarthInfoData info_;
  int int4_acceleration_ = 0;
  int int4_spin_ = 0;
};

class Resupply final : public sim::Activity {
 public:
  Resupply(sim::Actor& self, Actor& host, WDist close_enough,
           bool stay_on_resupplier = false);

  bool Tick(sim::Actor& self) override;
  void Cancel(sim::Actor& self, bool keep_queue) override;

 private:
  void OnResupplyEnding(sim::Actor& self, bool b_host_invalid = false);
  void RepairTick(sim::Actor& self);

  sim::IHealth* health_ = nullptr;
  std::vector<RepairsUnits*> vec_repairs_units_;
  sim::Target host_;
  WDist dist_close_enough_;
  Repairable* repairable_ = nullptr;
  RepairableNear* repairable_near_ = nullptr;
  Rearmable* rearmable_ = nullptr;
  std::vector<sim::INotifyResupply*> vec_notify_resupplies_;
  std::vector<sim::INotifyDockHost*> vec_notify_dock_hosts_;
  std::vector<sim::INotifyDockClient*> vec_notify_dock_clients_;
  sim::IMove* move_ = nullptr;
  Aircraft* aircraft_ = nullptr;
  const struct ora::mods::MobileInfoData* move_info_ = nullptr;
  bool b_stay_on_resupplier_ = false;
  bool b_was_repaired_ = false;
  ora::mods::PlayerResources* player_resources_ = nullptr;
  int int4_unit_cost_ = 0;
  MoveCooldownHelper move_cooldown_helper_;
  int int4_remaining_ticks_ = 0;
  bool b_played_ = false;
  bool b_actual_resupply_started_ = false;
  sim::ResupplyType kind_active_resupply_types_ = sim::ResupplyType::None;
};

}  // namespace activities

}  // namespace ora::mods
