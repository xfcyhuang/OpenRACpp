#pragma once

import std;

#include "game/ui/widget.hpp"

namespace ora::game::ui {

// 上游 MainMenuLogic.MenuType（MainMenuLogic.cs:39）。
enum class MenuType { kMain, kSingleplayer, kExtras, kMapEditor, kStartupPrompts, kNone };

[[nodiscard]] std::string_view menu_type_name(MenuType type);

// 上游 MapCache/Manifest 派生的按钮可用性输入（MainMenuLogic ctor :108-124/:83-89）。
struct MainMenuEnv {
  bool b_has_maps{true};            // MapCache.Any(Lobby) → SKIRMISH/LOAD_MAP
  bool b_has_campaign{};            // Manifest.Missions 非空 → MISSIONS
  bool b_has_missions{};            // MapCache MissionSelector Available → MISSIONS
  bool b_load_panel_enabled{};      // LoadGameBrowserLogic.IsLoadPanelEnabled → LOAD
  bool b_has_content_installer{};   // IFileSystemExternalContent → CONTENT
  bool b_fetch_news{true};          // Game.Settings.Game.FetchNews → NEWS_BG
};

// 菜单导航状态机：menuType + 退出/设置窗口开关 + 导航序列记录（D078 折叠回调面）。
struct MainMenuState {
  MenuType type{MenuType::kStartupPrompts};
  bool b_quit{};
  bool b_settings_open{};
  std::vector<std::string> vec_nav;
};

// 上游 ctor 尾部：StartupPrompts → prompt 全跳过（IntroductionPromptVersion 已满足）
// → OnSysInfoComplete → LoadAndDisplayNews（FetchNews=False 跳过）→ SwitchMenu(Main)。
void main_menu_setup(MainMenuState& st);

// 上游 SwitchMenu（:58-66）：menuType 赋值（Discord/tooltip 副作用不在最小面）。
void main_menu_switch(MainMenuState& st, MenuType type);

// SETTINGS_BUTTON OnClick（:91-98）：SwitchMenu(None) + 打开 SETTINGS_PANEL 窗口。
void main_menu_open_settings(MainMenuState& st);

// SETTINGS_PANEL onExit 回调：SwitchMenu(Main)。
void main_menu_close_settings(MainMenuState& st);

// IsVisible lambda 折叠：四菜单 + NEWS_BG（FetchNews && 菜单可见）+ UPDATE_NOTICE
// （M2 无 WebServices 版本检查面，恒不可见——偏离登记）。
void main_menu_apply(const MainMenuState& st, const MainMenuEnv& env, WidgetModel& root);

// 按钮 Disabled 折叠：SKIRMISH=!hasMaps、MISSIONS=!hasCampaign&&!hasMissions、
// LOAD=!loadPanelEnabled、CONTENT=!hasContentInstaller、LOAD_MAP=!hasMaps。
void main_menu_apply_disabled(const MainMenuEnv& env, WidgetModel& root);

// 消费本帧新增 click 事件记录并按按钮 id 分发（D078 折叠模式；面板打开类只记
// 导航序列，面板本体不在本票）。
void main_menu_dispatch(MainMenuState& st, const MainMenuEnv& env, WidgetModel& root,
                        std::span<const UiEventRecord> vec_events);

}  // namespace ora::game::ui
