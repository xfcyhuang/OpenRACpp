// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/MainMenuLogic.cs @e180e3225
//          L25-566 全文。
#pragma once
import std;

#include "game/web_services.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class ModData;
}

namespace ora::sim {
class World;
}

namespace ora::net {
class ConnectionTarget;
}

namespace ora::map {
class MapCache;
}

namespace ora::ui {
class ButtonWidget;
class DropDownButtonWidget;
class LabelWidget;
class ScrollPanelWidget;
}

namespace ora::mods {

class MainMenuLogic final : public ora::ui::ChromeLogic {
 public:
  enum class MenuType {
    Main = 0,
    Singleplayer = 1,
    Extras = 2,
    MapEditor = 3,
    StartupPrompts = 4,
    None = 5,
  };

  enum class MenuPanel {
    None = 0,
    Missions = 1,
    Skirmish = 2,
    Multiplayer = 3,
    MapEditor = 4,
    Replays = 5,
    GameSaves = 6,
  };

  MainMenuLogic(ora::ui::Widget& widget, sim::World& world,
                game::ModData& mod_data);
  ~MainMenuLogic() override;

 private:
  struct NewsItem {
    std::string str_title;
    std::string str_author;
    std::string str_date_time;
    std::string str_content;
  };

  map::MapCache& MapCache();
  void SwitchMenu(MenuType type);
  void BuildNewsPanel();
  void LoadAndDisplayNews(ora::ui::Widget* ptr_news_bg);
  static void LoadMapIntoEditor(const std::string& str_uid);
  void OpenNewsPanel(ora::ui::DropDownButtonWidget& button);
  void OnRemoteDirectConnect(const net::ConnectionTarget& endpoint);
  void SetNewsStatus(const std::string& str_message);
  std::vector<NewsItem> ParseNews(const std::string& str_path);
  void DisplayNews(const std::vector<NewsItem>& vec_news_items);
  void RemoveShellmapUI();
  void StartSkirmishGame();
  void OpenMissionBrowserPanel(const std::string& str_map);
  void OpenEncyclopediaPanel();
  void OpenSkirmishLobbyPanel();
  void OpenMultiplayerPanel();
  void OpenReplayBrowserPanel();
  void OpenGameSaveBrowserPanel();
  void OpenMenuBasedOnLastGame();

  ora::ui::Widget* ptr_root_menu_ = nullptr;
  ora::ui::Widget* ptr_menu_ = nullptr;
  ora::ui::Widget* ptr_news_bg_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_news_panel_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_news_panel_owner_;
  int int4_max_news_height_ = 0;
  std::unique_ptr<ora::ui::Widget> up_news_template_;
  ora::ui::LabelWidget* ptr_news_status_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
  std::unique_ptr<game::WebServices> up_web_services_;
  std::vector<NewsItem> vec_news_cache_;
  MenuType kind_menu_type_ = MenuType::Main;
  bool b_news_open_ = false;
  std::function<void()> fn_on_shellmap_loaded_unsubscribe_;
  std::function<void()> fn_before_game_start_unsubscribe_;
  std::function<void()> fn_on_remote_direct_connect_unsubscribe_;

  static bool b_fetched_news_;
  static MenuPanel kind_last_game_state_;
};

void RegisterMainMenuLogic();

}  // namespace ora::mods
