#pragma once
import std;

#include "mods/menu_logics.hpp"
#include "ui/widget.hpp"

namespace ora::sim {
class World;
}
namespace ora::net {
class OrderManager;
}
namespace ora::gfx {
class WorldRenderer;
}
namespace ora::game {
class ModData;
}

namespace ora::ui {
class ScrollItemWidget;
class ScrollPanelWidget;
}

namespace ora::mods {

class GameInfoLogic final : public ora::ui::ChromeLogic {
 public:
  GameInfoLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                sim::World& world, IngameInfoPanel initial_panel,
                std::function<void(bool)> fn_hide_menu,
                std::function<void()> fn_close_menu);

  IngameInfoPanel ActivePanelForTest() const { return active_panel_; }
  const std::vector<IngameInfoPanel>& VisiblePanelsForTest() const {
    return vec_visible_panels_;
  }
  const std::string& MapTitleForTest() const { return str_map_title_; }
  static void LeaveChatPanelForTest(ora::ui::Widget& widget_chat_panel);

 private:
  void SetupObjectivesPanel(ora::ui::ButtonWidget* ptr_tab_button,
                            ora::ui::Widget& widget_panel_container);
  void SetupMapPanel(ora::ui::ButtonWidget* ptr_tab_button,
                     ora::ui::Widget& widget_panel_container);
  void SetupLobbyOptionsPanel(ora::ui::ButtonWidget* ptr_tab_button,
                              ora::ui::Widget& widget_panel_container);
  void SetupDebugPanel(ora::ui::ButtonWidget* ptr_tab_button,
                       ora::ui::Widget& widget_panel_container);
  void SetupChatPanel(ora::ui::ButtonWidget* ptr_tab_button,
                      ora::ui::Widget& widget_panel_container);

  sim::World* ptr_world_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
  std::function<void(bool)> fn_hide_menu_;
  std::function<void()> fn_close_menu_;
  std::string str_objectives_panel_;
  IngameInfoPanel active_panel_ = IngameInfoPanel::AutoSelect;
  bool b_has_error_ = false;
  std::vector<IngameInfoPanel> vec_visible_panels_;
  std::string str_map_title_;
};

class GameInfoBriefingLogic final : public ora::ui::ChromeLogic {
 public:
  GameInfoBriefingLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                        sim::World& world);

  const std::string& BriefingTextForTest() const { return str_text_; }

 private:
  std::string str_text_;
};

class GameInfoObjectivesLogic final : public ora::ui::ChromeLogic {
 public:
  GameInfoObjectivesLogic(ora::ui::Widget& widget, sim::World& world);
  ~GameInfoObjectivesLogic() override;

  MissionObjectives* ObjectivesTraitForTest() const {
    return ptr_mission_objectives_;
  }
  const std::string& MissionStatusTextForTest() const {
    return str_mission_status_;
  }

 private:
  void PopulateObjectivesList();

  MissionObjectives* ptr_mission_objectives_ = nullptr;
  sim::Player* ptr_player_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_objectives_panel_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_template_;
  std::size_t n_objective_added_token_ = 0;
  std::string str_mission_status_;
};

class GameInfoStatsLogic final : public ora::ui::ChromeLogic {
 public:
  GameInfoStatsLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                     sim::World& world, net::OrderManager& order_manager,
                     gfx::WorldRenderer* ptr_world_renderer,
                     std::function<void(bool)> fn_hide_menu,
                     std::function<void()> fn_close_menu);

  const std::vector<std::string>& TeamHeadersForTest() const {
    return vec_team_headers_;
  }
  const std::vector<std::string>& PlayerRowsForTest() const {
    return vec_player_rows_;
  }
  const std::vector<std::string>& SpectatorRowsForTest() const {
    return vec_spectator_rows_;
  }
  const std::string& StatusTextForTest() const { return str_status_text_; }

 private:
  void KickAction(net::SessionClient& client, std::function<bool()> fn_is_vote_kick);

  sim::World* ptr_world_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
  net::OrderManager* ptr_order_manager_ = nullptr;
  std::function<void(bool)> fn_hide_menu_;
  std::function<void()> fn_close_menu_;
  std::vector<std::string> vec_team_headers_;
  std::vector<std::string> vec_player_rows_;
  std::vector<std::string> vec_spectator_rows_;
  std::string str_status_text_;
  std::unique_ptr<ora::ui::ScrollItemWidget> up_team_template_;
  std::unique_ptr<ora::ui::Widget> up_player_template_;
  std::unique_ptr<ora::ui::Widget> up_spectator_template_;
};

void RegisterGameInfoLogics();

}
