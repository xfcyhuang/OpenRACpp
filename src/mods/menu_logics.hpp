#pragma once
import std;

#include "sim/trait_interfaces.hpp"
#include "ui/widget.hpp"

namespace ora::sim {
class World;
}

namespace ora::net {
class OrderManager;
}

namespace ora::ui {
class ButtonWidget;
class ImageWidget;
class MenuButtonWidget;
class DropDownButtonWidget;
}

namespace ora::mods {

class SupportPowersWidget;
class ProductionTabsWidget;
class MissionObjectives;

enum class IngameInfoPanel {
  AutoSelect = 0,
  Map = 1,
  Objectives = 2,
  Debug = 3,
  Chat = 4,
  LobbyOptions = 5,
};

class AddFactionSuffixLogic final : public ora::ui::ChromeLogic {
 public:
  AddFactionSuffixLogic(ora::ui::Widget& widget, sim::World& world);
};

class SupportPowerBinLogic final : public ora::ui::ChromeLogic {
 public:
  explicit SupportPowerBinLogic(ora::ui::Widget& widget);
};

class SellOrderButtonLogic final : public ora::ui::ChromeLogic {
 public:
  SellOrderButtonLogic(ora::ui::Widget& widget, sim::World& world);
};

class RepairOrderButtonLogic final : public ora::ui::ChromeLogic {
 public:
  RepairOrderButtonLogic(ora::ui::Widget& widget, sim::World& world);
};

class PowerdownOrderButtonLogic final : public ora::ui::ChromeLogic {
 public:
  PowerdownOrderButtonLogic(ora::ui::Widget& widget, sim::World& world);
};

class BeaconOrderButtonLogic final : public ora::ui::ChromeLogic {
 public:
  BeaconOrderButtonLogic(ora::ui::Widget& widget, sim::World& world);
};

class MenuButtonsChromeLogic final : public ora::ui::ChromeLogic {
 public:
  MenuButtonsChromeLogic(ora::ui::Widget& widget, sim::World& world);
  ~MenuButtonsChromeLogic() override;

 private:
  void OpenMenuPanel(ora::ui::MenuButtonWidget& button,
                     ora::ui::WidgetArgs args_widget);

  sim::World* ptr_world_ = nullptr;
  ora::ui::Widget* ptr_world_root_ = nullptr;
  ora::ui::Widget* ptr_menu_root_ = nullptr;
  ora::ui::Widget* ptr_current_widget_ = nullptr;
  MissionObjectives* ptr_mission_objectives_ = nullptr;
  std::size_t n_objective_added_token_ = 0;
  bool b_disable_system_buttons_ = false;
};

void RegisterMenuLogics();

}
