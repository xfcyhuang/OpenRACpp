// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/MainMenuLogic.cs @1f594e320
//          L25-566 全文(实现部分)。
#include "mods/main_menu_logic.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/mod_data.hpp"
#include "map/map_cache.hpp"
#include "core/mersenne_twister.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

namespace {

std::string Fluent(std::string_view str_key) {
  return fluent::FluentProvider::Instance().GetMessage(str_key);
}

std::string Fluent1(std::string_view str_key, const std::string& str_arg_name,
                    const std::string& str_arg_value) {
  const std::vector<std::pair<std::string, fluent::FluentValue>> vec_args{
      {str_arg_name, fluent::FluentValue{str_arg_value}}};
  return fluent::FluentProvider::Instance().GetMessage(str_key, vec_args);
}

}  // namespace

bool MainMenuLogic::b_fetched_news_ = false;
MainMenuLogic::MenuPanel MainMenuLogic::kind_last_game_state_ =
    MainMenuLogic::MenuPanel::None;

MainMenuLogic::MainMenuLogic(ora::ui::Widget& widget, sim::World& world,
                             game::ModData& mod_data)
    : ptr_root_menu_{&widget},
      ptr_mod_data_{&mod_data},
      up_web_services_{std::make_unique<game::WebServices>()} {
  ora::ui::Widget* ptr_main_menu = widget.Get("MAIN_MENU");
  ptr_menu_ = ptr_main_menu;
  ptr_main_menu->fn_is_visible = [this] {
    return kind_menu_type_ == MenuType::Main;
  };

  ptr_main_menu->Get<ora::ui::ButtonWidget>("SINGLEPLAYER_BUTTON")
      ->fn_on_click = [this] { SwitchMenu(MenuType::Singleplayer); };

  ptr_main_menu->Get<ora::ui::ButtonWidget>("MULTIPLAYER_BUTTON")
      ->fn_on_click = [this] { OpenMultiplayerPanel(); };

  if (auto* ptr_content_button =
          ptr_main_menu->GetOrNull<ora::ui::ButtonWidget>(
              "CONTENT_BUTTON")) {
    ptr_content_button->b_disabled = true;
  }

  ptr_main_menu->Get<ora::ui::ButtonWidget>("SETTINGS_BUTTON")->fn_on_click =
      [this] {
        SwitchMenu(MenuType::None);
        ora::ui::WidgetArgs args_panel;
        args_panel = args_panel.With(
            "onExit",
            std::function<void()>{[this] { SwitchMenu(MenuType::Main); }});
        ora::ui::Ui::Instance().OpenWindow("SETTINGS_PANEL",
                                           std::move(args_panel));
      };

  ptr_main_menu->Get<ora::ui::ButtonWidget>("EXTRAS_BUTTON")->fn_on_click =
      [this] { SwitchMenu(MenuType::Extras); };

  ptr_main_menu->Get<ora::ui::ButtonWidget>("QUIT_BUTTON")->fn_on_click = [] {
    ora::ui::Ui::Instance().ExitDeps();
  };

  ora::ui::Widget* ptr_singleplayer_menu = widget.Get("SINGLEPLAYER_MENU");
  ptr_singleplayer_menu->fn_is_visible = [this] {
    return kind_menu_type_ == MenuType::Singleplayer;
  };

  auto* ptr_missions_button =
      ptr_singleplayer_menu->Get<ora::ui::ButtonWidget>("MISSIONS_BUTTON");
  ptr_missions_button->fn_on_click = [this] {
    OpenMissionBrowserPanel(
        MapCache().PickLastModifiedMap(
            map::MapVisibility::MissionSelector));
  };

  const bool b_has_campaign =
      !ptr_mod_data_->ManifestRef().Missions().empty();
  bool b_has_missions = false;
  for (const map::MapPreview* ptr_preview :
       MapCache().Previews()) {
    if (ptr_preview->Status() == map::MapStatus::Available &&
        (static_cast<std::int32_t>(ptr_preview->Visibility()) &
         static_cast<std::int32_t>(map::MapVisibility::MissionSelector)) != 0) {
      b_has_missions = true;
      break;
    }
  }

  ptr_missions_button->b_disabled = !b_has_campaign && !b_has_missions;

  bool b_has_maps = false;
  for (const map::MapPreview* ptr_preview :
       MapCache().Previews()) {
    if ((static_cast<std::int32_t>(ptr_preview->Visibility()) &
         static_cast<std::int32_t>(map::MapVisibility::Lobby)) != 0) {
      b_has_maps = true;
      break;
    }
  }

  auto* ptr_skirmish_button =
      ptr_singleplayer_menu->Get<ora::ui::ButtonWidget>("SKIRMISH_BUTTON");
  ptr_skirmish_button->fn_on_click = [this] { StartSkirmishGame(); };
  ptr_skirmish_button->b_disabled = !b_has_maps;

  auto* ptr_load_button =
      ptr_singleplayer_menu->Get<ora::ui::ButtonWidget>("LOAD_BUTTON");
  ptr_load_button->fn_is_disabled = [this] {
    const std::string str_base_save_path =
        std::string{"Saves"} + "/" + ptr_mod_data_->ManifestRef().Id() + "/" +
        ptr_mod_data_->ManifestRef().Metadata().str_version;
    std::error_code ec{};
    if (!std::filesystem::exists(str_base_save_path, ec))
      return true;
    for (const auto& entry :
         std::filesystem::directory_iterator{str_base_save_path, ec})
      if (entry.is_regular_file() && entry.path().extension() == ".orasav")
        return false;
    return true;
  };
  ptr_load_button->fn_on_click = [this] { OpenGameSaveBrowserPanel(); };

  if (auto* ptr_encyclopedia_button =
          ptr_singleplayer_menu->GetOrNull<ora::ui::ButtonWidget>(
              "ENCYCLOPEDIA_BUTTON"))
    ptr_encyclopedia_button->fn_on_click = [this] { OpenEncyclopediaPanel(); };

  ptr_singleplayer_menu->Get<ora::ui::ButtonWidget>("BACK_BUTTON")
      ->fn_on_click = [this] { SwitchMenu(MenuType::Main); };

  ora::ui::Widget* ptr_extras_menu = widget.Get("EXTRAS_MENU");
  ptr_extras_menu->fn_is_visible = [this] {
    return kind_menu_type_ == MenuType::Extras;
  };

  ptr_extras_menu->Get<ora::ui::ButtonWidget>("REPLAYS_BUTTON")
      ->fn_on_click = [this] { OpenReplayBrowserPanel(); };

  ptr_extras_menu->Get<ora::ui::ButtonWidget>("MUSIC_BUTTON")->fn_on_click =
      [this, &world] {
        SwitchMenu(MenuType::None);
        ora::ui::WidgetArgs args_panel;
        args_panel = args_panel.With(
            "onExit",
            std::function<void()>{[this] { SwitchMenu(MenuType::Extras); }});
        args_panel = args_panel.With("world", &world);
        ora::ui::Ui::Instance().OpenWindow("MUSIC_PANEL",
                                           std::move(args_panel));
      };

  ptr_extras_menu->Get<ora::ui::ButtonWidget>("MAP_EDITOR_BUTTON")
      ->fn_on_click = [this] { SwitchMenu(MenuType::MapEditor); };

  if (auto* ptr_asset_browser_button =
          ptr_extras_menu->GetOrNull<ora::ui::ButtonWidget>(
              "ASSETBROWSER_BUTTON"))
    ptr_asset_browser_button->fn_on_click = [this] {
      SwitchMenu(MenuType::None);
      ora::ui::WidgetArgs args_panel;
      args_panel = args_panel.With(
          "onExit",
          std::function<void()>{[this] { SwitchMenu(MenuType::Extras); }});
      ora::ui::Ui::Instance().OpenWindow("ASSETBROWSER_PANEL",
                                         std::move(args_panel));
    };

  ptr_extras_menu->Get<ora::ui::ButtonWidget>("CREDITS_BUTTON")->fn_on_click =
      [this] {
        SwitchMenu(MenuType::None);
        ora::ui::WidgetArgs args_panel;
        args_panel = args_panel.With(
            "onExit",
            std::function<void()>{[this] { SwitchMenu(MenuType::Extras); }});
        ora::ui::Ui::Instance().OpenWindow("CREDITS_PANEL",
                                           std::move(args_panel));
      };

  ptr_extras_menu->Get<ora::ui::ButtonWidget>("BACK_BUTTON")->fn_on_click =
      [this] { SwitchMenu(MenuType::Main); };

  ora::ui::Widget* ptr_map_editor_menu = widget.Get("MAP_EDITOR_MENU");
  ptr_map_editor_menu->fn_is_visible = [this] {
    return kind_menu_type_ == MenuType::MapEditor;
  };

  auto* ptr_new_map_button =
      widget.Get<ora::ui::ButtonWidget>("NEW_MAP_BUTTON");
  ptr_new_map_button->fn_on_click = [this] {
    SwitchMenu(MenuType::None);
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With(
        "onExit",
        std::function<void()>{[this] { SwitchMenu(MenuType::MapEditor); }});
    ora::ui::Ui::Instance().OpenWindow("NEW_MAP_BG", std::move(args_panel));
  };

  auto* ptr_load_map_button =
      widget.Get<ora::ui::ButtonWidget>("LOAD_MAP_BUTTON");
  ptr_load_map_button->fn_on_click = [this] {
    SwitchMenu(MenuType::None);
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With("initialMap", std::string{});
    ora::ui::Ui::Instance().OpenWindow("MAPCHOOSER_PANEL",
                                       std::move(args_panel));
  };

  ptr_load_map_button->b_disabled = !b_has_maps;

  ptr_map_editor_menu->Get<ora::ui::ButtonWidget>("BACK_BUTTON")
      ->fn_on_click = [this] { SwitchMenu(MenuType::Extras); };

  ora::ui::Widget* ptr_news_bg = widget.GetOrNull("NEWS_BG");
  if (ptr_news_bg != nullptr) {
    ptr_news_bg_ = ptr_news_bg;
    ptr_news_bg->fn_is_visible = [this] {
      return ora::ui::Ui::Instance().SettingsFetchNewsDeps() &&
             kind_menu_type_ != MenuType::None &&
             kind_menu_type_ != MenuType::StartupPrompts;
    };

    BuildNewsPanel();
    SetNewsStatus(Fluent("label-loading-news"));
  }

  if (ora::ui::Ui::Instance().CheckVersionDeps())
    up_web_services_->CheckModVersion();

  if (auto* ptr_update_label = ptr_root_menu_->GetOrNull("UPDATE_NOTICE"))
    ptr_update_label->fn_is_visible = [this] {
      return !b_news_open_ && kind_menu_type_ != MenuType::None &&
             kind_menu_type_ != MenuType::StartupPrompts &&
             up_web_services_->ModVersionStatus_() ==
                 game::ModVersionStatus::Outdated;
    };

  kind_menu_type_ = MenuType::StartupPrompts;

  auto fn_on_sys_info_complete =
      std::make_shared<std::function<void()>>();
  *fn_on_sys_info_complete = [this, ptr_news_bg] {
    LoadAndDisplayNews(ptr_news_bg);
    SwitchMenu(MenuType::Main);
  };

  auto fn_on_introduction_complete =
      std::make_shared<std::function<void()>>();
  *fn_on_introduction_complete = [this, fn_on_sys_info_complete] {
    if (ora::ui::Ui::Instance().ShouldShowSystemInfoPromptDeps()) {
      ora::ui::WidgetArgs args_panel;
      args_panel = args_panel.With("onComplete", *fn_on_sys_info_complete);
      ora::ui::Ui::Instance().OpenWindow("MAINMENU_SYSTEM_INFO_PROMPT",
                                         std::move(args_panel));
    } else {
      (*fn_on_sys_info_complete)();
    }
  };

  if (ora::ui::Ui::Instance().ShouldShowIntroductionPromptDeps()) {
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With("onComplete", *fn_on_introduction_complete);
    ora::ui::Ui::Instance().OpenWindow("MAINMENU_INTRODUCTION_PROMPT",
                                       std::move(args_panel));
  } else {
    (*fn_on_introduction_complete)();
  }

  fn_on_shellmap_loaded_unsubscribe_ =
      ora::ui::Ui::Instance().SubscribeOnShellmapLoaded(
          [this] { OpenMenuBasedOnLastGame(); });
  fn_before_game_start_unsubscribe_ =
      ora::ui::Ui::Instance().SubscribeBeforeGameStart(
          [this] { RemoveShellmapUI(); });
  fn_on_remote_direct_connect_unsubscribe_ =
      ora::ui::Ui::Instance().SubscribeOnRemoteDirectConnect(
          [this](const net::ConnectionTarget& endpoint) {
            OnRemoteDirectConnect(endpoint);
          });
}

MainMenuLogic::~MainMenuLogic() {
  if (fn_on_shellmap_loaded_unsubscribe_)
    fn_on_shellmap_loaded_unsubscribe_();
  if (fn_before_game_start_unsubscribe_)
    fn_before_game_start_unsubscribe_();
  if (fn_on_remote_direct_connect_unsubscribe_)
    fn_on_remote_direct_connect_unsubscribe_();
}

void MainMenuLogic::SwitchMenu(MenuType type) {
  kind_menu_type_ = type;
  ora::ui::Ui::Instance().RunAfterTickDefer(
      [] { ora::ui::Ui::Instance().ResetTooltips(); });
}

void MainMenuLogic::BuildNewsPanel() {
  ora::ui::Widget* ptr_loaded =
      ora::ui::Ui::Instance().LoadWidget("NEWS_PANEL", nullptr, {});
  up_news_panel_owner_ = ora::ui::Ui::Instance().ReleaseWidget(ptr_loaded);
  ptr_news_panel_ = dynamic_cast<ora::ui::ScrollPanelWidget*>(ptr_loaded);
  up_news_template_ = ptr_news_panel_->RemoveChild(
      *ptr_news_panel_->Get("NEWS_ITEM_TEMPLATE"));
  int4_max_news_height_ = ptr_news_panel_->bounds_widget.int4_height;
  ptr_news_status_ = ptr_news_panel_->Get<ora::ui::LabelWidget>("NEWS_STATUS");
}

void MainMenuLogic::LoadAndDisplayNews(ora::ui::Widget* ptr_news_bg) {
  if (ptr_news_bg == nullptr ||
      !ora::ui::Ui::Instance().SettingsFetchNewsDeps())
    return;

  const std::string str_cache_file =
      up_web_services_->GameNewsFileName;
  std::vector<NewsItem> vec_current = ParseNews(str_cache_file);
  if (!vec_current.empty()) {
    vec_news_cache_ = vec_current;
    DisplayNews(vec_news_cache_);
  }

  if (auto* ptr_news_button =
          ptr_news_bg->GetOrNull<ora::ui::DropDownButtonWidget>(
              "NEWS_BUTTON")) {
    if (!b_fetched_news_) {
      b_fetched_news_ = true;
      const std::string str_url = game::BuildHttpQuery(
          up_web_services_->GameNews,
          {{"version", "dev"},
           {"mod", ptr_mod_data_->ManifestRef().Id()},
           {"modversion", ptr_mod_data_->ManifestRef().Metadata().str_version}});
      const std::string str_url_full =
          str_url + ora::ui::Ui::Instance().CreateParameterStringDeps();
      ora::ui::Ui::Instance().FetchNewsDeps(
          str_url_full, str_cache_file, [this, str_cache_file,
                                         vec_current]() mutable {
            const std::vector<NewsItem> vec_new = ParseNews(str_cache_file);
            if (vec_new.empty())
              return;

            vec_news_cache_ = vec_new;
            if (ptr_news_panel_ != nullptr)
              DisplayNews(vec_news_cache_);

            if (vec_current.empty()) {
              if (auto* ptr_button =
                      ptr_news_bg_->GetOrNull<ora::ui::DropDownButtonWidget>(
                          "NEWS_BUTTON"))
                OpenNewsPanel(*ptr_button);
              return;
            }

            for (const NewsItem& item_new : vec_new) {
              bool b_found = false;
              for (const NewsItem& item_current : vec_current)
                if (item_current.str_date_time == item_new.str_date_time) {
                  b_found = true;
                  break;
                }
              if (!b_found) {
                if (auto* ptr_button =
                        ptr_news_bg_->GetOrNull<
                            ora::ui::DropDownButtonWidget>("NEWS_BUTTON"))
                  OpenNewsPanel(*ptr_button);
                return;
              }
            }
          });
    }

    ptr_news_button->fn_on_click = [this, ptr_news_button] {
      OpenNewsPanel(*ptr_news_button);
    };
  }
}

void MainMenuLogic::OpenNewsPanel(
    ora::ui::DropDownButtonWidget& button) {
  b_news_open_ = true;
  if (ptr_news_panel_ == nullptr) {
    BuildNewsPanel();
    DisplayNews(vec_news_cache_);
  }
  button.AttachPanel(std::move(up_news_panel_owner_),
                     [this] { b_news_open_ = false; });
  ptr_news_panel_ = nullptr;
}

void MainMenuLogic::OnRemoteDirectConnect(
    const net::ConnectionTarget&) {
  SwitchMenu(MenuType::None);
  ora::ui::WidgetArgs args_panel;
  args_panel = args_panel.With("onStart",
                               std::function<void()>{[this] {
                                 RemoveShellmapUI();
                               }});
  args_panel = args_panel.With(
      "onExit", std::function<void()>{[this] { SwitchMenu(MenuType::Main); }});
  ora::ui::Ui::Instance().OpenWindow("MULTIPLAYER_PANEL",
                                     std::move(args_panel));
}

void MainMenuLogic::SetNewsStatus(const std::string& str_message) {
  if (ptr_news_status_ != nullptr)
    ptr_news_status_->fn_get_text = [str_message] { return str_message; };
}

map::MapCache& MainMenuLogic::MapCache() {
  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache == nullptr)
    throw std::runtime_error("MainMenuLogic: no map cache wired");
  return *ptr_cache;
}

void MainMenuLogic::LoadMapIntoEditor(const std::string& str_uid) {
  ora::ui::Ui::Instance().LoadEditorDeps(str_uid);
  kind_last_game_state_ = MenuPanel::MapEditor;
}

std::vector<MainMenuLogic::NewsItem> MainMenuLogic::ParseNews(
    const std::string& str_path) {
  std::vector<NewsItem> vec_items;
  std::error_code ec{};
  if (!std::filesystem::exists(str_path, ec))
    return vec_items;

  const auto vec_nodes = yaml::MiniYaml::FromFile(str_path);
  for (const auto& node : vec_nodes) {
    NewsItem item{};
    for (const auto& child : node.Value.Nodes) {
      const std::string_view str_child_key =
          child.Key != nullptr ? std::string_view{*child.Key}
                               : std::string_view{};
      if (str_child_key == "Title" && child.Value.Value != nullptr)
        item.str_title = *child.Value.Value;
      else if (str_child_key == "Author" && child.Value.Value != nullptr)
        item.str_author = *child.Value.Value;
      else if (str_child_key == "Content" && child.Value.Value != nullptr)
        item.str_content = *child.Value.Value;
    }
    item.str_date_time =
        node.Key != nullptr ? std::string{*node.Key} : std::string{};
    vec_items.push_back(std::move(item));
  }
  return vec_items;
}

void MainMenuLogic::DisplayNews(const std::vector<NewsItem>& vec_news_items) {
  if (ptr_news_panel_ == nullptr)
    return;

  ptr_news_panel_->RemoveChildren();
  SetNewsStatus("");

  for (const NewsItem& item : vec_news_items) {
    std::unique_ptr<ora::ui::Widget> up_news_item = up_news_template_->Clone();

    auto* ptr_title_label =
        up_news_item->Get<ora::ui::LabelWidget>("TITLE");
    ptr_title_label->fn_get_text = [str = item.str_title] { return str; };

    auto* ptr_author_datetime_label =
        up_news_item->Get<ora::ui::LabelWidget>("AUTHOR_DATETIME");
    const std::string str_author_datetime = Fluent1(
        "label-author-datetime", "datetime", item.str_date_time);
    const std::string str_author = item.str_author;
    ptr_author_datetime_label->fn_get_text =
        [str_author, str_author_datetime]() {
          return str_author_datetime;
        };

    auto* ptr_content_label =
        up_news_item->Get<ora::ui::LabelWidget>("CONTENT");
    std::string str_content = item.str_content;
    const std::size_t pos_escape = str_content.find("\\n");
    if (pos_escape != std::string::npos) {
      std::string str_replaced;
      str_replaced.reserve(str_content.size());
      for (std::size_t i = 0; i < str_content.size(); ++i) {
        if (i + 1 < str_content.size() && str_content[i] == '\\' &&
            str_content[i + 1] == 'n') {
          str_replaced.push_back('\n');
          ++i;
        } else {
          str_replaced.push_back(str_content[i]);
        }
      }
      str_content = std::move(str_replaced);
    }
    ptr_content_label->fn_get_text = [str_content] { return str_content; };

    ptr_news_panel_->AddChild(std::move(up_news_item));
    if (ptr_news_panel_->ptr_layout != nullptr)
      ptr_news_panel_->ptr_layout->AdjustChildren();
    ptr_news_panel_->bounds_widget.int4_height =
        std::min(ptr_news_panel_->int4_content_height, int4_max_news_height_);
  }
}

void MainMenuLogic::RemoveShellmapUI() {
  if (ptr_root_menu_->ptr_parent != nullptr)
    ptr_root_menu_->ptr_parent->RemoveChild(*ptr_root_menu_);
}

void MainMenuLogic::StartSkirmishGame() {
  SwitchMenu(MenuType::None);

  std::string str_initial =
      MapCache().PickLastModifiedMap(
          map::MapVisibility::Lobby);
  if (str_initial.empty())
    str_initial = ora::ui::Ui::Instance().ServerMapGetDeps();

  MersenneTwister random{0};
  const std::string str_map =
      MapCache().ChooseInitialMap(str_initial,
                                                            random);
  ora::ui::Ui::Instance().ServerMapSetDeps(str_map);
  ora::ui::Ui::Instance().SettingsSaveDeps();

  const std::string str_endpoint =
      ora::ui::Ui::Instance().CreateLocalServerDeps(str_map, true);
  ora::ui::Ui::Instance().ConnectionConnectDeps(
      str_endpoint, "",
      [this] { OpenSkirmishLobbyPanel(); },
      [this] {
        ora::ui::Ui::Instance().CloseServerDeps();
        SwitchMenu(MenuType::Main);
      });
}

void MainMenuLogic::OpenMissionBrowserPanel(const std::string& str_map) {
  SwitchMenu(MenuType::None);
  ora::ui::WidgetArgs args_panel;
  args_panel = args_panel.With(
      "onExit", std::function<void()>{[this] {
        ora::ui::Ui::Instance().DisconnectDeps();
        SwitchMenu(MenuType::Singleplayer);
      }});
  args_panel = args_panel.With("onStart",
                               std::function<void()>{[this] {
                                 RemoveShellmapUI();
                                 kind_last_game_state_ = MenuPanel::Missions;
                               }});
  args_panel = args_panel.With("initialMap", str_map);
  ora::ui::Ui::Instance().OpenWindow("MISSIONBROWSER_PANEL",
                                     std::move(args_panel));
}

void MainMenuLogic::OpenEncyclopediaPanel() {
  SwitchMenu(MenuType::None);
  ora::ui::WidgetArgs args_panel;
  args_panel = args_panel.With(
      "onExit",
      std::function<void()>{[this] { SwitchMenu(MenuType::Singleplayer); }});
  ora::ui::Ui::Instance().OpenWindow("ENCYCLOPEDIA_PANEL",
                                     std::move(args_panel));
}

void MainMenuLogic::OpenSkirmishLobbyPanel() {
  SwitchMenu(MenuType::None);
  ora::ui::WidgetArgs args_panel;
  args_panel = args_panel.With(
      "onExit", std::function<void()>{[this] {
        ora::ui::Ui::Instance().DisconnectDeps();
        SwitchMenu(MenuType::Singleplayer);
      }});
  args_panel = args_panel.With("onStart",
                               std::function<void()>{[this] {
                                 RemoveShellmapUI();
                                 kind_last_game_state_ = MenuPanel::Skirmish;
                               }});
  args_panel = args_panel.With("skirmishMode", true);
  ora::ui::Ui::Instance().OpenWindow("SERVER_LOBBY", std::move(args_panel));
}

void MainMenuLogic::OpenMultiplayerPanel() {
  SwitchMenu(MenuType::None);
  ora::ui::WidgetArgs args_panel;
  args_panel = args_panel.With("onStart",
                               std::function<void()>{[this] {
                                 RemoveShellmapUI();
                                 kind_last_game_state_ = MenuPanel::Multiplayer;
                               }});
  args_panel = args_panel.With(
      "onExit", std::function<void()>{[this] { SwitchMenu(MenuType::Main); }});
  ora::ui::Ui::Instance().OpenWindow("MULTIPLAYER_PANEL",
                                     std::move(args_panel));
}

void MainMenuLogic::OpenReplayBrowserPanel() {
  SwitchMenu(MenuType::None);
  ora::ui::WidgetArgs args_panel;
  args_panel = args_panel.With(
      "onExit",
      std::function<void()>{[this] { SwitchMenu(MenuType::Extras); }});
  args_panel = args_panel.With("onStart",
                               std::function<void()>{[this] {
                                 RemoveShellmapUI();
                                 kind_last_game_state_ = MenuPanel::Replays;
                               }});
  ora::ui::Ui::Instance().OpenWindow("REPLAYBROWSER_PANEL",
                                     std::move(args_panel));
}

void MainMenuLogic::OpenGameSaveBrowserPanel() {
  SwitchMenu(MenuType::None);
  ora::ui::WidgetArgs args_panel;
  args_panel = args_panel.With(
      "onExit",
      std::function<void()>{[this] { SwitchMenu(MenuType::Singleplayer); }});
  args_panel = args_panel.With("onStart",
                               std::function<void()>{[this] {
                                 RemoveShellmapUI();
                                 kind_last_game_state_ = MenuPanel::GameSaves;
                               }});
  ora::ui::Ui::Instance().OpenWindow("LOAD_GAME_BROWSER_PANEL",
                                     std::move(args_panel));
}

void MainMenuLogic::OpenMenuBasedOnLastGame() {
  switch (kind_last_game_state_) {
    case MenuPanel::Missions:
      OpenMissionBrowserPanel("");
      break;
    case MenuPanel::Replays:
      OpenReplayBrowserPanel();
      break;
    case MenuPanel::Skirmish:
      StartSkirmishGame();
      break;
    case MenuPanel::Multiplayer:
      OpenMultiplayerPanel();
      break;
    case MenuPanel::MapEditor:
      SwitchMenu(MenuType::MapEditor);
      break;
    case MenuPanel::GameSaves:
      SwitchMenu(MenuType::Singleplayer);
      break;
    case MenuPanel::None:
      break;
  }

  kind_last_game_state_ = MenuPanel::None;
}

void RegisterMainMenuLogic() {
  ora::ui::ChromeLogicRegistry::Instance().Register(
      "MainMenuLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<MainMenuLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")),
            *std::get<game::ModData*>(args_args.At("modData")));
      });
}

}  // namespace ora::mods
