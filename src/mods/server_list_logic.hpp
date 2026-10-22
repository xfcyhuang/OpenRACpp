#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "server/game_server.hpp"
#include "ui/widget.hpp"
#include "ui/widgets/button_widget.hpp"

namespace ora::game {
class ModData;
class WebServices;
}
namespace ora::map {
class MapPreview;
}
namespace ora::net {
class ConnectionTarget;
}
namespace ora::server {
class LanProbe;
}
namespace ora::ui {
class ButtonWidget;
class DropDownButtonWidget;
class ImageWidget;
class LabelWidget;
class LabelWithTooltipWidget;
class LogicTickerWidget;
class MapPreviewWidget;
class ScrollItemWidget;
class ScrollPanelWidget;
}

namespace ora::mods {

class ServerListLogic final : public ora::ui::ChromeLogic {
 public:
  enum class SearchStatus { Fetching, Failed, NoGames, Hidden };

  ServerListLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                  std::function<void(server::GameServerData&)> fn_on_join);
  ~ServerListLogic() override;

  void RefreshServerList();

  std::string ProgressLabelText();

  SearchStatus SearchStatusForTest() const { return kind_search_; }
  const std::vector<const server::GameServerData*>& RowGamesForTest() const {
    return vec_row_games_;
  }
  void SelectServerForTest(std::size_t sz_row) {
    if (sz_row < vec_row_games_.size())
      SelectServer(vec_row_games_[sz_row]);
  }
  void JoinClickedForTest() {
    if (ptr_join_button_ != nullptr && ptr_join_button_->fn_on_click)
      ptr_join_button_->fn_on_click();
  }
  int PlayerCountForTest() const { return int4_player_count_; }
  const server::GameServerData* CurrentServerForTest() const {
    return ptr_current_;
  }
  void ToggleFilterFlagForTest(int int4_flag) { ToggleFilterFlag(int4_flag); }

 private:
  std::string PlayerLabel(const server::GameServerData& game);
  int GroupSortOrder(const server::GameServerData& test_entry);
  void SelectServer(const server::GameServerData* ptr_server);
  void RefreshServerListInner(
      std::shared_ptr<std::vector<server::GameServerData>> vec_games,
      bool b_games_valid);
  std::vector<std::unique_ptr<ora::ui::Widget>> LoadGameRows(
      std::vector<server::GameServerData>& vec_games,
      ora::ui::ScrollItemWidget*& ptr_next_server_row);
  std::string GetStateLabel(const server::GameServerData* ptr_game);
  core::Color GetStateColor(const server::GameServerData& game,
                            ora::ui::LabelWidget& label_status,
                            bool b_darkened = false);
  bool Filtered(const server::GameServerData& game);
  void ToggleFilterFlag(int int4_flag);

  game::ModData* ptr_mod_data_ = nullptr;
  std::function<void(server::GameServerData&)> fn_on_join_;
  std::unique_ptr<game::WebServices> up_services_;
  std::shared_ptr<server::LanProbe> up_probe_;
  std::shared_ptr<bool> ptr_alive_ = std::make_shared<bool>(true);
  std::shared_ptr<std::vector<server::GameServerData>> vec_games_;
  std::string str_mod_id_;
  std::string str_mod_version_;
  std::unique_ptr<ora::ui::Widget> up_filters_panel_;

  std::string str_playing_;
  std::string str_waiting_;
  std::string str_no_server_selected_;
  std::string str_map_status_searching_;
  std::string str_map_classification_unknown_;
  std::string str_password_protected_;
  std::string str_waiting_for_players_;
  std::string str_server_shutting_down_;
  std::string str_unknown_server_state_;
  std::string str_search_status_failed_;
  std::string str_search_status_no_games_;
  std::string str_bot_player_;
  std::string str_players_;
  std::string str_no_team_;
  std::string str_spectators_;

  core::CachedTransform<int, std::string> transform_players_;
  core::CachedTransform<int, std::string> transform_bots_;
  core::CachedTransform<int, std::string> transform_spectators_;
  core::CachedTransform<std::int64_t, std::string> transform_minutes_;
  core::CachedTransform<int, std::string> transform_players_online_;

  core::Color color_incompatible_version_{};
  core::Color color_incompatible_game_{};
  core::Color color_incompatible_protected_{};
  core::Color color_protected_{};
  core::Color color_waiting_{};
  core::Color color_incompatible_waiting_{};
  core::Color color_game_started_{};
  core::Color color_incompatible_game_started_{};

  ora::ui::ScrollPanelWidget* ptr_server_list_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_header_template_;
  std::unique_ptr<ora::ui::Widget> up_server_template_;
  ora::ui::Widget* ptr_notice_container_ = nullptr;
  ora::ui::Widget* ptr_client_container_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_client_list_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_client_header_;
  std::unique_ptr<ora::ui::Widget> up_client_template_;
  ora::ui::MapPreviewWidget* ptr_map_preview_ = nullptr;
  ora::ui::ButtonWidget* ptr_join_button_ = nullptr;
  int int4_join_button_y_ = 0;

  const server::GameServerData* ptr_current_ = nullptr;
  map::MapPreview* ptr_current_map_ = nullptr;
  bool b_show_notices_ = false;
  int int4_player_count_ = 0;
  SearchStatus kind_search_ = SearchStatus::Fetching;
  bool b_active_query_ = false;

  std::vector<const server::GameServerData*> vec_row_games_;
};

class MultiplayerLogic final : public ora::ui::ChromeLogic {
 public:
  MultiplayerLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                   std::function<void()> fn_on_start,
                   std::function<void()> fn_on_exit,
                   net::ConnectionTarget* ptr_direct_connect_endpoint);
  ~MultiplayerLogic() override;

  void JoinClickedForTest() {
    if (ptr_direct_connect_button_ != nullptr &&
        ptr_direct_connect_button_->fn_on_click)
      ptr_direct_connect_button_->fn_on_click();
  }
  void BackClickedForTest() {
    if (ptr_back_button_ != nullptr && ptr_back_button_->fn_on_click)
      ptr_back_button_->fn_on_click();
  }
  ServerListLogic& ServerListForTest() { return *up_server_list_logic_; }
  bool CreateServerDisabledForTest() const {
    return ptr_create_server_button_ != nullptr &&
           ptr_create_server_button_->fn_is_disabled();
  }

 private:
  void OpenLobby();
  void Join(server::GameServerData& server_game);

  std::function<void()> fn_on_start_;
  std::function<void()> fn_on_exit_;
  std::unique_ptr<ServerListLogic> up_server_list_logic_;
  ora::ui::ButtonWidget* ptr_direct_connect_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_create_server_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_back_button_ = nullptr;
};

void RegisterServerListLogics();

}  // namespace ora::mods
