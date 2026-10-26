// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/ 的 HUD 第一组
//          @1f594e320:
//          IngameRadarDisplayLogic.cs L18-52 全文 /
//          ClassicProductionLogic.cs L13-186 全文 /
//          ProductionTabsLogic.cs L13-97 全文 /
//          IngameCashCounterLogic.cs L14-86 全文 /
//          IngamePowerCounterLogic.cs L14-55 全文 /
//          IngameSiloBarLogic.cs L14-49 全文 /
//          GameTimerLogic.cs L14-95 全文。
//          The HUD group one under Ingame/ @1f594e320: the whole of
//          IngameRadarDisplayLogic.cs L18-52 / ClassicProductionLogic.cs
//          L13-186 / ProductionTabsLogic.cs L13-97 / IngameCashCounterLogic.cs
//          L14-86 / IngamePowerCounterLogic.cs L14-55 / IngameSiloBarLogic.cs
//          L14-49 / GameTimerLogic.cs L14-95.
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

/// IngameRadarDisplayLogic.cs L18-52
/// IngameRadarDisplayLogic.cs L18-52.
class IngameRadarDisplayLogic : public ora::ui::ChromeLogic {
 public:
  IngameRadarDisplayLogic(ora::ui::Widget& widget, sim::World& world);

 private:
  bool b_radar_enabled_ = false;
  bool b_cached_radar_enabled_ = false;
  core::Color color_block_{};
};

/// ClassicProductionLogic.cs L13-186
/// ClassicProductionLogic.cs L13-186.
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

/// ProductionTabsLogic.cs L13-97
/// ProductionTabsLogic.cs L13-97.
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

/// IngameCashCounterLogic.cs L14-86
/// IngameCashCounterLogic.cs L14-86.
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

/// IngamePowerCounterLogic.cs L14-55
/// IngamePowerCounterLogic.cs L14-55.
class IngamePowerCounterLogic : public ora::ui::ChromeLogic {
 public:
  IngamePowerCounterLogic(ora::ui::Widget& widget, sim::World& world);
};

/// IngameSiloBarLogic.cs L14-49
/// IngameSiloBarLogic.cs L14-49.
class IngameSiloBarLogic : public ora::ui::ChromeLogic {
 public:
  IngameSiloBarLogic(ora::ui::Widget& widget, sim::World& world);
};

/// GameTimerLogic.cs L14-95
/// GameTimerLogic.cs L14-95.
class GameTimerLogic : public ora::ui::ChromeLogic {
 public:
  GameTimerLogic(ora::ui::Widget& widget, net::OrderManager& order_manager,
                 sim::World& world);
};

void RegisterIngameHudLogics();

}  // namespace ora::mods
