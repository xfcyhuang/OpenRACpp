#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/vector_n.hpp"
#include "core/wpos.hpp"
#include "map/cell_layer.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/shroud.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::map {
class Map;
}

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct RadarPingsInfoData {
  int int4_from_radius = 200;
  int int4_to_radius = 15;
  int int4_shrink_speed = 4;
  float fp4_rotation_speed = 0.12f;

  static RadarPingsInfoData Parse(const meta::RecordObject& rec_info);
};

class RadarPing {
 public:
  std::function<bool()> fn_is_visible;
  WPos wpos_position;
  core::Color color_color;
  int int4_duration;
  int int4_from_radius;
  int int4_to_radius;
  int int4_shrink_speed;
  float fp4_rotation_speed;

  RadarPing(std::function<bool()> fn_is_visible_in, WPos position,
            core::Color color, int duration, int from_radius, int to_radius,
            int shrink_speed, float rotation_speed)
      : fn_is_visible(std::move(fn_is_visible_in)),
        wpos_position(position),
        color_color(color),
        int4_duration(duration),
        int4_from_radius(from_radius),
        int4_to_radius(to_radius),
        int4_shrink_speed(shrink_speed),
        fp4_rotation_speed(rotation_speed),
        int4_radius(from_radius) {}

  bool Tick() {
    if (++int4_tick == int4_duration)
      return false;
    int4_radius = std::max(int4_radius - int4_shrink_speed, int4_to_radius);
    fp4_angle -= fp4_rotation_speed;
    return true;
  }

  std::array<core::Vector2, 3> Points(int2 int2_center) const;

 private:
  int int4_radius;
  float fp4_angle = 0.0f;
  int int4_tick = 0;
};

class RadarPings final : public TraitBase, public sim::ITick {
 public:
  RadarPings(RadarPingsInfoData info)
      : info_{std::move(info)} {}

  ORA_TRAIT_INTERFACES(RadarPings, OpenRA_Mods_Common_Traits_RadarPings,
                       sim::ITick)

  void Tick(Actor& self) override;

  RadarPing& Add(std::function<bool()> fn_is_visible, WPos position,
                 core::Color color, int duration);
  void Remove(RadarPing* ping) {
    std::erase_if(vec_pings_, [ping](const auto& p) {
      return p.get() == ping;
    });
  }

  const std::vector<std::unique_ptr<RadarPing>>& Pings() const {
    return vec_pings_;
  }
  const std::optional<WPos>& LastPingPosition() const {
    return wpos_last_ping_position_;
  }

 private:
  RadarPingsInfoData info_;
  std::vector<std::unique_ptr<RadarPing>> vec_pings_;
  std::optional<WPos> wpos_last_ping_position_;
};

struct PlayerRadarTerrainInfoData {
  static PlayerRadarTerrainInfoData Parse(const meta::RecordObject& rec_info);
};

class PlayerRadarTerrain final : public TraitBase, public sim::IWorldLoaded {
 public:
  explicit PlayerRadarTerrain(Actor& self);

  ORA_TRAIT_INTERFACES(PlayerRadarTerrain,
                       OpenRA_Mods_Common_Traits_PlayerRadarTerrain,
                       sim::IWorldLoaded)

  void WorldLoaded(sim::World& w, gfx::WorldRenderer* wr) override;

  bool IsInitialized() const { return b_is_initialized_; }

  std::pair<std::uint32_t, std::uint32_t> At(MPos uv) const {
    return layer_terrain_color_->Get(uv);
  }

  std::size_t SubscribeCellTerrainColorChanged(
      std::function<void(MPos)> fn) {
    vec_cell_terrain_color_changed_.push_back(std::move(fn));
    return vec_cell_terrain_color_changed_.size() - 1;
  }
  void RemoveCellTerrainColorChangedCallback(std::size_t n_token) {
    if (n_token < vec_cell_terrain_color_changed_.size())
      vec_cell_terrain_color_changed_[n_token] = nullptr;
  }

  static std::pair<std::uint32_t, std::uint32_t> GetColor(
      const map::Map& map,
      std::span<sim::IRadarTerrainLayer* const> vec_radar_terrain_layers,
      MPos uv);

 private:
  void UpdateShroudCell(PPos puv);
  void UpdateTerrainCell(MPos uv);
  void UpdateTerrainCellColor(MPos uv);

  sim::World* ptr_world_ = nullptr;
  sim::Shroud* ptr_shroud_ = nullptr;
  std::vector<sim::IRadarTerrainLayer*> vec_radar_terrain_layers_;
  std::vector<std::size_t> vec_rtl_tokens_;
  std::optional<std::size_t> n_map_tiles_token_;
  std::unique_ptr<map::CellLayer<std::pair<std::uint32_t, std::uint32_t>>>
      layer_terrain_color_;
  std::vector<std::function<void(MPos)>> vec_cell_terrain_color_changed_;
  bool b_is_initialized_ = false;
};

struct AppearsOnRadarInfoData {
  sim::ConditionalTraitData conditional;
  bool b_use_location = false;
  sim::PlayerRelationship kind_valid_relationships =
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;

  static AppearsOnRadarInfoData Parse(const meta::RecordObject& rec_info);
};

class AppearsOnRadar final : public TraitBase,
                             public sim::ConditionalTraitCore<AppearsOnRadar>,
                             public sim::IObservesVariables,
                             public sim::INotifyCreated,
                             public sim::IRadarSignature {
 public:
  AppearsOnRadar(const ActorInitializer& init, AppearsOnRadarInfoData info);

  ORA_TRAIT_INTERFACES(AppearsOnRadar,
                       OpenRA_Mods_Common_Traits_Radar_AppearsOnRadar,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::IRadarSignature)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AppearsOnRadar>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AppearsOnRadar>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void Created(Actor& self) override {
    sim::ConditionalTraitCore<AppearsOnRadar>::CoreCreated(self);
    const auto vec_modifiers =
        self.TraitsImplementing<sim::IRadarColorModifier>();
    ptr_modifier_ = vec_modifiers.empty() ? nullptr : vec_modifiers.front();
  }

  void PopulateRadarSignatureCells(
      Actor& self,
      std::vector<std::pair<CPos, core::Color>>& vec_dest) override;

 private:
  AppearsOnRadarInfoData info_;
  sim::IRadarColorModifier* ptr_modifier_ = nullptr;
};

struct ProvidesRadarInfoData {
  sim::ConditionalTraitData conditional;
  static ProvidesRadarInfoData Parse(const meta::RecordObject& rec_info);
};

class ProvidesRadar final : public TraitBase,
                            public sim::ConditionalTraitCore<ProvidesRadar>,
                            public sim::IObservesVariables,
                            public sim::INotifyCreated {
 public:
  ProvidesRadar(const ActorInitializer& init, ProvidesRadarInfoData info)
      : sim::ConditionalTraitCore<ProvidesRadar>(info.conditional),
        info_{std::move(info)} {}

  ORA_TRAIT_INTERFACES(ProvidesRadar,
                       OpenRA_Mods_Common_Traits_Radar_ProvidesRadar,
                       sim::IObservesVariables, sim::INotifyCreated)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<ProvidesRadar>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<ProvidesRadar>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void Created(Actor& self) override {
    sim::ConditionalTraitCore<ProvidesRadar>::CoreCreated(self);
  }

 private:
  ProvidesRadarInfoData info_;
};

struct RadarColorFromTerrainInfoData {
  std::string str_terrain;

  static RadarColorFromTerrainInfoData Parse(const meta::RecordObject& rec_info);
  core::Color GetColorFromTerrain(sim::World& world) const;
};

class RadarColorFromTerrain final : public TraitBase,
                                    public sim::IRadarColorModifier {
 public:
  RadarColorFromTerrain(Actor& self,
                        const RadarColorFromTerrainInfoData& info)
      : color_color_(info.GetColorFromTerrain(self.world())) {}

  ORA_TRAIT_INTERFACES(RadarColorFromTerrain,
                       OpenRA_Mods_Common_Traits_Radar_RadarColorFromTerrain,
                       sim::IRadarColorModifier)

  core::Color RadarColorOverride(Actor&, core::Color) override {
    return color_color_;
  }

 private:
  core::Color color_color_;
};

}
