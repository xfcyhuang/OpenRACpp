// UPSTREAM: OpenRA.Mods.Common/Widgets/ObserverArmyIconsWidget.cs @1f594e320
//          L24-211 + ObserverProductionIconsWidget.cs L25-281 +
//          ObserverSupportPowerIconsWidget.cs L23-199(观战三图标条)
//          The three observer icon strips (army composition, in-progress
//          production, support powers).
#pragma once
import std;

#include "core/rectangle.hpp"
#include "gfx/animation.hpp"
#include "mods/player_statistics.hpp"
#include "mods/production_palette_widget.hpp"
#include "mods/support_powers.hpp"
#include "mods/support_powers_widget.hpp"
#include "sim/trait_interfaces.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::ui {
class TooltipContainerWidget;
}

namespace ora::mods {

/// ArmyIcon(ObserverArmyIconsWidget.cs L206-210)
struct ArmyIcon {
  Rectangle rect_bounds = Rectangle::Empty();
  ArmyUnit* ptr_unit = nullptr;
};

class ObserverArmyIconsWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "ObserverArmyIconsWidget";

  ObserverArmyIconsWidget(ora::ui::WidgetArgs& args_args);
  ObserverArmyIconsWidget(const ObserverArmyIconsWidget& widget_other);
  ~ObserverArmyIconsWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new ObserverArmyIconsWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Draw() override;
  void Tick() override;

  std::function<sim::Player*()> fn_get_player;
  ArmyUnit* TooltipUnit() const { return ptr_tooltip_unit_; }
  std::function<ArmyUnit*()> fn_get_tooltip_unit;

  int int4_icon_width = 32;
  int int4_icon_height = 24;
  int int4_icon_spacing = 1;
  int int4_min_width = 240;

  std::string str_tooltip_template{"ARMY_TOOLTIP"};
  std::string str_tooltip_container;

  const std::vector<ArmyIcon>& ArmyIconsForTest() const {
    return vec_army_icons_;
  }

 private:
  sim::World* ptr_world_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;

  ArmyUnit* ptr_tooltip_unit_ = nullptr;
  ora::ui::TooltipContainerWidget* ptr_tooltip_container_widget_ = nullptr;
  bool b_tooltip_container_materialized_ = false;
  std::vector<ArmyIcon> vec_army_icons_;

  sim::Player* ptr_stats_player_ = nullptr;
  PlayerStatistics* ptr_stats_ = nullptr;
  int int4_last_icon_idx_ = 0;
  int int4_current_tooltip_token_ = 0;
};

/// ProductionIcon(ObserverProductionIconsWidget.cs L41-42 消费的复用面 =
/// mods::ProductionIcon)
/// The production icon reuse (the consumer face of L41-42 = mods::
/// ProductionIcon).
class ObserverProductionIconsWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "ObserverProductionIconsWidget";

  ObserverProductionIconsWidget(ora::ui::WidgetArgs& args_args);
  ObserverProductionIconsWidget(
      const ObserverProductionIconsWidget& widget_other);
  ~ObserverProductionIconsWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new ObserverProductionIconsWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Draw() override;
  void Tick() override;

  std::string str_tooltip_template{"PRODUCTION_TOOLTIP"};
  std::string str_tooltip_container;
  std::function<sim::Player*()> fn_get_player;

  int int4_icon_width = 32;
  int int4_icon_height = 24;
  int int4_icon_spacing = 1;
  int int4_min_width = 240;

  std::string str_clock_animation{"clock"};
  std::string str_clock_sequence{"idle"};
  std::string str_clock_palette{"chrome"};

  ProductionIcon* TooltipIcon() const { return ptr_tooltip_icon_; }
  std::function<ProductionIcon*()> fn_get_tooltip_icon;

  const std::vector<ProductionIcon>& ProductionIconsForTest() const {
    return vec_production_icons_;
  }

 private:
  sim::World* ptr_world_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;

  std::map<ProductionQueue*, std::unique_ptr<gfx::Animation>> map_clocks_;
  ProductionIcon* ptr_tooltip_icon_ = nullptr;
  ora::ui::TooltipContainerWidget* ptr_tooltip_container_widget_ = nullptr;
  bool b_tooltip_container_materialized_ = false;
  std::vector<ProductionIcon> vec_production_icons_;
  std::vector<Rectangle> vec_production_icons_bounds_;
  int int4_last_icon_idx_ = 0;
  int int4_current_tooltip_token_ = 0;
};

class ObserverSupportPowerIconsWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "ObserverSupportPowerIconsWidget";

  ObserverSupportPowerIconsWidget(ora::ui::WidgetArgs& args_args);
  ObserverSupportPowerIconsWidget(
      const ObserverSupportPowerIconsWidget& widget_other);
  ~ObserverSupportPowerIconsWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new ObserverSupportPowerIconsWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Draw() override;
  void Tick() override;

  std::string str_tooltip_template{"SUPPORT_POWER_TOOLTIP"};
  std::string str_tooltip_container;
  std::function<sim::Player*()> fn_get_player;

  int int4_icon_width = 32;
  int int4_icon_height = 24;
  int int4_icon_spacing = 1;

  std::string str_clock_animation{"clock"};
  std::string str_clock_sequence{"idle"};
  std::string str_clock_palette{"chrome"};

  SupportPowerIcon* TooltipIcon() const { return ptr_tooltip_icon_; }
  std::function<SupportPowerIcon*()> fn_get_tooltip_icon;

  const std::vector<SupportPowerIcon>& SupportPowerIconsForTest() const {
    return vec_support_power_icons_;
  }

 private:
  sim::World* ptr_world_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  std::map<std::string, std::unique_ptr<gfx::Animation>> map_clocks_;
  SupportPowerIcon* ptr_tooltip_icon_ = nullptr;
  ora::ui::TooltipContainerWidget* ptr_tooltip_container_widget_ = nullptr;
  bool b_tooltip_container_materialized_ = false;
  std::vector<SupportPowerIcon> vec_support_power_icons_;
  std::vector<Rectangle> vec_support_power_icons_bounds_;
  int int4_last_icon_idx_ = 0;
  int int4_current_tooltip_token_ = 0;
};

void RegisterObserverWidgets();

}  // namespace ora::mods
