#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "gfx/animation.hpp"
#include "gfx/renderable.hpp"
#include "sim/effects.hpp"
#include "sim/screen_map.hpp"
#include "sim/shroud.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/weapons.hpp"

namespace ora::game {
class WeaponInfo;
}

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {
class Player;
class World;
}

namespace ora::mods {

class RangeCircleAnnotationRenderable final : public gfx::IRenderable,
                                               public gfx::IFinalizedRenderable {
 public:
  RangeCircleAnnotationRenderable(WPos wpos_pos, WDist dist_radius,
                                  std::int32_t int4_z_offset,
                                  core::Color color_c, float fp4_width,
                                  core::Color color_border,
                                  float fp4_border_width);

  WPos Pos() const override { return wpos_pos_; }
  std::int32_t ZOffset() const override { return int4_z_offset_; }
  bool IsDecoration() const override { return true; }
  gfx::IRenderable* WithZOffset(std::int32_t int4_new_offset) override;
  gfx::IRenderable* OffsetBy(const WVec& vec_offset) override;
  gfx::IRenderable* AsDecoration() override { return this; }
  gfx::IFinalizedRenderable* PrepareRender(gfx::WorldRenderer&) override {
    return this;
  }

  void Render(gfx::WorldRenderer& wr) override;
  void RenderDebugGeometry(gfx::WorldRenderer&) override {}
  Rectangle ScreenBounds(gfx::WorldRenderer&) override { return Rectangle{}; }

  static void DrawRangeCircle(gfx::WorldRenderer& wr,
                              const WPos& wpos_center, WDist dist_radius,
                              float fp4_width, core::Color color_c,
                              float fp4_border_width,
                              core::Color color_border);

 private:
  WPos wpos_pos_;
  WDist dist_radius_;
  std::int32_t int4_z_offset_;
  core::Color color_c_;
  float fp4_width_;
  core::Color color_border_;
  float fp4_border_width_;
};

class RevealShroudEffect final : public sim::IEffect {
 public:
  RevealShroudEffect(WPos wpos_pos, WDist dist_radius,
                     sim::ShroudSourceType kind_type,
                     sim::Player& player_for,
                     sim::PlayerRelationship kind_stances, int delay = 0,
                     int duration = 50);

  void Tick(sim::World& world) override;

 private:
  void AddCellsToPlayerShroud(sim::Player& p,
                              const std::vector<PPos>& vec_uv);
  void RemoveCellsFromPlayerShroud(sim::Player& p);
  std::vector<PPos> ProjectedCells(sim::World& world);

  WPos wpos_pos_;
  sim::Player* ptr_player_;
  WDist dist_reveal_radius_;
  sim::PlayerRelationship kind_valid_stances_;
  sim::ShroudSourceType kind_source_type_;
  int int4_duration_;
  int int4_ticks_ = 0;
};

class Beacon final : public sim::IEffect,
                     public sim::ISpatiallyPartitionable {
 public:
  Beacon(sim::Player& owner, WPos wpos_pos, int duration,
         std::string str_beacon_palette, bool b_is_player_palette,
         std::string str_beacon_collection,
         std::string str_beacon_sequence, std::string str_arrow_sprite,
         std::string str_circle_sprite, int delay = 0);

  Beacon(sim::Player& owner, WPos wpos_pos, bool b_is_player_palette,
         std::string str_palette, std::string str_poster_collection,
         std::string str_poster_type, std::string str_poster_palette,
         std::string str_beacon_sequence, std::string str_arrow_sequence,
         std::string str_circle_sequence, std::string str_clock_sequence,
         std::function<float()> fn_clock_fraction, int delay = 0,
         int duration = -1);

  void Tick(sim::World& world) override;

  void RenderAboveShroud(gfx::WorldRenderer& wr,
                         std::vector<gfx::RenderItem>& vec_out);

  int ArrowHeightForTest() const { return int4_arrow_height_; }
  int TickCountForTest() const { return int4_tick_; }

 private:
  sim::Player* ptr_owner_;
  WPos wpos_pos_;
  bool b_is_player_palette_ = false;
  std::string str_beacon_palette_;
  std::string str_poster_palette_;
  std::string str_beacon_collection_;
  std::optional<gfx::Animation> opt_beacon_;
  std::optional<gfx::Animation> opt_arrow_;
  std::optional<gfx::Animation> opt_circles_;
  std::optional<gfx::Animation> opt_clock_;
  std::optional<gfx::Animation> opt_poster_;
  int int4_duration_;
  int int4_delay_;
  static constexpr int kMaxArrowHeight = 512;
  int int4_arrow_height_ = kMaxArrowHeight;
  int int4_arrow_speed_ = 50;
  int int4_tick_ = 0;
};

class NukeLaunch final : public sim::IProjectile,
                         public sim::ISpatiallyPartitionable {
 public:
  NukeLaunch(sim::Player& fired_by, std::string str_image,
             const game::WeaponInfo& weapon, std::string str_weapon_palette,
             std::string str_up_sequence, std::string str_down_sequence,
             WPos wpos_launch_pos, WPos wpos_target_pos,
             WDist dist_detonation_altitude, bool b_remove_on_detonation,
             WDist dist_velocity, int int4_launch_delay,
             int int4_impact_delay, bool b_skip_ascent,
             std::string str_trail_image,
             std::vector<std::string> vec_trail_sequences,
             std::string str_trail_palette, bool b_trail_use_player_palette,
             int int4_trail_delay, int int4_trail_interval);

  void Tick(sim::World& world) override;

  void Render(gfx::WorldRenderer& wr, std::vector<gfx::RenderItem>& vec_out);

  float FractionComplete() const;

  std::shared_ptr<float> FractionHandle() const { return ptr_fraction_; }
  WPos PositionForTest() const { return wpos_pos_; }
  bool DetonatedForTest() const { return b_detonated_; }
  bool InWorldForTest() const { return b_in_world_; }
  const game::WeaponInfo* WeaponForTest() const { return weapon_; }

 private:
  void Explode(sim::World& world, bool b_remove_projectile);

  sim::Player* ptr_fired_by_;
  std::optional<gfx::Animation> opt_anim_;
  const game::WeaponInfo* weapon_;
  std::string str_weapon_palette_;
  std::string str_up_sequence_;
  std::string str_down_sequence_;

  WPos wpos_ascend_source_;
  WPos wpos_ascend_target_;
  WPos wpos_descend_source_;
  WPos wpos_descend_target_;
  WDist dist_detonation_altitude_;
  bool b_remove_on_detonation_ = false;
  int int4_impact_delay_;
  int int4_turn_;
  std::string str_trail_image_;
  std::vector<std::string> vec_trail_sequences_;
  std::string str_trail_palette_;
  int int4_trail_interval_;
  int int4_trail_delay_;

  WPos wpos_pos_;
  int int4_ticks_ = 0;
  int int4_trail_ticks_ = 0;
  int int4_launch_delay_;
  bool b_is_launched_ = false;
  bool b_detonated_ = false;
  bool b_in_world_ = true;
  std::shared_ptr<float> ptr_fraction_ = std::make_shared<float>(0.0f);
};

}  // namespace ora::mods
