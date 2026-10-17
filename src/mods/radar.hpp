// UPSTREAM: OpenRA.Mods.Common/Traits/World/RadarPings.cs @e180e3225
//          L20-115 + Traits/Player/PlayerRadarTerrain.cs L19-96 +
//          Traits/Radar/{AppearsOnRadar,ProvidesRadar,RadarColorFromTerrain}.cs
//          (雷达链五件:ping 星箭头动画/玩家雷达地形缓存/minimap 签名三元
//          组/纯开关/地形色覆写)
//          L20-115 plus PlayerRadarTerrain.cs L19-96 and the three Radar/
//          traits (the five radar-chain pieces: the ping star animation,
//          the player radar-terrain cache, the minimap signature triple,
//          the bare switch, the terrain-color override).
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

/// RadarPingsInfo(L21-28)的解析面
/// The parsed face of RadarPingsInfo (L21-28).
struct RadarPingsInfoData {
  int int4_from_radius = 200;    // L23
  int int4_to_radius = 15;       // L24
  int int4_shrink_speed = 4;     // L25
  float fp4_rotation_speed = 0.12f;  // L26

  static RadarPingsInfoData Parse(const meta::RecordObject& rec_info);
};

/// RadarPing(L69-114):三点星箭头的收缩旋转;Tick 到期返 false
/// RadarPing (L69-114): the shrinking rotating three-point star; Tick
/// returns false on expiry.
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

  /// Points(L109-114):三点星 | Points (L109-114): the three-point star.
  std::array<core::Vector2, 3> Points(int2 int2_center) const;

 private:
  int int4_radius;
  float fp4_angle = 0.0f;
  int int4_tick = 0;
};

/// RadarPings(L31-67):world actor 的 ping 账本(Tick 排水到期项)
/// RadarPings (L31-67): the world actor's ping ledger (Tick drains the
/// expired entries).
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

/// PlayerRadarTerrainInfo(L19-25)的解析面
/// The parsed face of PlayerRadarTerrainInfo (L19-25).
struct PlayerRadarTerrainInfoData {
  static PlayerRadarTerrainInfoData Parse(const meta::RecordObject& rec_info);
};

/// PlayerRadarTerrain(L28-95):玩家侧雷达地形色缓存(迷雾可见格驱动)
/// PlayerRadarTerrain (L28-95): the player-side radar terrain-color cache
/// (fog-visible-cell driven).
class PlayerRadarTerrain final : public TraitBase, public sim::IWorldLoaded {
 public:
  explicit PlayerRadarTerrain(Actor& self);

  ORA_TRAIT_INTERFACES(PlayerRadarTerrain,
                       OpenRA_Mods_Common_Traits_PlayerRadarTerrain,
                       sim::IWorldLoaded)

  void WorldLoaded(sim::World& w, gfx::WorldRenderer* wr) override;

  bool IsInitialized() const { return b_is_initialized_; }

  /// this[uv](L85) | this[uv] (L85).
  std::pair<std::uint32_t, std::uint32_t> At(MPos uv) const {
    return layer_terrain_color_->Get(uv);
  }

  /// CellTerrainColorChanged 事件(L37):订阅令牌对(RadarWidget 的退订面)
  /// The CellTerrainColorChanged event (L37): the subscribe-token pair
  /// (RadarWidget's unsubscribe face).
  std::size_t SubscribeCellTerrainColorChanged(
      std::function<void(MPos)> fn) {
    vec_cell_terrain_color_changed_.push_back(std::move(fn));
    return vec_cell_terrain_color_changed_.size() - 1;
  }
  void RemoveCellTerrainColorChangedCallback(std::size_t n_token) {
    if (n_token < vec_cell_terrain_color_changed_.size())
      vec_cell_terrain_color_changed_[n_token] = nullptr;
  }

  /// GetColor(L87-95):IRadarTerrainLayer 首命中,回落 map.GetTerrainColorPair
  /// GetColor (L87-95): the first IRadarTerrainLayer hit, falling back to
  /// map.GetTerrainColorPair.
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

/// AppearsOnRadarInfo(L20-28)的解析面
/// The parsed face of AppearsOnRadarInfo (L20-28).
struct AppearsOnRadarInfoData {
  sim::ConditionalTraitData conditional;
  bool b_use_location = false;  // L23
  sim::PlayerRelationship kind_valid_relationships =
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;  // L26

  static AppearsOnRadarInfoData Parse(const meta::RecordObject& rec_info);
};

/// AppearsOnRadar(L31-63):IRadarSignature(占用格或中心位 + 关系过滤 +
/// IRadarColorModifier 覆写)
/// AppearsOnRadar (L31-63): IRadarSignature (occupied cells or center
/// position, relationship filter, IRadarColorModifier override).
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

/// ProvidesRadarInfo(L15-18)的解析面
/// The parsed face of ProvidesRadarInfo (L15-18).
struct ProvidesRadarInfoData {
  sim::ConditionalTraitData conditional;
  static ProvidesRadarInfoData Parse(const meta::RecordObject& rec_info);
};

/// ProvidesRadar(L20-24):雷达开关的纯承载
/// ProvidesRadar (L20-24): the bare radar-switch carrier.
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

/// RadarColorFromTerrainInfo(L17-29)的解析面 + GetColorFromTerrain(L22-26)
/// The parsed face of RadarColorFromTerrainInfo (L17-29) plus
/// GetColorFromTerrain (L22-26).
struct RadarColorFromTerrainInfoData {
  std::string str_terrain;  // L20([FieldLoader.Require])

  static RadarColorFromTerrainInfoData Parse(const meta::RecordObject& rec_info);
  core::Color GetColorFromTerrain(sim::World& world) const;
};

/// RadarColorFromTerrain(L31-41):IRadarColorModifier(地形类型色覆写)
/// RadarColorFromTerrain (L31-41): IRadarColorModifier (the terrain-type
/// color override).
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

}  // namespace ora::mods
