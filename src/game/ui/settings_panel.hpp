#pragma once

import std;

#include "game/input/hotkey.hpp"
#include "game/input/hotkey_registry.hpp"
#include "game/miniyaml.hpp"
#include "game/settings.hpp"
#include "game/sound/sound.hpp"
#include "game/ui/mainmenu_logic.hpp"
#include "game/ui/widget.hpp"
#include "game/ui/widget_loader.hpp"

namespace ora::game::ui {

// 上游 settings.yaml `Panels:` 注册序（GAMEPLAY/INPUT/DISPLAY/AUDIO/HOTKEYS/ADVANCED）
// 的本票子集：显示/音频/输入（改键）三页，相对序保持。
enum class SettingsPanelId { kDisplay, kAudio, kHotkeys };

[[nodiscard]] std::string_view settings_panel_id_name(SettingsPanelId id);

// 上游 HotkeysSettingsLogic.BindHotkeyPref 行面（HotkeysSettingsLogic.cs:72-112）。
struct HotkeyRow {
  std::string str_name;
  std::string str_key;      // 当前绑定（hotkey_to_string）
  std::string str_default;  // 默认绑定
  bool b_readonly{};
  bool b_has_duplicates{};
};

// SettingsLogic + 三页 Logic 的状态折叠：activePanel/needsRestart/原文快照
// （static originalGraphicSettings/originalSoundSettings 首开快照语义）+ 控件文本态。
struct SettingsPanelState {
  SettingsPanelId active{SettingsPanelId::kDisplay};
  bool b_needs_restart{};
  WidgetModel* ptr_window{};  // SETTINGS_PANEL 树（open 后有效；可见性/高亮折叠挂此）
  SettingsModel* ptr_settings{};
  input::HotkeyRegistryModel* ptr_hotkeys{};
  sound_facade* ptr_sound{};  // 可空：无音频引擎环境（音量只写设置）
  std::string str_settings_path;
  GraphicSettingsModel graphics_original{};
  SoundSettingsModel sound_original{};
  std::string str_window_width_text;   // WINDOW_WIDTH TextField.Text 等价
  std::string str_window_height_text;  // WINDOW_HEIGHT TextField.Text 等价
  std::string str_mode_text;           // MODE_DROPDOWN GetText 折叠（fluent key 原文）
  std::optional<std::string> opt_selected_hotkey;  // 选中待改键行（上游 selectedHotkeyDefinition）
  std::vector<std::string> vec_events;
};

// SettingsLogic ctor 等价：SETTINGS_PANEL 装载 + PANEL_TEMPLATE 三克隆（Id=面板名，
// 模板移除）+ 三页 chrome 装载进克隆 + BUTTON_TEMPLATE 克隆按 ButtonStride(0,35)
// 布局 + activePanel=首个注册 + 三页控件初值从 settings 读入。
[[nodiscard]] std::expected<void, std::string> settings_panel_open(
    SettingsPanelState& st, WidgetModel& root, const WidgetLoaderModel& loader_settings,
    const WidgetLoaderModel& loader_display, const WidgetLoaderModel& loader_audio,
    const WidgetLoaderModel& loader_hotkeys, const WidgetInitCtx& ctx,
    std::string_view str_settings_path);

// BACK_BUTTON（SettingsLogic.cs:113-143）：leave(active)→needsRestart |=、Save、
// 关窗 + onExit（菜单回 Main）。重启/保存确认对话框不在最小面（偏离登记）。
void settings_panel_close(SettingsPanelState& st, MainMenuState* ptr_menu);

// leavePanelActions[id]()（DisplaySettingsLogic.InitPanel 尾 lambda /
// AudioSettingsLogic 同形 / HotkeysSettingsLogic 恒 false）：写回字段并返回
// needsRestart 增量。
[[nodiscard]] bool settings_panel_leave(SettingsPanelState& st, SettingsPanelId id);

// Game.Settings.Save() 等价（T21 settings_save；热键 Keys 段从注册表 vec_settings
// 同步进模型）。
void settings_panel_save(SettingsPanelState& st);

// tab.OnClick（SettingsLogic.cs:190-195）：needsRestart |= leave(active)、Save、
// activePanel = panelID。
void settings_panel_tab_click(SettingsPanelState& st, SettingsPanelId id);

// RESET_BUTTON → resetPanelActions[id]()+Save（确认对话框直执行——偏离登记）。
void settings_panel_reset(SettingsPanelState& st, SettingsPanelId id);

// —— 显示页控件（DisplaySettingsLogic）——
// MODE_DROPDOWN 选项选择（ShowWindowModeDropdown：fullscreen/legacy-fullscreen/windowed）。
void settings_panel_set_window_mode(SettingsPanelState& st, WindowMode mode);
// WINDOW_WIDTH/WINDOW_HEIGHT 文本（int.TryParse 在 leave 时发生，非法文本 leave 时取 0）。
void settings_panel_set_window_text(SettingsPanelState& st, bool b_width,
                                    std::string_view str_text);
// VSYNC_CHECKBOX（BindCheckboxPref：OnClick 翻转）。
void settings_panel_toggle_vsync(SettingsPanelState& st);

// —— 音频页控件（AudioSettingsLogic）——
// 三通道滑条 OnChange（idx：0=Sound 1=Music 2=Video）：设置字段 + 引擎实时音量。
void settings_panel_set_volume(SettingsPanelState& st, std::size_t sz_channel, float fp4_value);
// MUTE_SOUND：Mute 翻转 + MuteAudio/UnmuteAudio（引擎主音量 0/1）。
void settings_panel_toggle_mute(SettingsPanelState& st);

// —— 输入页（HotkeysSettingsLogic）——
// 改键：hotkey_set + SaveHotkey（注册表 vec_settings → settings Keys 段）。
[[nodiscard]] bool settings_panel_rekey(SettingsPanelState& st, std::string_view str_name,
                                        const input::HotkeyModel& hk);
// 改键列表行（定义装载序；key=当前值、default、Readonly、HasDuplicates）。
[[nodiscard]] std::vector<HotkeyRow> settings_panel_hotkey_rows(
    const input::HotkeyRegistryModel& reg);

// 消费新增 UI 事件（tab/BACK/RESET 按钮 click、音频滑条 change）驱动面板状态机。
void settings_panel_dispatch(SettingsPanelState& st, std::span<const UiEventRecord> vec_events);

// 上游 ListLayout.AdjustChildren（Mods.Common/Widgets/ListLayout.cs:32-45）最小折叠：
// child.Y = ContentHeight（CollapseHiddenChildren 且不可见不计高），返回 ContentHeight
// 尾差（TopBottomSpacing - ItemSpacing）已计入。chrome 字段缺省 = 上游 ScrollPanelWidget
// 默认（TopBottomSpacing=2, ItemSpacing=0）。
[[nodiscard]] int settings_panel_scrollpanel_layout(WidgetModel& scroll);

}  // namespace ora::game::ui
