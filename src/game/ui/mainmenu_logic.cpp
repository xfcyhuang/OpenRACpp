#include "game/ui/mainmenu_logic.hpp"

namespace ora::game::ui {

std::string_view menu_type_name(MenuType type) {
  switch (type) {
    case MenuType::kMain:
      return "Main";
    case MenuType::kSingleplayer:
      return "Singleplayer";
    case MenuType::kExtras:
      return "Extras";
    case MenuType::kMapEditor:
      return "MapEditor";
    case MenuType::kStartupPrompts:
      return "StartupPrompts";
    case MenuType::kNone:
      return "None";
  }
  return "None";
}

void main_menu_setup(MainMenuState& st) {
  st = MainMenuState{};
  st.type = MenuType::kStartupPrompts;
  // IntroductionPrompt/SystemInfoPrompt 版本已满足（首启提示跳过）+ FetchNews=False
  // 路径 → OnSysInfoComplete → SwitchMenu(Main)（MainMenuLogic.cs:246-275）
  main_menu_switch(st, MenuType::kMain);
}

void main_menu_switch(MainMenuState& st, MenuType type) {
  st.type = type;
  st.vec_nav.push_back(std::format("switch {}", menu_type_name(type)));
}

void main_menu_open_settings(MainMenuState& st) {
  main_menu_switch(st, MenuType::kNone);
  st.b_settings_open = true;
  st.vec_nav.push_back("open SETTINGS_PANEL");
}

void main_menu_close_settings(MainMenuState& st) {
  st.b_settings_open = false;
  main_menu_switch(st, MenuType::kMain);
}

void main_menu_apply(const MainMenuState& st, const MainMenuEnv& env, WidgetModel& root) {
  const auto set_visible{[&root](std::string_view str_id, bool b_visible) {
    if (WidgetModel* ptr_w{widget_find_by_id(root, str_id)}) ptr_w->b_visible = b_visible;
  }};
  set_visible("MAIN_MENU", st.type == MenuType::kMain);
  set_visible("SINGLEPLAYER_MENU", st.type == MenuType::kSingleplayer);
  set_visible("EXTRAS_MENU", st.type == MenuType::kExtras);
  set_visible("MAP_EDITOR_MENU", st.type == MenuType::kMapEditor);
  // NEWS_BG（:222）：FetchNews && menuType != None && != StartupPrompts
  set_visible("NEWS_BG", env.b_fetch_news && st.type != MenuType::kNone
                   && st.type != MenuType::kStartupPrompts);
  // UPDATE_NOTICE（:240-244）：!newsOpen && 菜单可见 && ModVersionStatus==Outdated——
  // M2 无版本检查面，恒不可见（偏离登记）。
  set_visible("UPDATE_NOTICE", false);
}

void main_menu_apply_disabled(const MainMenuEnv& env, WidgetModel& root) {
  const auto set_disabled{[&root](std::string_view str_id, bool b_disabled) {
    if (WidgetModel* ptr_w{widget_find_by_id(root, str_id)}) ptr_w->b_disabled = b_disabled;
  }};
  set_disabled("SKIRMISH_BUTTON", !env.b_has_maps);
  set_disabled("MISSIONS_BUTTON", !env.b_has_campaign && !env.b_has_missions);
  set_disabled("LOAD_BUTTON", !env.b_load_panel_enabled);
  set_disabled("CONTENT_BUTTON", !env.b_has_content_installer);
  set_disabled("LOAD_MAP_BUTTON", !env.b_has_maps);
}

namespace {

// 打开外部面板按钮的折叠记录：SwitchMenu(None) + open <PANEL>
void open_panel(MainMenuState& st, std::string_view str_panel) {
  main_menu_switch(st, MenuType::kNone);
  st.vec_nav.push_back(std::format("open {}", str_panel));
}

// BACK_BUTTON 语境由当前 menuType 决定（三个菜单同名按钮，可见性路由已保证唯一可达）
void back_click(MainMenuState& st) {
  switch (st.type) {
    case MenuType::kMapEditor:
      main_menu_switch(st, MenuType::kExtras);
      break;
    default:
      main_menu_switch(st, MenuType::kMain);
      break;
  }
}

}  // namespace

void main_menu_dispatch(MainMenuState& st, const MainMenuEnv& env, WidgetModel& root,
                        std::span<const UiEventRecord> vec_events) {
  static_cast<void>(env);
  static_cast<void>(root);
  for (const UiEventRecord& rec : vec_events) {
    if (rec.str_kind != "click") continue;
    if (rec.str_id == "SINGLEPLAYER_BUTTON") {
      main_menu_switch(st, MenuType::kSingleplayer);
    } else if (rec.str_id == "MULTIPLAYER_BUTTON") {
      open_panel(st, "MULTIPLAYER_PANEL");
    } else if (rec.str_id == "CONTENT_BUTTON") {
      // contentInstaller?.ManageContent（:83-89）——M2 无内容安装器，仅记录
      st.vec_nav.push_back("manage-content");
    } else if (rec.str_id == "SETTINGS_BUTTON") {
      main_menu_open_settings(st);
    } else if (rec.str_id == "EXTRAS_BUTTON") {
      main_menu_switch(st, MenuType::kExtras);
    } else if (rec.str_id == "QUIT_BUTTON") {
      st.b_quit = true;
      st.vec_nav.push_back("quit");
    } else if (rec.str_id == "SKIRMISH_BUTTON") {
      // StartSkirmishGame（:446-458）：SwitchMenu(None) + 本地服务器/大厅不在本票
      main_menu_switch(st, MenuType::kNone);
      st.vec_nav.push_back("start-skirmish");
    } else if (rec.str_id == "MISSIONS_BUTTON") {
      open_panel(st, "MISSIONBROWSER_PANEL");
    } else if (rec.str_id == "LOAD_BUTTON") {
      open_panel(st, "LOAD_GAME_BROWSER_PANEL");
    } else if (rec.str_id == "ENCYCLOPEDIA_BUTTON") {
      open_panel(st, "ENCYCLOPEDIA_PANEL");
    } else if (rec.str_id == "REPLAYS_BUTTON") {
      open_panel(st, "REPLAYBROWSER_PANEL");
    } else if (rec.str_id == "MUSIC_BUTTON") {
      open_panel(st, "MUSIC_PANEL");
    } else if (rec.str_id == "MAP_EDITOR_BUTTON") {
      main_menu_switch(st, MenuType::kMapEditor);
    } else if (rec.str_id == "ASSETBROWSER_BUTTON") {
      open_panel(st, "ASSETBROWSER_PANEL");
    } else if (rec.str_id == "CREDITS_BUTTON") {
      open_panel(st, "CREDITS_PANEL");
    } else if (rec.str_id == "NEW_MAP_BUTTON") {
      open_panel(st, "NEW_MAP_BG");
    } else if (rec.str_id == "LOAD_MAP_BUTTON") {
      open_panel(st, "MAPCHOOSER_PANEL");
    } else if (rec.str_id == "BACK_BUTTON") {
      back_click(st);
    }
  }
}

}  // namespace ora::game::ui
