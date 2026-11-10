export module ora.game.ui;

import std;
import ora.game;
import ora.game.input;
import ora.game.miniyaml;
import ora.game.render;
import ora.game.sound;
import ora.platforms;

export {
namespace ora::game::ui {

[[nodiscard]] std::expected<int, std::string> widget_int_eval(
    std::string_view str_expr, const std::map<std::string, int>& map_symbols);

}

namespace ora::game::ui {

struct WidgetBounds {
  int int4_x{};
  int int4_y{};
  int int4_width{};
  int int4_height{};
};

struct WidgetModel {
  std::string str_type;
  std::string str_id;
  std::string str_x;
  std::string str_y;
  std::string str_width;
  std::string str_height;
  bool b_visible{true};
  bool b_ignore_mouse_over{};
  bool b_ignore_child_mouse_over{};
  bool b_click_through{};
  bool b_disabled{};
  bool b_depressed{};
  bool b_moving{};
  bool b_checked{};
  bool b_has_double_click{};
  bool b_record_mouse_down{};
  float fp4_value{};
  float fp4_min_value{};
  float fp4_max_value{1.0f};
  int int4_ticks{};
  int int4_track_height{5};
  std::vector<std::string> vec_logic;
  std::string str_default_cursor;
  std::vector<std::pair<std::string, std::string>> vec_fields;
  std::vector<std::unique_ptr<WidgetModel>> vec_children;
  WidgetModel* ptr_parent{};
  WidgetBounds bounds{};
};

struct UiEventRecord {
  std::string str_id;
  std::string str_kind;
};

struct UiState {
  WidgetModel* ptr_mouse_focus{};
  WidgetModel* ptr_keyboard_focus{};
  WidgetModel* ptr_mouse_over{};
  int int4_last_mouse_x{};
  int int4_last_mouse_y{};
  std::vector<WidgetModel*> vec_entered;
  std::vector<WidgetModel*> vec_exited;
  std::vector<UiEventRecord> vec_events;
};

struct WidgetInitCtx {
  int int4_window_width{1024};
  int int4_window_height{768};
  const std::map<std::string, int>* map_substitutions{};
  std::string str_default_cursor;
};

void widget_initialize(WidgetModel& w, const WidgetInitCtx& ctx);

[[nodiscard]] std::pair<int, int> widget_render_origin(const WidgetModel& w);

[[nodiscard]] WidgetBounds widget_event_bounds(const WidgetModel& w);

[[nodiscard]] bool widget_event_bounds_contains(const WidgetModel& w, int int4_x, int int4_y);

struct SliderGeometry {
  int int4_track_origin_x{};
  int int4_track_x{};
  int int4_track_y{};
  int int4_track_w{};
  int int4_track_h{};
  int int4_thumb_x{};
  int int4_thumb_y{};
  int int4_thumb_w{};
  int int4_thumb_h{};
};

[[nodiscard]] SliderGeometry widget_slider_geometry(const WidgetModel& w);

float widget_slider_value_from_px(const WidgetModel& w, int int4_x);

void widget_slider_update_value(WidgetModel& w, UiState& st, float fp4_new);

[[nodiscard]] bool widget_slider_thumb_contains(const WidgetModel& w, int int4_x, int int4_y);

[[nodiscard]] bool widget_take_mouse_focus(WidgetModel& w, UiState& st);

[[nodiscard]] bool widget_yield_mouse_focus(WidgetModel& w, UiState& st);

[[nodiscard]] bool widget_handle_mouse_input(WidgetModel& w, UiState& st,
                                             const ora::game::input::MouseInput& mi);

[[nodiscard]] bool widget_handle_mouse_input_outer(WidgetModel& w, UiState& st,
                                                   const ora::game::input::MouseInput& mi);

[[nodiscard]] bool ui_handle_input(WidgetModel& root, UiState& st,
                                   const ora::game::input::MouseInput& mi);

[[nodiscard]] bool widget_handle_key_press_outer(WidgetModel& w, UiState& st,
                                                 const ora::game::input::KeyInput& ki);

[[nodiscard]] bool ui_handle_key_press(WidgetModel& root, UiState& st,
                                       const ora::game::input::KeyInput& ki);

[[nodiscard]] const std::string* widget_get_cursor(const WidgetModel& w);

// 上游 Widget.Get(id)（Widget.cs:190-197）：子树先序查找（子节点声明序优先于自身）。
[[nodiscard]] WidgetModel* widget_find_by_id(WidgetModel& w, std::string_view str_id);

[[nodiscard]] const std::string* widget_hit_test(const WidgetModel& w, int int4_x, int int4_y);

}

namespace ora::game::ui {

[[nodiscard]] std::expected<std::map<std::string, std::string>, std::string> chrome_metrics_initialize(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources);

struct ChromeRect {
  int int4_x{};
  int int4_y{};
  int int4_width{};
  int int4_height{};
};

struct ChromeCollection {
  std::string str_image;
  std::string str_image2x;
  std::string str_image3x;
  std::vector<int> vec_panel_region;
  std::uint32_t u4_panel_sides{0x1F};
  std::map<std::string, ChromeRect> map_regions;
};

[[nodiscard]] std::expected<std::map<std::string, ChromeCollection>, std::string>
chrome_provider_initialize(const std::vector<std::vector<MiniYamlNode>>& vec_sources);

[[nodiscard]] std::optional<ChromeRect> chrome_try_get_image(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection,
    std::string_view str_image);

[[nodiscard]] std::vector<std::optional<ChromeRect>> chrome_get_panel_images(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection);

[[nodiscard]] std::pair<int, int> chrome_get_minimum_panel_size(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection);

struct ChromeSheet {
  std::uint32_t u4_id{};
  int int4_width{};
  int int4_height{};
  std::vector<std::uint32_t> vec_u4_argb;
};

struct ChromeSheetSet {
  std::map<std::string, ChromeSheet> map_sheets;
  std::uint32_t u4_next_id{1};
};

[[nodiscard]] std::expected<std::uint32_t, std::string> chrome_sheet_load(
    ChromeSheetSet& set, std::string_view str_image_name, std::span<const std::uint8_t> span1u_png);

[[nodiscard]] const ChromeSheet* chrome_sheet_for_collection(
    const ChromeSheetSet& set, const std::map<std::string, ChromeCollection>& map_collections,
    std::string_view str_collection);

}

namespace ora::game::ui {

struct WidgetTypeSpec {
  std::string_view str_type;
  bool b_click_through{};
  bool b_ignore_mouse_over{};
  bool b_is_button{};
  bool b_cursor_null{};
};

[[nodiscard]] const std::vector<WidgetTypeSpec>& widget_type_registry();

[[nodiscard]] const WidgetTypeSpec* widget_type_spec_find(std::string_view str_type);

struct WidgetLoaderModel {
  std::map<std::string, const MiniYamlNode*> map_widgets;
};

[[nodiscard]] std::expected<WidgetLoaderModel, std::string> widget_loader_from_nodes(
    const std::vector<MiniYamlNode>& vec_nodes);

[[nodiscard]] std::expected<WidgetModel*, std::string> widget_loader_load_widget(
    const WidgetLoaderModel& loader, WidgetModel& parent, const WidgetInitCtx& ctx,
    std::string_view str_id);

[[nodiscard]] std::string_view widget_key_type_part(std::string_view str_key);

[[nodiscard]] std::string_view widget_key_id_part(std::string_view str_key);

}

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
// b_start_skirmish（T28）：SKIRMISH_BUTTON 的 StartSkirmishGame 出口位——上游连接
// 本地服务器开 SERVER_LOBBY，C++ 由 launcher 流程状态机（launcher/flow_state）承接。
struct MainMenuState {
  MenuType type{MenuType::kStartupPrompts};
  bool b_quit{};
  bool b_settings_open{};
  bool b_start_skirmish{};
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

namespace ora::game::ui {

struct UiQuad {
  int int4_x{};
  int int4_y{};
  int int4_width{};
  int int4_height{};
  int int4_src_x{};
  int int4_src_y{};
  std::uint32_t u4_sheet{};
  std::uint32_t u4_rgba{0xFFFFFFFFu};
};

struct UiRenderCtx {
  std::map<std::string, render::SpriteFont>* map_fonts{};
  std::string str_default_font;
  std::uint32_t u4_default_color{0xFFFFFFFFu};
  const std::map<std::string, ChromeCollection>* map_chrome{};
  const ChromeSheetSet* set_sheets{};
  const std::map<std::string, std::string>* map_metrics{};
  const UiState* ptr_state{};
};

[[nodiscard]] std::vector<UiQuad> ui_draw_tree(const WidgetModel& root, const UiRenderCtx& ctx);

[[nodiscard]] std::vector<std::string> widgets_draw_dump(const WidgetModel& root, const UiRenderCtx& ctx);

[[nodiscard]] std::string widget_stateful_image_name(std::string_view str_base, bool b_disabled,
                                                     bool b_pressed, bool b_hover);

}

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
}
