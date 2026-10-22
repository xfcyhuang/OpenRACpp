#include "mods/server_list_logic.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "game/web_services.hpp"
#include "map/map_cache.hpp"
#include "meta/parse.hpp"
#include "mods/connection_logics.hpp"
#include "server/lan_probe.hpp"
#include "sim/trait_registry.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "yaml/mini_yaml.hpp"

#ifdef GetMessage
#undef GetMessage
#endif

namespace ora::mods {
namespace {

constexpr int kFilterWaiting = 1;
constexpr int kFilterEmpty = 2;
constexpr int kFilterProtected = 4;
constexpr int kFilterStarted = 8;
constexpr int kFilterIncompatible = 16;

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string Msg1(std::string_view str_key, std::string_view str_arg_name,
                 fluent::FluentValue val_value) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, fluent::FluentValue>>{
          {std::string{str_arg_name}, std::move(val_value)}});
}

}  // namespace

ServerListLogic::ServerListLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    std::function<void(server::GameServerData&)> fn_on_join)
    : ptr_mod_data_(&mod_data),
      fn_on_join_(std::move(fn_on_join)),
      transform_players_([](const int& int4_count) {
        return Msg1("label-players-count", "players",
                    fluent::FluentValue{
                        static_cast<std::int64_t>(int4_count)});
      }),
      transform_bots_([](const int& int4_count) {
        return Msg1("label-bots-count", "bots",
                    fluent::FluentValue{
                        static_cast<std::int64_t>(int4_count)});
      }),
      transform_spectators_([](const int& int4_count) {
        return Msg1("label-spectators-count", "spectators",
                    fluent::FluentValue{
                        static_cast<std::int64_t>(int4_count)});
      }),
      transform_minutes_([](const std::int64_t& int8_minutes) {
        return Msg1("label-in-progress-for", "minutes",
                    fluent::FluentValue{int8_minutes});
      }),
      transform_players_online_([](const int& int4_count) {
        return Msg1("label-players-online-count", "players",
                    fluent::FluentValue{
                        static_cast<std::int64_t>(int4_count)});
      }) {
  up_services_ = std::make_unique<game::WebServices>();
  str_mod_id_ = ptr_mod_data_->ManifestRef().Id();
  str_mod_version_ = ptr_mod_data_->ManifestRef().Metadata().str_version;

  str_playing_ = Msg("label-playing");
  str_waiting_ = Msg("label-waiting");
  str_no_server_selected_ = Msg("label-no-server-selected");
  str_map_status_searching_ = Msg("label-map-status-searching");
  str_map_classification_unknown_ = Msg("label-map-classification-unknown");
  str_password_protected_ = Msg("label-password-protected");
  str_waiting_for_players_ = Msg("label-waiting-for-players");
  str_server_shutting_down_ = Msg("label-server-shutting-down");
  str_unknown_server_state_ = Msg("label-unknown-server-state");
  str_search_status_failed_ = Msg("label-search-status-failed");
  str_search_status_no_games_ = Msg("label-search-status-no-games");
  str_bot_player_ = Msg("label-bot-player");
  str_players_ = Msg("label-players");
  str_no_team_ = Msg("label-no-team");
  str_spectators_ = Msg("label-spectators");

  auto* ptr_metrics = ora::ui::Ui::Instance().ChromeMetricsDeps();
  if (ptr_metrics != nullptr) {
    color_incompatible_version_ = ptr_metrics->GetColor("IncompatibleVersionColor");
    color_incompatible_game_ = ptr_metrics->GetColor("IncompatibleGameColor");
    color_incompatible_protected_ = ptr_metrics->GetColor("IncompatibleProtectedGameColor");
    color_protected_ = ptr_metrics->GetColor("ProtectedGameColor");
    color_waiting_ = ptr_metrics->GetColor("WaitingGameColor");
    color_incompatible_waiting_ = ptr_metrics->GetColor("IncompatibleWaitingGameColor");
    color_game_started_ = ptr_metrics->GetColor("GameStartedColor");
    color_incompatible_game_started_ = ptr_metrics->GetColor("IncompatibleGameStartedColor");
  }

  ptr_server_list_ = widget.Get<ora::ui::ScrollPanelWidget>("SERVER_LIST");
  up_header_template_ = ptr_server_list_->RemoveChild(
      *ptr_server_list_->Get<ora::ui::ScrollItemWidget>("HEADER_TEMPLATE"));
  up_server_template_ = ptr_server_list_->RemoveChild(
      *ptr_server_list_->Get<ora::ui::ScrollItemWidget>("SERVER_TEMPLATE"));

  ptr_notice_container_ = widget.GetOrNull("NOTICE_CONTAINER");
  if (ptr_notice_container_ != nullptr) {
    ptr_notice_container_->fn_is_visible = [this] { return b_show_notices_; };
    ptr_notice_container_->Get("OUTDATED_VERSION_LABEL")->fn_is_visible =
        [this] {
          return up_services_->ModVersionStatus_() ==
                 game::ModVersionStatus::Outdated;
        };
    ptr_notice_container_->Get("UNKNOWN_VERSION_LABEL")->fn_is_visible =
        [this] {
          return up_services_->ModVersionStatus_() ==
                 game::ModVersionStatus::Unknown;
        };
    ptr_notice_container_->Get("PLAYTEST_AVAILABLE_LABEL")->fn_is_visible =
        [this] {
          return up_services_->ModVersionStatus_() ==
                 game::ModVersionStatus::PlaytestAvailable;
        };
  }

  if (auto* ptr_notice_watcher =
          widget.GetOrNull<ora::ui::LogicTickerWidget>("NOTICE_WATCHER")) {
    if (ptr_notice_container_ != nullptr) {
      const int int4_container_height =
          ptr_notice_container_->bounds_widget.int4_height;
      ptr_notice_watcher->fn_on_tick =
          [this, int4_container_height] {
            const game::ModVersionStatus kind_status =
                up_services_->ModVersionStatus_();
            const bool b_show =
                kind_status != game::ModVersionStatus::NotChecked &&
                kind_status != game::ModVersionStatus::Latest;
            if (b_show != b_show_notices_) {
              const int int4_dir = b_show ? 1 : -1;
              ptr_server_list_->bounds_widget.int4_y +=
                  int4_dir * int4_container_height;
              ptr_server_list_->bounds_widget.int4_height -=
                  int4_dir * int4_container_height;
              b_show_notices_ = b_show;
            }
          };
    }
  }

  ptr_join_button_ = widget.GetOrNull<ora::ui::ButtonWidget>("JOIN_BUTTON");
  if (ptr_join_button_ != nullptr) {
    ptr_join_button_->fn_is_visible = [this] {
      return ptr_current_ != nullptr;
    };
    ptr_join_button_->fn_is_disabled = [this] {
      return ptr_current_ == nullptr || !ptr_current_->IsJoinable;
    };
    ptr_join_button_->fn_on_click = [this] {
      if (ptr_current_ != nullptr)
        fn_on_join_(const_cast<server::GameServerData&>(*ptr_current_));
    };
    int4_join_button_y_ = ptr_join_button_->bounds_widget.int4_y;
  }

  auto* ptr_progress_text = widget.Get<ora::ui::LabelWidget>("PROGRESS_LABEL");
  ptr_progress_text->fn_is_visible = [this] {
    return kind_search_ != SearchStatus::Hidden;
  };
  ptr_progress_text->fn_get_text = [this] { return ProgressLabelText(); };

  auto* ptr_filters_button =
      widget.GetOrNull<ora::ui::DropDownButtonWidget>(
          "FILTERS_DROPDOWNBUTTON");
  if (ptr_filters_button != nullptr) {
    auto* ptr_filters_panel = ora::ui::Ui::Instance().LoadWidget(
        "MULTIPLAYER_FILTER_PANEL", ptr_filters_button,
        ora::ui::WidgetArgs{});
    up_filters_panel_ = ptr_filters_button->RemoveChild(*ptr_filters_panel);

    if (auto* ptr_checkbox =
            up_filters_panel_->GetOrNull<ora::ui::CheckboxWidget>(
                "WAITING_FOR_PLAYERS")) {
      ptr_checkbox->fn_is_checked = [] {
        return (ora::ui::Ui::Instance().MPGameFiltersGetDeps() &
                kFilterWaiting) != 0;
      };
      ptr_checkbox->fn_on_click = [this] { ToggleFilterFlag(kFilterWaiting); };
    }

    if (auto* ptr_checkbox =
            up_filters_panel_->GetOrNull<ora::ui::CheckboxWidget>("EMPTY")) {
      ptr_checkbox->fn_is_checked = [] {
        return (ora::ui::Ui::Instance().MPGameFiltersGetDeps() &
                kFilterEmpty) != 0;
      };
      ptr_checkbox->fn_on_click = [this] { ToggleFilterFlag(kFilterEmpty); };
    }

    if (auto* ptr_checkbox =
            up_filters_panel_->GetOrNull<ora::ui::CheckboxWidget>(
                "ALREADY_STARTED")) {
      ptr_checkbox->fn_is_checked = [] {
        return (ora::ui::Ui::Instance().MPGameFiltersGetDeps() &
                kFilterStarted) != 0;
      };
      ptr_checkbox->fn_on_click = [this] { ToggleFilterFlag(kFilterStarted); };
    }

    if (auto* ptr_checkbox =
            up_filters_panel_->GetOrNull<ora::ui::CheckboxWidget>(
                "PASSWORD_PROTECTED")) {
      ptr_checkbox->fn_is_checked = [] {
        return (ora::ui::Ui::Instance().MPGameFiltersGetDeps() &
                kFilterProtected) != 0;
      };
      ptr_checkbox->fn_on_click = [this] {
        ToggleFilterFlag(kFilterProtected);
      };
    }

    if (auto* ptr_checkbox =
            up_filters_panel_->GetOrNull<ora::ui::CheckboxWidget>(
                "INCOMPATIBLE_VERSION")) {
      ptr_checkbox->fn_is_checked = [] {
        return (ora::ui::Ui::Instance().MPGameFiltersGetDeps() &
                kFilterIncompatible) != 0;
      };
      ptr_checkbox->fn_on_click = [this] {
        ToggleFilterFlag(kFilterIncompatible);
      };
    }

    ptr_filters_button->fn_is_disabled = [this] {
      return kind_search_ == SearchStatus::Fetching;
    };
    ptr_filters_button->fn_on_mouse_down =
        [this, ptr_filters_button](const MouseInput&) {
          ptr_filters_button->RemovePanel();
          ptr_filters_button->AttachPanel(up_filters_panel_->Clone());
        };
  }

  if (auto* ptr_reload_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("RELOAD_BUTTON")) {
    ptr_reload_button->fn_is_disabled = [this] {
      return kind_search_ == SearchStatus::Fetching;
    };
    ptr_reload_button->fn_on_click = [this] { RefreshServerList(); };

    if (auto* ptr_reload_icon =
            ptr_reload_button->GetOrNull<ora::ui::ImageWidget>(
                "IMAGE_RELOAD")) {
      auto ptr_disabled_image = std::make_shared<std::string>("disabled-0");
      ptr_reload_icon->fn_get_image_name =
          [this, ptr_disabled_image,
           ptr_icon = ptr_reload_icon]() -> std::string {
        return kind_search_ == SearchStatus::Fetching ? *ptr_disabled_image
                                                     : ptr_icon->str_image_name;
      };

      if (auto* ptr_reload_ticker = ptr_reload_icon->Get<ora::ui::LogicTickerWidget>(
              "ANIMATION")) {
        auto ptr_frame = std::make_shared<int>(0);
        ptr_reload_ticker->fn_on_tick =
            [this, ptr_frame, ptr_disabled_image] {
              *ptr_frame = kind_search_ == SearchStatus::Fetching
                               ? (*ptr_frame + 1) % 12
                               : 0;
              *ptr_disabled_image =
                  std::format("disabled-{}", *ptr_frame);
            };
      }
    }
  }

  if (auto* ptr_players_label =
          widget.GetOrNull<ora::ui::LabelWidget>("PLAYER_COUNT")) {
    ptr_players_label->fn_is_visible = [this] {
      return int4_player_count_ != 0;
    };
    ptr_players_label->fn_get_text = [this] {
      return transform_players_online_.Update(int4_player_count_);
    };
  }

  ptr_map_preview_ =
      widget.GetOrNull<ora::ui::MapPreviewWidget>("SELECTED_MAP_PREVIEW");
  if (ptr_map_preview_ != nullptr)
    ptr_map_preview_->fn_preview = [this] { return ptr_current_map_; };

  if (auto* ptr_map_title =
          widget.GetOrNull<ora::ui::LabelWithTooltipWidget>("SELECTED_MAP")) {
    auto ptr_font = std::make_shared<gfx::SpriteFont*>(
        &ora::ui::RequireWidgetFont(ptr_map_title->str_font));
    auto ptr_title = std::make_shared<
        core::CachedTransform<std::string, std::string>>(
        [ptr_font, ptr_map_title](const std::string& str_text) {
          const std::string str_truncated = ora::ui::TruncateText(
              str_text, ptr_map_title->bounds_widget.int4_width,
              **ptr_font);
          if (str_text != str_truncated)
            ptr_map_title->fn_get_tooltip_text = [str_text] {
              return str_text;
            };
          else
            ptr_map_title->fn_get_tooltip_text = nullptr;
          return str_truncated;
        });
    ptr_map_title->fn_get_text = [this, ptr_title] {
      if (ptr_current_map_ == nullptr)
        return str_no_server_selected_;
      if (ptr_current_map_->Status() == map::MapStatus::Searching)
        return str_map_status_searching_;
      if (ptr_current_map_->Class() == map::MapClassification::Unknown)
        return str_map_classification_unknown_;
      return ptr_title->Update(ptr_current_map_->Title());
    };
  }

  if (auto* ptr_ip = widget.GetOrNull<ora::ui::LabelWidget>("SELECTED_IP")) {
    ptr_ip->fn_is_visible = [this] { return ptr_current_ != nullptr; };
    ptr_ip->fn_get_text = [this]() -> std::string {
      return ptr_current_ != nullptr ? ptr_current_->Address
                                     : std::string{};
    };
  }

  if (auto* ptr_status =
          widget.GetOrNull<ora::ui::LabelWidget>("SELECTED_STATUS")) {
    ptr_status->fn_is_visible = [this] { return ptr_current_ != nullptr; };
    ptr_status->fn_get_text = [this] { return GetStateLabel(ptr_current_); };
    ptr_status->fn_get_color = [this, ptr_status] {
      if (ptr_current_ == nullptr)
        return ptr_status->color_text;
      return GetStateColor(*ptr_current_, *ptr_status);
    };
  }

  if (auto* ptr_mod_version =
          widget.GetOrNull<ora::ui::LabelWidget>("SELECTED_MOD_VERSION")) {
    ptr_mod_version->fn_is_visible = [this] {
      return ptr_current_ != nullptr;
    };
    ptr_mod_version->fn_get_color = [this, ptr_mod_version] {
      return ptr_current_ != nullptr && ptr_current_->IsCompatible
                 ? ptr_mod_version->color_text
                 : color_incompatible_version_;
    };

    auto ptr_font = std::make_shared<gfx::SpriteFont*>(
        &ora::ui::RequireWidgetFont(ptr_mod_version->str_font));
    auto ptr_version = std::make_shared<
        core::CachedTransform<std::string, std::string>>(
        [ptr_font, ptr_mod_version](const std::string& str_label) {
          return ora::ui::TruncateText(
              str_label, ptr_mod_version->bounds_widget.int4_width,
              **ptr_font);
        });
    ptr_mod_version->fn_get_text = [this, ptr_version]() -> std::string {
      return ptr_current_ != nullptr
                 ? ptr_version->Update(ptr_current_->ModLabel())
                 : std::string{};
    };
  }

  if (auto* ptr_selected_players =
          widget.GetOrNull<ora::ui::LabelWidget>("SELECTED_PLAYERS")) {
    ptr_selected_players->fn_is_visible = [this] {
      return ptr_current_ != nullptr &&
             (ptr_client_container_ == nullptr ||
              ptr_current_->vec_clients.empty());
    };
    ptr_selected_players->fn_get_text = [this]() -> std::string {
      return ptr_current_ != nullptr ? PlayerLabel(*ptr_current_)
                                     : std::string{};
    };
  }

  ptr_client_container_ = widget.GetOrNull("CLIENT_LIST_CONTAINER");
  if (ptr_client_container_ != nullptr) {
    auto* ptr_client_list = ora::ui::Ui::Instance().LoadWidget(
        "MULTIPLAYER_CLIENT_LIST", ptr_client_container_,
        ora::ui::WidgetArgs{});
    ptr_client_list_ = static_cast<ora::ui::ScrollPanelWidget*>(ptr_client_list);
    ptr_client_list_->fn_is_visible = [this] {
      return ptr_current_ != nullptr && !ptr_current_->vec_clients.empty();
    };
    up_client_header_ = ptr_client_list_->RemoveChild(
        *ptr_client_list_->Get<ora::ui::ScrollItemWidget>("HEADER"));
    up_client_template_ = ptr_client_list_->RemoveChild(
        *ptr_client_list_->Get<ora::ui::ScrollItemWidget>("TEMPLATE"));
    ptr_client_list_->RemoveChildren();
  }

  up_probe_ = std::make_shared<server::LanProbe>("OpenRALANGame");
  up_probe_->Start();

  RefreshServerList();
}

ServerListLogic::~ServerListLogic() {
  *ptr_alive_ = false;
}

std::string ServerListLogic::ProgressLabelText() {
  if (kind_search_ == SearchStatus::Failed)
    return str_search_status_failed_;
  if (kind_search_ == SearchStatus::NoGames)
    return str_search_status_no_games_;
  return std::string{};
}

std::string ServerListLogic::PlayerLabel(const server::GameServerData& game) {
  std::string str_label = transform_players_.Update(game.Players);
  if (game.Bots > 0)
    str_label += " " + transform_bots_.Update(game.Bots);
  if (game.Spectators > 0)
    str_label += " " + transform_spectators_.Update(game.Spectators);
  return str_label;
}

void ServerListLogic::RefreshServerList() {
  if (b_active_query_)
    return;

  kind_search_ = SearchStatus::Fetching;

  const std::string str_query_url = game::BuildHttpQuery(
      up_services_->ServerList,
      {{"protocol",
        std::to_string(server::GameServerData::kProtocolVersion)},
       {"engine", ora::ui::Ui::Instance().EngineVersionDeps()},
       {"mod", str_mod_id_},
       {"version", str_mod_version_}});

  b_active_query_ = true;

  const std::weak_ptr<bool> weak_alive = ptr_alive_;
  const std::shared_ptr<server::LanProbe> ptr_probe = up_probe_;
  ora::ui::Ui::Instance().FetchServerListDeps(
      str_query_url,
      [this, weak_alive, ptr_probe, str_query_url](
          bool b_ok, std::string str_body) {
        auto ptr_games =
            std::make_shared<std::vector<server::GameServerData>>();
        bool b_games_valid = false;

        if (b_ok) {
          b_games_valid = true;
          try {
            auto vec_nodes =
                yaml::MiniYaml::FromStream(str_body, str_query_url);
            for (const auto& node : vec_nodes) {
              auto opt_game =
                  server::GameServerData::TryFromYaml(node.Value);
              if (opt_game.has_value() && !opt_game->b_address_null)
                ptr_games->push_back(std::move(*opt_game));
            }
          } catch (...) {
            b_games_valid = false;
          }
        }

        auto vec_lan = std::vector<server::GameServerData>{};
        if (ptr_probe != nullptr && ptr_probe->IsValid()) {
          for (const server::LanProbe::Location& location :
               ptr_probe->Locations()) {
            try {
              if (location.str_data.empty())
                continue;

              auto pool = yaml::StringPool{};
              auto vec_beacon = yaml::MiniYaml::FromString(
                  location.str_data,
                  std::format("BeaconLocation_{}_{}",
                              location.str_address,
                              location.i8_last_advertised),
                  true, pool);
              if (vec_beacon.empty())
                continue;
              yaml::MiniYamlBuilder builder{vec_beacon.front().Value};

              yaml::MiniYamlNodeBuilder* ptr_id_node = nullptr;
              for (auto& node : builder.Nodes)
                if (node.Key != nullptr && *node.Key == "Id") {
                  ptr_id_node = &node;
                  break;
                }

              const std::string str_session_guid =
                  ora::ui::Ui::Instance().SessionGuidDeps();
              const std::string* ptr_id_value =
                  ptr_id_node != nullptr ? ptr_id_node->Value.Value : nullptr;

              if (ptr_id_node != nullptr &&
                  (ptr_id_value == nullptr ||
                   *ptr_id_value != str_session_guid)) {
                ptr_id_node->Value.Value = pool.Intern("-1");

                const std::size_t sz_colon =
                    location.str_address.find(':');
                const std::string str_ip =
                    sz_colon == std::string::npos
                        ? location.str_address
                        : location.str_address.substr(0, sz_colon);
                for (auto& node : builder.Nodes)
                  if (node.Key != nullptr && *node.Key == "Address" &&
                      node.Value.Value != nullptr) {
                    const std::string_view sv_old = *node.Value.Value;
                    const std::size_t sz_old_colon = sv_old.find(':');
                    const std::string str_port =
                        sz_old_colon == std::string_view::npos
                            ? std::string{}
                            : std::string{sv_old.substr(sz_old_colon + 1)};
                    node.Value.Value =
                        pool.Intern(str_ip + ":" + str_port);
                    break;
                  }

                builder.Nodes.emplace_back(
                    pool.Intern("Location"),
                    yaml::MiniYamlBuilder{pool.Intern("Local Network")});

                auto opt_game =
                    server::GameServerData::TryFromYaml(builder.Build());
                if (opt_game.has_value())
                  vec_lan.push_back(std::move(*opt_game));
              }
            } catch (...) {
            }
          }
        }

        auto vec_lan_dedup = std::vector<server::GameServerData>{};
        {
          auto set_addr = std::set<std::string>{};
          for (auto it = vec_lan.rbegin(); it != vec_lan.rend(); ++it)
            if (set_addr.insert(it->Address).second)
              vec_lan_dedup.push_back(std::move(*it));
          std::ranges::reverse(vec_lan_dedup);
        }

        if (b_games_valid) {
          for (auto& game : vec_lan_dedup)
            ptr_games->push_back(std::move(game));
        } else if (!vec_lan_dedup.empty()) {
          b_games_valid = true;
          *ptr_games = std::move(vec_lan_dedup);
        }

        ora::ui::Ui::Instance().RunAfterTickDefer(
            [this, weak_alive, ptr_games, b_games_valid] {
              if (weak_alive.expired())
                return;
              RefreshServerListInner(ptr_games, b_games_valid);
              b_active_query_ = false;
            });
      });
}

int ServerListLogic::GroupSortOrder(
    const server::GameServerData& test_entry) {
  if (!test_entry.IsCompatible)
    return test_entry.Mod == str_mod_id_ ? 1 : 0;
  if (test_entry.Mod == str_mod_id_)
    return test_entry.Version == str_mod_version_ ? 4 : 3;
  return 2;
}

void ServerListLogic::SelectServer(const server::GameServerData* ptr_server) {
  ptr_current_ = ptr_server;
  ptr_current_map_ = nullptr;
  if (ptr_server != nullptr) {
    auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
    if (ptr_cache != nullptr && !ptr_server->Map.empty())
      ptr_current_map_ = &ptr_cache->At(ptr_server->Map);
  }

  if (ptr_server != nullptr && ptr_map_preview_ != nullptr) {
    const std::vector<CPos> vec_spawns =
        ptr_current_map_ != nullptr ? ptr_current_map_->SpawnPoints()
                                    : std::vector<CPos>{};
    auto map_occupants = std::map<int, ora::ui::SpawnOccupant>{};
    for (const auto& client : ptr_server->vec_clients) {
      if (client.SpawnPoint - 1 >= 0 &&
          static_cast<std::size_t>(client.SpawnPoint - 1) <
              vec_spawns.size())
        map_occupants.insert_or_assign(
            client.SpawnPoint,
            ora::ui::SpawnOccupant{client,
                                   ptr_server->Mod != str_mod_id_});
    }

    ptr_map_preview_->fn_spawn_occupants =
        [map_occupants = std::move(map_occupants)] {
          return map_occupants;
        };
    ptr_map_preview_->fn_disabled_spawn_points =
        [vec_disabled = ptr_server->vec_disabled_spawn_points] {
          return vec_disabled;
        };
  }

  if (ptr_server == nullptr || ptr_server->vec_clients.empty()) {
    if (ptr_join_button_ != nullptr)
      ptr_join_button_->bounds_widget.int4_y = int4_join_button_y_;
    return;
  }

  if (ptr_join_button_ != nullptr && ptr_client_container_ != nullptr)
    ptr_join_button_->bounds_widget.int4_y =
        ptr_client_container_->bounds_widget.Bottom();

  if (ptr_client_list_ == nullptr)
    return;

  ptr_client_list_->RemoveChildren();

  auto vec_teams =
      std::vector<std::pair<int, std::vector<const server::GameClientData*>>>{};
  for (const auto& client : ptr_server->vec_clients)
    if (!client.IsSpectator) {
      bool b_found = false;
      for (auto& [int4_team, vec_members] : vec_teams)
        if (int4_team == client.Team) {
          vec_members.push_back(&client);
          b_found = true;
          break;
        }
      if (!b_found)
        vec_teams.emplace_back(
            client.Team,
            std::vector<const server::GameClientData*>{&client});
    }
  std::sort(vec_teams.begin(), vec_teams.end(),
            [](const auto& pair_a, const auto& pair_b) {
              return pair_a.first < pair_b.first;
            });

  auto vec_display =
      std::vector<std::pair<std::string,
                            std::vector<const server::GameClientData*>>>{};
  const bool b_no_teams = vec_teams.size() == 1;
  for (const auto& [int4_team, vec_members] : vec_teams) {
    std::string str_label =
        b_no_teams
            ? str_players_
            : (int4_team > 0
                   ? Msg1("label-team-name", "team",
                          fluent::FluentValue{
                              static_cast<std::int64_t>(int4_team)})
                   : str_no_team_);
    vec_display.emplace_back(std::move(str_label), vec_members);
  }

  bool b_has_spectators = false;
  for (const auto& client : ptr_server->vec_clients)
    if (client.IsSpectator) {
      b_has_spectators = true;
      break;
    }
  if (b_has_spectators) {
    auto vec_spectators = std::vector<const server::GameClientData*>{};
    for (const auto& client : ptr_server->vec_clients)
      if (client.IsSpectator)
        vec_spectators.push_back(&client);
    vec_display.emplace_back(str_spectators_, std::move(vec_spectators));
  }

  auto vec_factions = std::vector<std::string>{};
  if (const game::ActorInfo* ptr_world_actor_info =
          ptr_mod_data_->DefaultRules().FindActor("world"))
    for (const meta::RecordObject* rec_faction :
         ptr_world_actor_info->TraitInfosByInterface("FactionInfo"))
      if (const auto val_name =
              sim::RecordFieldString(*rec_faction, "InternalName"))
        vec_factions.emplace_back(*val_name);
  const auto ptr_known_factions =
      std::make_shared<const std::vector<std::string>>(
          std::move(vec_factions));

  for (const auto& [str_group, vec_members] : vec_display) {
    if (!str_group.empty()) {
      auto up_header = ora::ui::ScrollItemWidget::Setup(
          static_cast<ora::ui::ScrollItemWidget&>(*up_client_header_),
          [] { return false; }, [] {});
      up_header->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
          [str_group] { return str_group; };
      ptr_client_list_->AddChild(std::move(up_header));
    }

    for (const server::GameClientData* ptr_client : vec_members) {
      auto ptr_player_name = std::make_shared<
          core::CachedTransform<
              std::tuple<map::MapStatus, int, gfx::SpriteFont*>,
              std::string>>(
          [this, ptr_client](
              const std::tuple<map::MapStatus, int,
                               gfx::SpriteFont*>& tpl_key) {
            std::string str_name = ptr_client->Name;
            if (ptr_client->IsBot) {
              std::string str_msg;
              if (ptr_current_map_ != nullptr &&
                  ptr_current_map_->TryGetMessage(ptr_client->Name, str_msg))
                str_name = str_msg;
              else
                str_name = str_bot_player_;
            }
            return ora::ui::TruncateText(str_name, std::get<1>(tpl_key),
                                         *std::get<2>(tpl_key));
          });

      auto up_item = ora::ui::ScrollItemWidget::Setup(
          static_cast<ora::ui::ScrollItemWidget&>(*up_client_template_),
          [] { return false; }, [] {});
      if (!ptr_client->IsSpectator && ptr_server->Mod == str_mod_id_) {
        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        auto* ptr_font = &ora::ui::RequireWidgetFont(ptr_label->str_font);
        ptr_label->fn_get_text = [this, ptr_player_name, ptr_label,
                                  ptr_font] {
          if (ptr_current_map_ == nullptr)
            return std::string{};
          return ptr_player_name->Update(
              std::tuple{ptr_current_map_->Status(),
                         ptr_label->bounds_widget.int4_width, ptr_font});
        };
        ptr_label->fn_get_color = [color = ptr_client->Color] {
          return color;
        };

        auto* ptr_flag = up_item->Get<ora::ui::ImageWidget>("FLAG");
        ptr_flag->fn_is_visible = [] { return true; };
        ptr_flag->fn_get_image_collection = [] {
          return std::string{"flags"};
        };
        ptr_flag->fn_get_image_name =
            [ptr_client, ptr_known_factions]() -> std::string {
          return std::find(ptr_known_factions->begin(),
                           ptr_known_factions->end(),
                           ptr_client->Faction) !=
                         ptr_known_factions->end()
                     ? ptr_client->Faction
                     : std::string{"Random"};
        };
      } else {
        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("NOFLAG_LABEL");
        auto* ptr_font = &ora::ui::RequireWidgetFont(ptr_label->str_font);
        const core::Color color_client =
            ptr_client->IsSpectator
                ? core::Color::FromArgb(0xFFFFFFFFu)
                : ptr_client->Color;
        ptr_label->fn_get_text = [this, ptr_player_name, ptr_label,
                                  ptr_font] {
          if (ptr_current_map_ == nullptr)
            return std::string{};
          return ptr_player_name->Update(
              std::tuple{ptr_current_map_->Status(),
                         ptr_label->bounds_widget.int4_width, ptr_font});
        };
        ptr_label->fn_get_color = [color_client] { return color_client; };
      }

      ptr_client_list_->AddChild(std::move(up_item));
    }
  }
}

void ServerListLogic::RefreshServerListInner(
    std::shared_ptr<std::vector<server::GameServerData>> vec_games,
    bool b_games_valid) {
  ora::ui::ScrollItemWidget* ptr_next_server_row = nullptr;
  auto ptr_rows =
      std::make_shared<std::vector<std::unique_ptr<ora::ui::Widget>>>();

  if (b_games_valid)
    *ptr_rows = LoadGameRows(*vec_games, ptr_next_server_row);

  const std::weak_ptr<bool> weak_alive = ptr_alive_;
  ora::ui::Ui::Instance().RunAfterTickDefer(
      [this, weak_alive, vec_games = std::move(vec_games), b_games_valid,
       ptr_rows, ptr_next_server_row] {
        if (weak_alive.expired())
          return;

        ptr_server_list_->RemoveChildren();
        SelectServer(nullptr);

        if (!b_games_valid) {
          kind_search_ = SearchStatus::Failed;
          return;
        }

        if (ptr_rows->empty()) {
          kind_search_ = SearchStatus::NoGames;
          return;
        }

        kind_search_ = SearchStatus::Hidden;

        if (ora::ui::Ui::Instance().AllowDownloadingDeps()) {
          auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
          if (ptr_cache != nullptr) {
            auto vec_uids = std::vector<std::string>{};
            for (const auto& game : *vec_games)
              if (!Filtered(game))
                vec_uids.push_back(game.Map);
            if (!vec_uids.empty())
              ptr_cache->QueryRemoteMapDetails(
                  up_services_->MapRepository, vec_uids);
          }
        }

        vec_games_ = vec_games;

        for (auto& up_row : *ptr_rows)
          ptr_server_list_->AddChild(std::move(up_row));

        if (ptr_next_server_row != nullptr && ptr_next_server_row->fn_on_click)
          ptr_next_server_row->fn_on_click();

        int4_player_count_ = 0;
        for (const auto& game : *vec_games)
          int4_player_count_ += game.Players;
      });
}

std::vector<std::unique_ptr<ora::ui::Widget>> ServerListLogic::LoadGameRows(
    std::vector<server::GameServerData>& vec_games,
    ora::ui::ScrollItemWidget*& ptr_next_server_row) {
  ptr_next_server_row = nullptr;
  vec_row_games_.clear();
  auto vec_rows = std::vector<std::unique_ptr<ora::ui::Widget>>{};

  struct ModGroup {
    std::string str_label;
    std::vector<server::GameServerData*> vec_games;
  };

  auto vec_groups = std::vector<ModGroup>{};
  for (auto& game : vec_games) {
    bool b_found = false;
    for (auto& group : vec_groups)
      if (group.str_label == game.ModLabel()) {
        group.vec_games.push_back(&game);
        b_found = true;
        break;
      }
    if (!b_found)
      vec_groups.push_back(ModGroup{game.ModLabel(), {&game}});
  }

  std::stable_sort(vec_groups.begin(), vec_groups.end(),
                   [this](const ModGroup& group_a, const ModGroup& group_b) {
                     const int int4_a =
                         GroupSortOrder(*group_a.vec_games.front());
                     const int int4_b =
                         GroupSortOrder(*group_b.vec_games.front());
                     if (int4_a != int4_b)
                       return int4_a > int4_b;
                     return group_a.vec_games.size() >
                            group_b.vec_games.size();
                   });

  const auto list_order = [](const server::GameServerData& game) {
    if (game.State == 1 && game.Players > 0)
      return 0;
    if (game.State == 1 && game.Spectators > 0)
      return 1;
    if (game.State >= 2)
      return 2;
    return 3;
  };

  for (auto& mod_group : vec_groups) {
    bool b_all_filtered = true;
    for (const auto* game : mod_group.vec_games)
      if (!Filtered(*game)) {
        b_all_filtered = false;
        break;
      }
    if (b_all_filtered)
      continue;

    auto up_header = ora::ui::ScrollItemWidget::Setup(
        static_cast<ora::ui::ScrollItemWidget&>(*up_header_template_),
        [] { return false; }, [] {});
    const std::string str_header_title =
        mod_group.vec_games.front()->ModLabel();
    up_header->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
        [str_header_title] { return str_header_title; };
    vec_rows.push_back(std::move(up_header));

    auto vec_state_groups =
        std::vector<std::pair<int, std::vector<server::GameServerData*>>>{};
    for (auto* game : mod_group.vec_games) {
      const int int4_order = list_order(*game);
      bool b_found = false;
      for (auto& [int4_key, vec_list] : vec_state_groups)
        if (int4_key == int4_order) {
          vec_list.push_back(game);
          b_found = true;
          break;
        }
      if (!b_found)
        vec_state_groups.emplace_back(
            int4_order, std::vector<server::GameServerData*>{game});
    }
    std::sort(vec_state_groups.begin(), vec_state_groups.end(),
              [](const auto& pair_a, const auto& pair_b) {
                return pair_a.first < pair_b.first;
              });

    for (auto& [int4_key, vec_list] : vec_state_groups) {
      if (int4_key == 2)
        std::stable_sort(
            vec_list.begin(), vec_list.end(),
            [](const server::GameServerData* game_a,
               const server::GameServerData* game_b) {
              return game_a->Started > game_b->Started;
            });
      else
        std::stable_sort(
            vec_list.begin(), vec_list.end(),
            [](const server::GameServerData* game_a,
               const server::GameServerData* game_b) {
              return game_a->Players > game_b->Players;
            });

      for (auto* ptr_game : vec_list) {
        if (Filtered(*ptr_game))
          continue;

        const bool b_can_join = ptr_game->IsJoinable;
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            static_cast<ora::ui::ScrollItemWidget&>(*up_server_template_),
            [this, ptr_game] { return ptr_current_ == ptr_game; },
            [this, ptr_game] { SelectServer(ptr_game); },
            [this, ptr_game] { fn_on_join_(*ptr_game); });

        auto* ptr_title =
            up_item->GetOrNull<ora::ui::LabelWithTooltipWidget>("TITLE");
        if (ptr_title != nullptr) {
          ora::ui::TruncateLabelToTooltip(*ptr_title, ptr_game->Name);
          const core::Color color_fallback = color_incompatible_game_;
          ptr_title->fn_get_color =
              [ptr_title, b_can_join, color_fallback] {
                return b_can_join ? ptr_title->color_text : color_fallback;
              };
        }

        auto* ptr_password =
            up_item->GetOrNull<ora::ui::ImageWidget>("PASSWORD_PROTECTED");
        if (ptr_password != nullptr) {
          ptr_password->fn_is_visible = [ptr_game] {
            return ptr_game->Protected;
          };
          ptr_password->fn_get_image_name = [b_can_join]() -> std::string {
            return b_can_join ? "protected" : "protected-disabled";
          };
        }

        auto* ptr_auth =
            up_item->GetOrNull<ora::ui::ImageWidget>(
                "REQUIRES_AUTHENTICATION");
        if (ptr_auth != nullptr) {
          ptr_auth->fn_is_visible = [ptr_game] {
            return ptr_game->Authentication;
          };
          ptr_auth->fn_get_image_name = [b_can_join]() -> std::string {
            return b_can_join ? "authentication"
                              : "authentication-disabled";
          };

          if (ptr_game->Protected && ptr_password != nullptr)
            ptr_auth->bounds_widget.int4_x -=
                ptr_password->bounds_widget.int4_width + 5;
        }

        auto* ptr_players_label =
            up_item->GetOrNull<ora::ui::LabelWithTooltipWidget>("PLAYERS");
        if (ptr_players_label != nullptr) {
          const std::string str_label = std::format(
              "{} / {}{}",
              ptr_game->Players + ptr_game->Bots,
              ptr_game->MaxPlayers + ptr_game->Bots,
              ptr_game->Spectators > 0
                  ? std::format(" + {}", ptr_game->Spectators)
                  : std::string{});
          ptr_players_label->fn_get_text = [str_label] {
            return str_label;
          };
          const core::Color color_fallback = color_incompatible_game_;
          ptr_players_label->fn_get_color =
              [ptr_players_label, b_can_join, color_fallback] {
                return b_can_join ? ptr_players_label->color_text
                                  : color_fallback;
              };

          if (!ptr_game->vec_clients.empty()) {
            auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
            map::MapPreview* ptr_preview =
                ptr_cache != nullptr
                    ? &ptr_cache->At(ptr_game->Map)
                    : &map::MapCache::UnknownMap();
            auto ptr_tooltip = std::make_shared<
                core::CachedTransform<map::MapStatus, std::string>>(
                [this, ptr_game, ptr_preview](const map::MapStatus&) {
                  auto vec_names = std::vector<std::string>{};
                  for (const auto& client : ptr_game->vec_clients) {
                    if (client.IsBot) {
                      std::string str_msg;
                      vec_names.push_back(
                          ptr_preview->TryGetMessage(client.Name, str_msg)
                              ? str_msg
                              : str_bot_player_);
                    } else
                      vec_names.push_back(client.Name);
                  }
                  if (ptr_game->vec_clients.size() > 10) {
                    vec_names.resize(9);
                    vec_names.push_back(Msg1(
                        "label-other-players-count", "players",
                        fluent::FluentValue{
                            static_cast<std::int64_t>(
                                ptr_game->vec_clients.size() - 9)}));
                  }
                  std::string str_out{};
                  for (std::size_t sz_i = 0; sz_i < vec_names.size();
                       ++sz_i) {
                    if (sz_i > 0)
                      str_out += "\n";
                    str_out += vec_names[sz_i];
                  }
                  return str_out;
                });
            ptr_players_label->fn_get_tooltip_text =
                [ptr_tooltip, ptr_preview] {
                  return ptr_tooltip->Update(ptr_preview->Status());
                };
          } else
            ptr_players_label->fn_get_tooltip_text = nullptr;
        }

        auto* ptr_state =
            up_item->GetOrNull<ora::ui::LabelWidget>("STATUS");
        if (ptr_state != nullptr) {
          const std::string str_label =
              ptr_game->State >= 2 ? str_playing_ : str_waiting_;
          ptr_state->fn_get_text = [str_label] { return str_label; };
          const core::Color color_state =
              GetStateColor(*ptr_game, *ptr_state, !b_can_join);
          ptr_state->fn_get_color = [color_state] { return color_state; };
        }

        auto* ptr_location =
            up_item->GetOrNull<ora::ui::LabelWidget>("LOCATION");
        if (ptr_location != nullptr) {
          auto& font = ora::ui::RequireWidgetFont(ptr_location->str_font);
          const std::string str_label = ora::ui::TruncateText(
              ptr_game->Location, ptr_location->bounds_widget.int4_width,
              font);
          ptr_location->fn_get_text = [str_label] { return str_label; };
          const core::Color color_fallback = color_incompatible_game_;
          ptr_location->fn_get_color =
              [ptr_location, b_can_join, color_fallback] {
                return b_can_join ? ptr_location->color_text
                                  : color_fallback;
              };
        }

        if (ptr_current_ != nullptr &&
            ptr_game->Address == ptr_current_->Address)
          ptr_next_server_row = up_item.get();

        vec_row_games_.push_back(ptr_game);
        vec_rows.push_back(std::move(up_item));
      }
    }
  }

  return vec_rows;
}

std::string ServerListLogic::GetStateLabel(
    const server::GameServerData* ptr_game) {
  if (ptr_game == nullptr)
    return std::string{};
  if (ptr_game->State == 2) {
    const std::int64_t int8_total_minutes =
        (static_cast<std::int64_t>(ptr_game->PlayTime) + 59) / 60;
    return transform_minutes_.Update(int8_total_minutes);
  }
  if (ptr_game->State == 1)
    return ptr_game->Protected ? str_password_protected_
                               : str_waiting_for_players_;
  if (ptr_game->State == 3)
    return str_server_shutting_down_;
  return str_unknown_server_state_;
}

core::Color ServerListLogic::GetStateColor(
    const server::GameServerData& game, ora::ui::LabelWidget& label_status,
    bool b_darkened) {
  if (!game.Protected && game.State == 1)
    return b_darkened ? color_incompatible_waiting_ : color_waiting_;
  if (game.Protected && game.State == 1)
    return b_darkened ? color_incompatible_protected_ : color_protected_;
  if (game.State >= 2)
    return b_darkened ? color_incompatible_game_started_
                      : color_game_started_;
  return label_status.color_text;
}

bool ServerListLogic::Filtered(const server::GameServerData& game) {
  const int int4_filters = ora::ui::Ui::Instance().MPGameFiltersGetDeps();
  if (game.State >= 2 && (int4_filters & kFilterStarted) == 0)
    return true;
  if (game.State == 1 && (int4_filters & kFilterWaiting) == 0 &&
      game.Players + game.Spectators != 0)
    return true;
  if (game.Players + game.Spectators == 0 &&
      (int4_filters & kFilterEmpty) == 0)
    return true;
  if (!game.IsCompatible && (int4_filters & kFilterIncompatible) == 0)
    return true;
  if (game.Protected && (int4_filters & kFilterProtected) == 0)
    return true;
  return false;
}

void ServerListLogic::ToggleFilterFlag(int int4_flag) {
  ora::ui::Ui::Instance().MPGameFiltersSetDeps(
      ora::ui::Ui::Instance().MPGameFiltersGetDeps() ^ int4_flag);
  ora::ui::Ui::Instance().SettingsSaveDeps();
  RefreshServerList();
}

MultiplayerLogic::MultiplayerLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    std::function<void()> fn_on_start, std::function<void()> fn_on_exit,
    net::ConnectionTarget* ptr_direct_connect_endpoint)
    : fn_on_start_(std::move(fn_on_start)),
      fn_on_exit_(std::move(fn_on_exit)) {
  up_server_list_logic_ = std::make_unique<ServerListLogic>(
      widget, mod_data,
      [this](server::GameServerData& server_game) {
        Join(server_game);
      });

  ptr_direct_connect_button_ =
      widget.Get<ora::ui::ButtonWidget>("DIRECTCONNECT_BUTTON");
  ptr_direct_connect_button_->fn_on_click = [this] {
    ora::ui::WidgetArgs args_args;
    args_args.Add("openLobby",
                  std::function<void()>{[this] { OpenLobby(); }});
    args_args.Add("onExit", std::function<void()>{[] {}});
    args_args.Add("directConnectEndPoint",
                  static_cast<net::ConnectionTarget*>(nullptr));
    ora::ui::Ui::Instance().OpenWindow("DIRECTCONNECT_PANEL",
                                       std::move(args_args));
  };

  ptr_create_server_button_ =
      widget.Get<ora::ui::ButtonWidget>("CREATE_BUTTON");
  ptr_create_server_button_->fn_on_click = [this] {
    ora::ui::WidgetArgs args_args;
    args_args.Add("openLobby",
                  std::function<void()>{[this] { OpenLobby(); }});
    args_args.Add("onExit", std::function<void()>{[] {}});
    ora::ui::Ui::Instance().OpenWindow("MULTIPLAYER_CREATESERVER_PANEL",
                                       std::move(args_args));
  };

  bool b_has_maps = false;
  if (auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps())
    for (const map::MapPreview* ptr_preview : ptr_cache->Previews())
      if ((static_cast<std::int32_t>(ptr_preview->Visibility()) &
           static_cast<std::int32_t>(map::MapVisibility::Shellmap)) == 0) {
        b_has_maps = true;
        break;
      }
  ptr_create_server_button_->b_disabled = !b_has_maps;

  ptr_back_button_ = widget.Get<ora::ui::ButtonWidget>("BACK_BUTTON");
  ptr_back_button_->fn_on_click = [this] {
    ora::ui::Ui::Instance().CloseWindow();
    if (fn_on_exit_)
      fn_on_exit_();
  };

  if (ptr_direct_connect_endpoint != nullptr) {
    widget.b_visible = false;
    const auto ptr_endpoint = std::make_shared<net::ConnectionTarget>(
        *ptr_direct_connect_endpoint);
    ora::ui::Ui::Instance().RunAfterTickDefer(
        [ptr_widget = &widget, ptr_endpoint, this] {
          ora::ui::WidgetArgs args_args;
          args_args.Add("openLobby",
                        std::function<void()>{[this] { OpenLobby(); }});
          args_args.Add("onExit", std::function<void()>{[] {}});
          args_args.Add("directConnectEndPoint", ptr_endpoint.get());
          ora::ui::Ui::Instance().OpenWindow("DIRECTCONNECT_PANEL",
                                             std::move(args_args));
          ptr_widget->b_visible = true;
        });
  }
}

MultiplayerLogic::~MultiplayerLogic() = default;

void MultiplayerLogic::OpenLobby() {
  ora::ui::Ui::Instance().CloseWindow();

  const auto ptr_on_lobby_exit =
      std::make_shared<std::function<void()>>([this] {
        ora::ui::WidgetArgs args_args;
        args_args.Add("onStart", std::function<void()>{fn_on_start_});
        args_args.Add("onExit", std::function<void()>{fn_on_exit_});
        args_args.Add("directConnectEndPoint",
                      static_cast<net::ConnectionTarget*>(nullptr));
        ora::ui::Ui::Instance().OpenWindow("MULTIPLAYER_PANEL",
                                           std::move(args_args));

        ora::ui::Ui::Instance().DisconnectDeps();
      });

  ora::ui::WidgetArgs args_args;
  args_args.Add("onStart", std::function<void()>{fn_on_start_});
  args_args.Add("onExit", std::function<void()>{
      [ptr_on_lobby_exit] { (*ptr_on_lobby_exit)(); }});
  args_args.Add("skirmishMode", false);
  ora::ui::Ui::Instance().OpenWindow("SERVER_LOBBY", std::move(args_args));
}

void MultiplayerLogic::Join(server::GameServerData& server_game) {
  if (!server_game.IsJoinable)
    return;

  const std::string& str_address = server_game.Address;
  const std::size_t sz_colon = str_address.find(':');
  const std::string str_host =
      sz_colon == std::string::npos
          ? str_address
          : str_address.substr(0, sz_colon);
  const std::string str_port =
      sz_colon == std::string::npos
          ? std::string{}
          : str_address.substr(sz_colon + 1);
  std::int32_t int4_port = 0;
  if (!meta::TryParseInt32Invariant(str_port, int4_port))
    return;

  const net::ConnectionTarget target_endpoint{str_host, int4_port};
  Connect(target_endpoint, "", [this] { OpenLobby(); }, [] {});
}

void RegisterServerListLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();

  registry_logics.Register(
      "ServerListLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        std::function<void(server::GameServerData&)> fn_on_join;
        if (const auto* ptr_value =
                std::get_if<std::function<void(server::GameServerData&)>>(
                    &args_args.At("onJoin")))
          fn_on_join = *ptr_value;

        return std::make_unique<ServerListLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::move(fn_on_join));
      });

  registry_logics.Register(
      "MultiplayerLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        std::function<void()> fn_on_start;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onStart")))
          fn_on_start = *ptr_value;

        std::function<void()> fn_on_exit;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onExit")))
          fn_on_exit = *ptr_value;

        net::ConnectionTarget* ptr_endpoint = nullptr;
        if (const auto* ptr_value = std::get_if<net::ConnectionTarget*>(
                &args_args.At("directConnectEndPoint")))
          ptr_endpoint = *ptr_value;

        return std::make_unique<MultiplayerLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::move(fn_on_start), std::move(fn_on_exit), ptr_endpoint);
      });
}

}  // namespace ora::mods
