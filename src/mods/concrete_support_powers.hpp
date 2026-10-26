// UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/NukePower.cs @1f594e320 L23-243 +
//          SpawnActorPower.cs L22-163 + ProduceActorPower.cs L19-113 +
//          GrantExternalConditionPower.cs L23-178
#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "game/input.hpp"
#include "meta/generic_record.hpp"
#include "mods/support_power_effects.hpp"
#include "mods/support_powers.hpp"
#include "sim/activity.hpp"
#include "sim/order_generator.hpp"

namespace ora::game {
class ActorInfo;
class WeaponInfo;
}

namespace ora::sim {
class World;
}

namespace ora::mods {

class BodyOrientation;

using SupportPowerSoundPlayer = std::function<void(
    const std::string&, sim::World&, const WPos&)>;

void SetSupportPowerSoundPlayer(SupportPowerSoundPlayer fn_play);
void PlaySupportPowerSoundIfWired(const std::string& str_name,
                                  sim::World& world, const WPos& wpos_pos);

class RemoveSelf final : public sim::Activity {
 public:
  bool Tick(sim::Actor& self) override;
};

struct NukePowerInfoData : SupportPowerInfoData {
  std::string str_missile_weapon;
  int int4_missile_delay = 0;
  std::string str_missile_image;
  std::string str_missile_up{"up"};
  std::string str_missile_down{"down"};
  WVec wvec_spawn_offset{};
  WDist dist_detonation_altitude{0};
  bool b_remove_missile_on_detonation = true;
  std::string str_missile_palette{"effect"};
  bool b_is_player_palette = false;
  std::string str_trail_image;
  std::vector<std::string> vec_trail_sequences;
  int int4_trail_interval = 1;
  int int4_trail_delay = 1;
  std::string str_trail_palette{"effect"};
  bool b_trail_use_player_palette = false;
  int int4_flight_delay = 400;
  WDist dist_flight_velocity{512};
  bool b_skip_ascent = false;
  int int4_beacon_remove_advance = 25;
  WDist dist_camera_range{0};
  bool b_reveal_generated_shroud = true;
  sim::PlayerRelationship kind_camera_relationships =
      sim::PlayerRelationship::Ally;
  int int4_camera_spawn_advance = 25;
  int int4_camera_remove_delay = 25;
  core::Color color_circle =
      core::Color::FromArgb(128, 255, 0, 0);
  float fp4_circle_width = 1.0f;
  core::Color color_circle_border =
      core::Color::FromArgb(64, 255, 0, 0);
  float fp4_circle_border_width = 3.0f;
  bool b_has_circle_ranges = false;
  std::vector<WDist> vec_circle_ranges;
  const game::WeaponInfo* ptr_weapon_info = nullptr;

  static NukePowerInfoData Parse(const meta::RecordObject& rec_info,
                                 const sim::World& world);
};

class NukePower final : public SupportPower {
 public:
  NukePower(const ActorInitializer& init, NukePowerInfoData info);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_NukePower;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts = ora::sim::MakeTraitUpcasts<
      NukePower, SupportPower, sim::IObservesVariables,
      sim::INotifyCreated>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  const NukePowerInfoData& NukeInfo() const { return nuke_info_; }

  void Created(Actor& self) override;
  void SelectTarget(Actor& self, const std::string& str_order,
                    SupportPowerManager& manager) override;
  void Activate(Actor& self, const net::Order& order,
                SupportPowerManager& manager) override;
  void ActivateAt(Actor& self, const WPos& wpos_target);

 private:
  NukePowerInfoData nuke_info_;
  BodyOrientation* ptr_body_ = nullptr;
};

class SelectNukePowerTarget final : public SelectGenericPowerTarget {
 public:
  SelectNukePowerTarget(std::string str_order, SupportPowerManager& manager,
                        const NukePowerInfoData& info,
                        GameSettingsFace game_settings = {});

  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer& wr,
                                                   sim::World& world) override;

 private:
  const NukePowerInfoData& nuke_info_;
  std::vector<RangeCircleAnnotationRenderable> vec_circles_;
  std::vector<gfx::IRenderable*> vec_circle_ptrs_;
};

struct SpawnActorPowerInfoData : SupportPowerInfoData {
  std::string str_actor;
  int int4_life_time = 250;
  bool b_has_terrain = false;
  std::vector<std::string> vec_terrain;
  bool b_allow_under_shroud = true;
  std::string str_deploy_sound;
  std::string str_effect_image;
  std::string str_effect_sequence;
  std::string str_effect_palette;
  bool b_effect_palette_is_player_palette = false;

  static SpawnActorPowerInfoData Parse(const meta::RecordObject& rec_info);
};

class SpawnActorPower final : public SupportPower {
 public:
  SpawnActorPower(const ActorInitializer& init, SpawnActorPowerInfoData info);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_SpawnActorPower;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts = ora::sim::MakeTraitUpcasts<
      SpawnActorPower, SupportPower, sim::IObservesVariables,
      sim::INotifyCreated>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  const SpawnActorPowerInfoData& SpawnInfo() const { return spawn_info_; }

  void SelectTarget(Actor& self, const std::string& str_order,
                    SupportPowerManager& manager) override;
  void Activate(Actor& self, const net::Order& order,
                SupportPowerManager& manager) override;
  bool Validate(sim::World& world, CPos cell) const;

 private:
  SpawnActorPowerInfoData spawn_info_;
};

class SelectSpawnActorPowerTarget final : public sim::IOrderGenerator {
 public:
  SelectSpawnActorPowerTarget(std::string str_order,
                              SupportPowerManager& manager,
                              SpawnActorPower& power,
                              GameSettingsFace game_settings = {});

  MouseButton ActionButton() const override;
  MouseButton CancelButton() const { return ActionButton(); }
  std::vector<std::unique_ptr<net::Order>> Order(
      sim::World& world, CPos cell,
      int2 world_pixel,
      const MouseInput& mi_input) override;
  void Tick(sim::World& world) override;
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer&,
                                        sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer&,
                                                  sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::string GetCursor(sim::World& world, CPos cell, int2,
                        const MouseInput&) override;
  void Deactivate() override {}
  void SelectionChanged(sim::World&, std::span<sim::Actor* const>) override {}

 private:
  SupportPowerManager& manager_;
  SpawnActorPower& power_;
  std::string str_order_key_;
  GameSettingsFace game_settings_;
};

struct ProduceActorPowerInfoData : SupportPowerInfoData {
  std::vector<std::string> vec_actors;
  std::string str_type;
  std::string str_ready_audio;
  std::string str_ready_text_notification;
  std::string str_blocked_audio;
  std::string str_blocked_text_notification;

  static ProduceActorPowerInfoData Parse(const meta::RecordObject& rec_info);
};

class ProduceActorPower final : public SupportPower {
 public:
  ProduceActorPower(const ActorInitializer& init,
                    ProduceActorPowerInfoData info);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ProduceActorPower;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts = ora::sim::MakeTraitUpcasts<
      ProduceActorPower, SupportPower, sim::IObservesVariables,
      sim::INotifyCreated>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  const ProduceActorPowerInfoData& ProduceInfo() const {
    return produce_info_;
  }
  const std::string& Faction() const { return str_faction_; }

  void SelectTarget(Actor& self, const std::string& str_order,
                    SupportPowerManager& manager) override;
  void Activate(Actor& self, const net::Order& order,
                SupportPowerManager& manager) override;

 private:
  ProduceActorPowerInfoData produce_info_;
  std::string str_faction_;
};

struct GrantExternalConditionPowerInfoData : SupportPowerInfoData {
  std::string str_condition;
  int int4_duration = 0;
  CVec cvec_dimensions{0, 0};
  std::string str_footprint;
  std::string str_on_fire_sound;
  sim::PlayerRelationship kind_valid_relationships =
      sim::PlayerRelationship::Ally;
  std::string str_sequence{"active"};
  std::string str_footprint_image{"overlay"};
  std::string str_footprint_sequence{"target-select"};

  static GrantExternalConditionPowerInfoData Parse(
      const meta::RecordObject& rec_info);
};

class GrantExternalConditionPower final : public SupportPower {
 public:
  GrantExternalConditionPower(const ActorInitializer& init,
                              GrantExternalConditionPowerInfoData info);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_GrantExternalConditionPower;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts = ora::sim::MakeTraitUpcasts<
      GrantExternalConditionPower, SupportPower, sim::IObservesVariables,
      sim::INotifyCreated>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  const GrantExternalConditionPowerInfoData& ConditionInfo() const {
    return condition_info_;
  }
  const std::vector<char>& Footprint() const { return vec_footprint_; }

  void SelectTarget(Actor& self, const std::string& str_order,
                    SupportPowerManager& manager) override;
  void Activate(Actor& self, const net::Order& order,
                SupportPowerManager& manager) override;

  std::vector<sim::Actor*> UnitsInRange(CPos cpos_xy);
  bool HasUnitsInRange(CPos cpos_xy) { return !UnitsInRange(cpos_xy).empty(); }

 private:
  GrantExternalConditionPowerInfoData condition_info_;
  std::vector<char> vec_footprint_;
};

class SelectConditionTarget final : public sim::IOrderGenerator {
 public:
  SelectConditionTarget(sim::World& world, std::string str_order,
                        SupportPowerManager& manager,
                        GrantExternalConditionPower& power,
                        GameSettingsFace game_settings = {});

  MouseButton ActionButton() const override;
  MouseButton CancelButton() const { return ActionButton(); }
  std::vector<std::unique_ptr<net::Order>> Order(
      sim::World& world, CPos cell,
      int2 world_pixel,
      const MouseInput& mi_input) override;
  void Tick(sim::World& world) override;
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer& wr,
                                        sim::World& world) override;
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer&,
                                                  sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer& wr,
                                                   sim::World& world) override;
  std::string GetCursor(sim::World& world, CPos cell, int2,
                        const MouseInput&) override;
  void Deactivate() override {}
  void SelectionChanged(sim::World&, std::span<sim::Actor* const>) override {}

 private:
  SupportPowerManager& manager_;
  GrantExternalConditionPower& power_;
  std::string str_order_key_;
  GameSettingsFace game_settings_;
  std::vector<char> vec_footprint_;
  CVec cvec_dimensions_;
  gfx::Sprite sprite_tile_;
  float fp4_alpha_ = 1.0f;
};

}  // namespace ora::mods
