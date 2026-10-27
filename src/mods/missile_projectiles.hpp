#pragma once
import std;

#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "mods/util.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/weapons.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::gfx {
class Animation;
}

namespace ora::mods {

using sim::Actor;

struct MissileInfoData {
  std::string str_image;
  std::vector<std::string> vec_sequences{"idle"};
  std::string str_palette{"effect"};
  bool b_is_player_palette = false;
  bool b_shadow = false;
  core::Color shadow_color{
      core::Color::FromArgbRaw(0x8C000000u)};
  WAngle min_launch_angle{-64};
  WAngle max_launch_angle{128};
  WDist min_launch_speed{-1};
  WDist max_launch_speed{-1};
  WDist speed{384};
  WDist acceleration{5};
  int int4_arm = 0;
  bool b_blockable = true;
  bool b_terrain_height_aware = false;
  WDist dist_width{1};
  WDist dist_inaccuracy{0};
  InaccuracyType inaccuracy_type = InaccuracyType::Absolute;
  WDist lock_on_inaccuracy{-1};
  int int4_lock_on_probability = 100;
  WAngle horizontal_rate_of_turn{20};
  WAngle vertical_rate_of_turn{24};
  int int4_gravity = 10;
  WDist range_limit{0};
  bool b_explode_when_empty = true;
  WDist airburst_altitude{0};
  WDist cruise_altitude{512};
  int int4_homing_activation_delay = 0;
  std::string str_trail_image;
  std::vector<std::string> vec_trail_sequences{"idle"};
  std::string str_trail_palette{"effect"};
  bool b_trail_use_player_palette = false;
  int int4_trail_interval = 2;
  bool b_trail_when_deactivated = false;
  int int4_contrail_length = 0;
  int int4_contrail_delay = 1;
  int int4_contrail_z_offset = 2047;
  WDist dist_contrail_start_width{64};
  std::optional<WDist> opt_contrail_end_width;
  core::Color contrail_start_color;
  bool b_contrail_start_color_use_player_color = false;
  int int4_contrail_start_color_alpha = 255;
  std::optional<core::Color> opt_contrail_end_color;
  bool b_contrail_end_color_use_player_color = false;
  int int4_contrail_end_color_alpha = 0;
  bool b_jammable = true;
  int int4_jammed_diversion_range = 20;
  std::string str_bound_to_terrain_type;
  bool b_allow_snapping = false;
  WDist close_enough{298};

  static MissileInfoData Parse(const meta::RecordObject& rec_info);
};

class MissileInfo final : public sim::IProjectileInfo {
 public:
  explicit MissileInfo(MissileInfoData data) : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const MissileInfoData& Info() const { return info_; }

 private:
  MissileInfoData info_;
};

struct GravityBombInfoData {
  std::string str_image;
  std::vector<std::string> vec_sequences{"idle"};
  std::string str_open_sequence;
  std::string str_palette{"effect"};
  bool b_is_player_palette = false;
  bool b_shadow = false;
  core::Color shadow_color{
      core::Color::FromArgbRaw(0x8C000000u)};
  WVec velocity{0, 0, 0};
  WVec acceleration{0, 0, -15};

  static GravityBombInfoData Parse(const meta::RecordObject& rec_info);
};

class GravityBombInfo final : public sim::IProjectileInfo {
 public:
  explicit GravityBombInfo(GravityBombInfoData data)
      : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const GravityBombInfoData& Info() const { return info_; }

 private:
  GravityBombInfoData info_;
};

struct TeslaZapInfoData {
  std::string str_image{"litning"};
  std::string str_bright_sequence{"bright"};
  std::string str_dim_sequence{"dim"};
  std::string str_palette{"effect"};
  int int4_bright_zaps = 1;
  int int4_dim_zaps = 2;
  int int4_duration = 2;
  int int4_damage_duration = 1;
  bool b_track_target = true;
  int int4_z_offset = 0;

  static TeslaZapInfoData Parse(const meta::RecordObject& rec_info);
};

class TeslaZapInfo final : public sim::IProjectileInfo {
 public:
  explicit TeslaZapInfo(TeslaZapInfoData data) : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const TeslaZapInfoData& Info() const { return info_; }

 private:
  TeslaZapInfoData info_;
};

class Missile final : public sim::IProjectile, public sim::ISync {
 public:
  Missile(const MissileInfoData& info, sim::ProjectileArgs& args);

  void Tick(sim::World& world) override;

  static int SyncHashOf(const sim::ISync* s);

 private:

  static int LoopRadiusOf(int speed, int rot);

  void DetermineLaunchSpeedAndAngleForIncline(int pred_clf_dist,
                                              int diff_clf_msl_hgt,
                                              int rel_tar_hor_dist,
                                              int& speed, int& v_facing);

  void DetermineLaunchSpeedAndAngle(sim::World& world, int& speed,
                                    int& v_facing);

  static bool WillClimbWithinDistance(int v_facing, int loop_radius,
                                      int pred_clf_dist,
                                      int diff_clf_msl_hgt);

  static bool IsNearInclineTop(int v_facing, int loop_radius,
                               int pred_clf_dist);

  static bool WillClimbAroundInclineTop(int v_facing, int loop_radius,
                                        int pred_clf_dist,
                                        int diff_clf_msl_hgt);

  template <class Pred>
  static int BisectionSearch(int lower_bound, int upper_bound, Pred test);

  void ChangeSpeed(int sign = 1);

  WVec FreefallTick();

  void InclineLookahead(sim::World& world, int dist_check,
                        int& pred_clf_hgt, int& pred_clf_dist,
                        int& last_ht_chg, int& last_ht);

  int IncreaseAltitude(int pred_clf_dist, int diff_clf_msl_hgt,
                       int rel_tar_hor_dist, int v_facing);

  int HomingInnerTick(int pred_clf_dist, int diff_clf_msl_hgt,
                      int rel_tar_hor_dist, int last_ht_chg, int last_ht,
                      int rel_tar_hgt, int v_facing, bool target_passed_by);

  WVec HomingTick(sim::World& world, const WVec& tar_dist_vec,
                  int rel_tar_hor_dist);

  void Explode(sim::World& world);

  enum class States { Freefall, Homing, Hitting };

  const MissileInfoData& info_;
  sim::ProjectileArgs args_;
  gfx::Animation* animation_ = nullptr;

  WVec gravity_{};
  int min_launch_speed_ = 0;
  int max_launch_speed_ = 0;
  int max_speed_ = 0;
  WAngle min_launch_angle_{};
  WAngle max_launch_angle_{};

  int int4_ticks_ = 0;
  int int4_ticks_to_next_smoke_ = 0;
  std::string str_trail_palette_;

  States state_ = States::Freefall;
  bool b_target_passed_by_ = false;
  bool b_lock_on_ = false;
  bool b_allow_pass_by_ = false;

  WPos target_position_{};
  WVec offset_{};

  WVec tar_vel_{};
  WVec pred_vel_{};

  WPos pos_{};
  WVec velocity_{};
  int int4_speed_ = 0;
  int int4_loop_radius_ = 0;
  WDist distance_covered_{0};
  WDist range_limit_{0};

  WAngle render_facing_{};
  int int4_h_facing_ = 0;
  int int4_v_facing_ = 0;
};

class GravityBomb final : public sim::IProjectile, public sim::ISync {
 public:
  GravityBomb(const GravityBombInfoData& info, sim::ProjectileArgs& args);

  void Tick(sim::World& world) override;

  static int SyncHashOf(const sim::ISync* s);

 private:
  const GravityBombInfoData& info_;
  sim::ProjectileArgs args_;
  gfx::Animation* animation_ = nullptr;
  WVec acceleration_{};

  WPos pos_, last_pos_;

  WVec velocity_{};
};

class TeslaZap final : public sim::IProjectile, public sim::ISync {
 public:
  TeslaZap(const TeslaZapInfoData& info, sim::ProjectileArgs& args);

  void Tick(sim::World& world) override;

  static int SyncHashOf(const sim::ISync* s);

 private:
  sim::ProjectileArgs args_;
  const TeslaZapInfoData& info_;
  int int4_ticks_until_remove_ = 0;
  int int4_damage_duration_ = 0;

  WPos target_{};
};

}
