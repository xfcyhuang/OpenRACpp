#pragma once
import std;

#include "core/color.hpp"
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

struct BulletInfoData {
  std::vector<WDist> vec_speed{WDist{17}};
  WDist dist_inaccuracy{0};
  InaccuracyType inaccuracy_type = InaccuracyType::Maximum;
  std::string str_image;
  std::vector<std::string> vec_sequences{"idle"};
  std::string str_palette{"effect"};
  bool b_is_player_palette = false;
  bool b_shadow = false;
  core::Color shadow_color{
      core::Color::FromArgbRaw(0x8C000000u)};
  std::string str_trail_image;
  std::vector<std::string> vec_trail_sequences{"idle"};
  int int4_trail_interval = 2;
  int int4_trail_delay = 1;
  std::string str_trail_palette{"effect"};
  bool b_trail_use_player_palette = false;
  bool b_blockable = true;
  WDist dist_width{1};
  std::vector<WAngle> vec_launch_angle{WAngle{0}};
  int int4_bounce_count = 0;
  int int4_bounce_range_modifier = 60;
  std::string str_bounce_sound;
  std::vector<std::string> vec_invalid_bounce_terrain;
  sim::PlayerRelationship valid_bounce_blocker_relationships =
      sim::PlayerRelationship::Enemy | sim::PlayerRelationship::Neutral;
  WDist dist_airburst_altitude{0};

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

  static BulletInfoData Parse(const meta::RecordObject& rec_info);
};

class BulletInfo final : public sim::IProjectileInfo {
 public:
  explicit BulletInfo(BulletInfoData data) : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const BulletInfoData& Info() const { return info_; }

 private:
  BulletInfoData info_;
};

struct InstantHitInfoData {
  WDist dist_inaccuracy{0};
  InaccuracyType inaccuracy_type = InaccuracyType::Maximum;
  bool b_blockable = false;
  WDist dist_width{1};
  WDist dist_blocker_scan_radius{-1};

  static InstantHitInfoData Parse(const meta::RecordObject& rec_info);
};

class InstantHitInfo final : public sim::IProjectileInfo {
 public:
  explicit InstantHitInfo(InstantHitInfoData data)
      : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const InstantHitInfoData& Info() const { return info_; }

 private:
  InstantHitInfoData info_;
};

void SetBulletAnimationFactory(
    std::function<gfx::Animation*(sim::World&, const std::string&,
                                  const std::string&,
                                  std::function<WAngle()>)> fn_factory);

gfx::Animation* MakeBulletAnimation(
    sim::World& world, const std::string& str_image,
    const std::string& str_sequence, std::function<WAngle()> fn_facing);

void SetProjectileSoundPlayer(
    std::function<void(const std::string&, sim::World&, const WPos&)>
        fn_play);

void PlayProjectileSoundIfWired(const std::string& str_name,
                                sim::World& world, const WPos& wpos_pos);

void InstallCommonSyncEffectHasher(sim::World& world);

}
