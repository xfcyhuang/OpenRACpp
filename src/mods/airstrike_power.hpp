// UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/DirectionalSupportPower.cs
//          + AirstrikePower.cs + SelectDirectionalTarget.cs @1f594e320
#pragma once
import std;

#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "game/input.hpp"
#include "gfx/sprite.hpp"
#include "meta/generic_record.hpp"
#include "mods/support_powers.hpp"
#include "sim/order_generator.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {
class World;
}

namespace ora::mods {

struct DirectionalSupportPowerInfoData : SupportPowerInfoData {
  bool b_use_directional_target = false;
  std::vector<std::string> vec_arrows{"arrow-t",  "arrow-tl", "arrow-l",
                                      "arrow-bl", "arrow-b",  "arrow-br",
                                      "arrow-r",  "arrow-tr"};
  std::string str_direction_arrow_animation;
  std::string str_direction_arrow_palette{"chrome"};

  static DirectionalSupportPowerInfoData Parse(
      const meta::RecordObject& rec_info);
};

class DirectionalSupportPower : public SupportPower {
 public:
  DirectionalSupportPower(const ActorInitializer& init,
                          const DirectionalSupportPowerInfoData& info);

  void SelectTarget(Actor& self, const std::string& str_order,
                    SupportPowerManager& manager) override;

 protected:
  DirectionalSupportPowerInfoData directional_info_;
};

struct AirstrikePowerInfoData : DirectionalSupportPowerInfoData {
  std::string str_unit_type{"badr.bomber"};
  int int4_squad_size = 1;
  WVec wvec_squad_offset{-1536, 1536, 0};
  int int4_quantized_facings = 32;
  WDist dist_cordon{5120};
  std::string str_camera_actor;
  int int4_camera_remove_delay = 25;
  WDist dist_beacon_distance_offset = WDist::FromCells(6);

  static AirstrikePowerInfoData Parse(const meta::RecordObject& rec_info);
};

class AirstrikePower final : public DirectionalSupportPower {
 public:
  AirstrikePower(const ActorInitializer& init,
                 const AirstrikePowerInfoData& info);

  void Activate(Actor& self, const net::Order& order,
                SupportPowerManager& manager) override;

  std::vector<Actor*> SendAirstrike(Actor& self, WPos target,
                                    std::optional<WAngle> facing);

 private:
  void RemoveCamera(Actor* camera);
  void RemoveBeacon(class Beacon* beacon);

  AirstrikePowerInfoData airstrike_info_;
};

class SelectDirectionalTarget final : public sim::IOrderGenerator {
 public:
  SelectDirectionalTarget(sim::World& world, std::string str_order,
                          SupportPowerManager& manager,
                          const DirectionalSupportPowerInfoData& info);

  std::string OrderKey() const { return str_order_; }

  MouseButton ActionButton() const override {
    return game_settings_.ResolveActionButton(MouseActionType::SupportPower);
  }
  MouseButton CancelButton() const {
    return game_settings_.ResolveCancelButton(MouseActionType::SupportPower);
  }

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
  void Deactivate() override;
  void SelectionChanged(sim::World&, std::span<Actor* const>) override {}

 private:
  struct Arrow {
    gfx::Sprite sprite{};
    double fp8_end_angle = 0.0;
    WAngle angle_direction{0};
  };

  static double AngleOf(core::Vector2 delta);
  Arrow GetArrow(double degree);
  std::vector<Arrow> LoadArrows(const std::string& str_cursor_animation,
                                sim::World& world, int n_no_of_dividing_points);
  bool IsOutsideDragZone() const;

  std::string str_order_;
  SupportPowerManager& manager_;
  GameSettingsFace game_settings_;
  const DirectionalSupportPowerInfoData& info_;
  std::vector<Arrow> vec_direction_arrows_;
  CPos cell_target_;
  int2 int2_target_location_;
  core::Vector2 vec2_drag_direction_{0.0f, 0.0f};
  bool b_activated_ = false;
  bool b_drag_started_ = false;
  Arrow arrow_current_;
};

}  // namespace ora::mods
