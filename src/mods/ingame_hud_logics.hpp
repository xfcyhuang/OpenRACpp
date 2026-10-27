#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "ui/widget.hpp"

namespace ora::sim {
class World;
class Player;
}

namespace ora::net {
class OrderManager;
}

namespace ora::ui {
class LabelWithTooltipWidget;
}

namespace ora::mods {

class RadarWidget;
class ProductionPaletteWidget;
class ProductionTabsWidget;
class ProductionTypeButtonWidget;
class ProductionQueue;
class PlayerResources;
class PowerManager;
class DeveloperMode;

class IngameRadarDisplayLogic : public ora::ui::ChromeLogic {
 public:
  IngameRadarDisplayLogic(ora::ui::Widget& widget, sim::World& world);

 private:
  bool b_radar_enabled_ = false;
  bool b_cached_radar_enabled_ = false;
  core::Color color_block_{};
};

class ClassicProductionLogic : public ora::ui::ChromeLogic {
 public:
  ClassicProductionLogic(ora::ui::Widget& widget, sim::World& world);

 private:
  void SetupProductionGroupButton(ProductionTypeButtonWidget* button);
  void UpdateBackground(int int4_old_count, int int4_icons);
  static void SetMaximumVisibleRows(ProductionPaletteWidget& palette);

  ProductionPaletteWidget* ptr_palette_ = nullptr;
  sim::World* ptr_world_ = nullptr;
  ora::ui::Widget* ptr_background_ = nullptr;
  ora::ui::Widget* ptr_foreground_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_background_row_template_;
  std::unique_ptr<ora::ui::Widget> up_foreground_row_template_;
  ora::ui::Widget* ptr_background_bottom_ = nullptr;
};

class ProductionTabsLogic : public ora::ui::ChromeLogic {
 public:
  ProductionTabsLogic(ora::ui::Widget& widget, sim::World& world);
  ~ProductionTabsLogic() override;

 private:
  void SetupProductionGroupButton(ProductionTypeButtonWidget* button);

  ProductionTabsWidget* ptr_tabs_ = nullptr;
  sim::World* ptr_world_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_icon_template_;
  std::uint64_t uint8_added_token_ = 0;
  std::uint64_t uint8_removed_token_ = 0;
};

class IngameCashCounterLogic : public ora::ui::ChromeLogic {
 public:
  IngameCashCounterLogic(ora::ui::Widget& widget, sim::World& world);
  void Tick() override;

 private:
  static constexpr float kDisplayFracPerFrame = 0.07f;
  static constexpr int kDisplayDeltaPerFrame = 37;

  sim::World* ptr_world_ = nullptr;
  sim::Player* ptr_player_ = nullptr;
  PlayerResources* ptr_player_resources_ = nullptr;
  ora::ui::LabelWithTooltipWidget* ptr_cash_label_ = nullptr;
  core::CachedTransform<std::tuple<int, int>, std::string>
      cache_silo_usage_tooltip_;
  int int4_next_cash_tick_time_ = 0;
  int int4_display_resources_ = 0;
  std::string str_silo_usage_tooltip_;
};

class IngamePowerCounterLogic : public ora::ui::ChromeLogic {
 public:
  IngamePowerCounterLogic(ora::ui::Widget& widget, sim::World& world);
};

class IngameSiloBarLogic : public ora::ui::ChromeLogic {
 public:
  IngameSiloBarLogic(ora::ui::Widget& widget, sim::World& world);
};

class GameTimerLogic : public ora::ui::ChromeLogic {
 public:
  GameTimerLogic(ora::ui::Widget& widget, net::OrderManager& order_manager,
                 sim::World& world);
};

void RegisterIngameHudLogics();

}
