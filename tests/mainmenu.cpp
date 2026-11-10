import std;

#include <algorithm>

#include "game/input/hotkey_registry.hpp"
#include "game/miniyaml.hpp"
#include "game/settings.hpp"
#include "game/sound/sound.hpp"
#include "game/ui/mainmenu_logic.hpp"
#include "game/ui/settings_panel.hpp"
#include "game/ui/widget.hpp"
#include "game/ui/widget_loader.hpp"
#include "platforms/openal_loader.hpp"
#include "test_util.hpp"
using ora::test::check;

// 菜单测试不依赖真音频设备：强制 OpenAL 探测失败回落 dummy（dsound 真链在
// asan 环境崩溃；音量断言走 facade 记录面与引擎无关）
namespace pl = ora::platforms;
struct OpenalDummyGuard {
  OpenalDummyGuard() { pl::openal_probe_override("ora-mainmenu-test-no-such-dir/soft_oal.dll"); }
  ~OpenalDummyGuard() { pl::openal_probe_override(""); }
};

#ifndef ORA_MAINMENU_GOLDEN_DIR
#define ORA_MAINMENU_GOLDEN_DIR "tests/golden/mainmenu"
#endif

namespace {

namespace ui = ora::game::ui;
namespace input = ora::game::input;

std::optional<std::vector<ora::game::MiniYamlNode>> load_nodes(const std::string& str_name) {
  const auto res{ora::game::miniyaml_from_file(
      std::string{ORA_MAINMENU_GOLDEN_DIR} + "/fixture/" + str_name)};
  if (!res.has_value()) {
    check(false, std::format("corpus {} parses: {}", str_name, res.error()));
    return std::nullopt;
  }
  return std::optional<std::vector<ora::game::MiniYamlNode>>{std::move(*res)};
}

ui::WidgetInitCtx menu_ctx() {
  return ui::WidgetInitCtx{.int4_window_width = 1024, .int4_window_height = 768,
                           .map_substitutions = nullptr, .str_default_cursor = "default"};
}

// 上游 Ui.LoadWidget 形态：顶层 MAINMENU 挂哑 root（PARENT=0）。
// 返回 unique_ptr 保持根节点存活——子树 ptr_parent 指向该根，值语义移动会悬垂
std::unique_ptr<ui::WidgetModel> load_menu_tree() {
  const auto vec_nodes{load_nodes("mainmenu.yaml")};
  if (!vec_nodes.has_value()) return nullptr;
  const auto res_loader{ui::widget_loader_from_nodes(*vec_nodes)};
  if (!res_loader.has_value()) {
    check(false, std::format("mainmenu loader build: {}", res_loader.error()));
    return nullptr;
  }
  auto ptr_root{std::make_unique<ui::WidgetModel>()};
  ptr_root->str_type = "Container";
  ptr_root->str_id = "ROOT";
  ptr_root->bounds = ui::WidgetBounds{0, 0, 1024, 768};
  const auto res_widget{
      ui::widget_loader_load_widget(*res_loader, *ptr_root, menu_ctx(), "MAINMENU")};
  if (!res_widget.has_value()) {
    check(false, std::format("MAINMENU load: {}", res_widget.error()));
    return nullptr;
  }
  return ptr_root;
}

// 合成点击（菜单按钮中心）→ 返回本步新增事件
std::vector<ui::UiEventRecord> click_at(ui::WidgetModel& root, ui::UiState& st, int int4_x,
                                        int int4_y) {
  const std::size_t sz_base{st.vec_events.size()};
  input::MouseInput mi{};
  mi.button = input::MouseButton::kLeft;
  mi.event = input::MouseInputEvent::kDown;
  mi.int4_x = int4_x;
  mi.int4_y = int4_y;
  mi.int4_tap_count = 1;
  static_cast<void>(ui::ui_handle_input(root, st, mi));
  mi.event = input::MouseInputEvent::kUp;
  static_cast<void>(ui::ui_handle_input(root, st, mi));
  return {st.vec_events.begin() + static_cast<std::ptrdiff_t>(sz_base), st.vec_events.end()};
}

std::string click_kinds(const std::vector<ui::UiEventRecord>& vec_events) {
  std::string str_out;
  for (const auto& rec : vec_events) {
    if (!str_out.empty()) str_out += ';';
    str_out += rec.str_id + ":" + rec.str_kind;
  }
  return str_out;
}

bool visible_menu(ui::WidgetModel& root, std::string_view str_id) {
  const ui::WidgetModel* ptr_w{ui::widget_find_by_id(root, str_id)};
  return ptr_w != nullptr && ptr_w->b_visible;
}

const ui::WidgetModel* menu_button(ui::WidgetModel& root, std::string_view str_menu,
                                   std::string_view str_id) {
  const ui::WidgetModel* ptr_menu{ui::widget_find_by_id(root, str_menu)};
  if (ptr_menu == nullptr) return nullptr;
  return ui::widget_find_by_id(*const_cast<ui::WidgetModel*>(ptr_menu), str_id);
}

// tab 按钮与 panel 克隆同名（上游 tab.Id = panelID）：容器子树限定查找
ui::WidgetModel* container_child(ui::WidgetModel& root, std::string_view str_container,
                                 std::string_view str_id) {
  ui::WidgetModel* ptr_container{ui::widget_find_by_id(root, str_container)};
  if (ptr_container == nullptr) return nullptr;
  return ui::widget_find_by_id(*ptr_container, str_id);
}

std::pair<int, int> button_center(const ui::WidgetModel& w) {
  const ui::WidgetBounds b{ui::widget_event_bounds(w)};
  return {b.int4_x + b.int4_width / 2, b.int4_y + b.int4_height / 2};
}

void test_menu_navigation() {
  auto ptr_tree{load_menu_tree()};
  if (ptr_tree == nullptr) return;
  ui::WidgetModel& root{*ptr_tree};

  ui::MainMenuEnv env{};
  env.b_has_maps = true;       // ra MapCache 有 Lobby 可见图（ore-lord 等）
  env.b_has_campaign = true;   // ra manifest Missions: ra|missions.yaml 非空
  env.b_has_missions = true;
  env.b_load_panel_enabled = false;
  env.b_has_content_installer = true;  // ra FileSystemLoader 实现 IFileSystemExternalContent（nav.txt CONTENT=0）

  ui::MainMenuState st;
  ui::main_menu_setup(st);
  check(st.type == ui::MenuType::kMain, "setup lands on Main (prompts skipped, news off)");
  check(st.vec_nav.size() == 1 && st.vec_nav[0] == "switch Main", "setup nav record");

  ui::main_menu_apply(st, env, root);
  ui::main_menu_apply_disabled(env, root);
  check(visible_menu(root, "MAIN_MENU"), "MAIN_MENU visible at Main");
  check(!visible_menu(root, "SINGLEPLAYER_MENU"), "SINGLEPLAYER_MENU hidden at Main");
  check(!visible_menu(root, "EXTRAS_MENU"), "EXTRAS_MENU hidden at Main");
  check(!visible_menu(root, "MAP_EDITOR_MENU"), "MAP_EDITOR_MENU hidden at Main");

  const ui::WidgetModel* ptr_single{
      menu_button(root, "MAIN_MENU", "SINGLEPLAYER_BUTTON")};
  const ui::WidgetModel* ptr_skirmish{menu_button(root, "SINGLEPLAYER_MENU", "SKIRMISH_BUTTON")};
  const ui::WidgetModel* ptr_missions{menu_button(root, "SINGLEPLAYER_MENU", "MISSIONS_BUTTON")};
  const ui::WidgetModel* ptr_load{menu_button(root, "SINGLEPLAYER_MENU", "LOAD_BUTTON")};
  const ui::WidgetModel* ptr_content{menu_button(root, "MAIN_MENU", "CONTENT_BUTTON")};
  check(ptr_single != nullptr && ptr_skirmish != nullptr && ptr_missions != nullptr
            && ptr_load != nullptr && ptr_content != nullptr,
        "menu buttons found");
  if (ptr_single == nullptr || ptr_skirmish == nullptr || ptr_missions == nullptr
      || ptr_load == nullptr || ptr_content == nullptr)
    return;
  check(!ptr_skirmish->b_disabled, "skirmish enabled (has maps)");
  check(!ptr_missions->b_disabled, "missions enabled (has campaign)");
  check(ptr_load->b_disabled, "load button disabled (no saves support)");
  check(!ptr_content->b_disabled, "content button enabled (ra external content installer)");

  ui::UiState uist;

  // SINGLEPLAYER_BUTTON → SwitchMenu(Singleplayer)
  {
    const auto [x, y]{button_center(*ptr_single)};
    const auto vec_clicked{click_at(root, uist, x, y)};
    check(click_kinds(vec_clicked) == "SINGLEPLAYER_BUTTON:click", "singleplayer click event");
    ui::main_menu_dispatch(st, env, root, vec_clicked);
    check(st.type == ui::MenuType::kSingleplayer, "menu type Singleplayer");
    ui::main_menu_apply(st, env, root);
    check(!visible_menu(root, "MAIN_MENU"), "MAIN_MENU hidden at Singleplayer");
    check(visible_menu(root, "SINGLEPLAYER_MENU"), "SINGLEPLAYER_MENU visible");
  }

  // BACK_BUTTON（singleplayer 语境）→ Main
  {
    const ui::WidgetModel* ptr_back{menu_button(root, "SINGLEPLAYER_MENU", "BACK_BUTTON")};
    check(ptr_back != nullptr, "singleplayer back found");
    if (ptr_back != nullptr) {
      const auto [x, y]{button_center(*ptr_back)};
      const auto vec_clicked{click_at(root, uist, x, y)};
      ui::main_menu_dispatch(st, env, root, vec_clicked);
      check(st.type == ui::MenuType::kMain, "back to Main");
      ui::main_menu_apply(st, env, root);
      check(visible_menu(root, "MAIN_MENU"), "MAIN_MENU visible again");
    }
  }

  // SETTINGS_BUTTON → SwitchMenu(None) + SETTINGS_PANEL 窗口开
  {
    const ui::WidgetModel* ptr_settings{menu_button(root, "MAIN_MENU", "SETTINGS_BUTTON")};
    check(ptr_settings != nullptr, "settings button found");
    if (ptr_settings != nullptr) {
      const auto [x, y]{button_center(*ptr_settings)};
      const auto vec_clicked{click_at(root, uist, x, y)};
      ui::main_menu_dispatch(st, env, root, vec_clicked);
      check(st.type == ui::MenuType::kNone, "settings opens with menu type None");
      check(st.b_settings_open, "settings window open");
      check(st.vec_nav.back() == "open SETTINGS_PANEL", "nav records settings panel open");
      ui::main_menu_close_settings(st);
      check(st.type == ui::MenuType::kMain && !st.b_settings_open,
            "settings onExit returns to Main");
    }
  }

  // EXTRAS_BUTTON → Extras；MAP_EDITOR_BUTTON → MapEditor；BACK 链回 Main
  {
    const ui::WidgetModel* ptr_extras{menu_button(root, "MAIN_MENU", "EXTRAS_BUTTON")};
    const ui::WidgetModel* ptr_editor{
        menu_button(root, "EXTRAS_MENU", "MAP_EDITOR_BUTTON")};
    const ui::WidgetModel* ptr_editor_back{
        menu_button(root, "MAP_EDITOR_MENU", "BACK_BUTTON")};
    const ui::WidgetModel* ptr_extras_back{menu_button(root, "EXTRAS_MENU", "BACK_BUTTON")};
    check(ptr_extras != nullptr && ptr_editor != nullptr && ptr_editor_back != nullptr
              && ptr_extras_back != nullptr,
          "extras chain buttons found");
    if (ptr_extras != nullptr && ptr_editor != nullptr && ptr_editor_back != nullptr
        && ptr_extras_back != nullptr) {
      auto [ex, ey]{button_center(*ptr_extras)};
      ui::main_menu_dispatch(st, env, root, click_at(root, uist, ex, ey));
      check(st.type == ui::MenuType::kExtras, "extras menu");
      ui::main_menu_apply(st, env, root);
      check(visible_menu(root, "EXTRAS_MENU"), "EXTRAS_MENU visible");

      const ui::WidgetModel* ptr_load_map{
          menu_button(root, "MAP_EDITOR_MENU", "LOAD_MAP_BUTTON")};
      check(ptr_load_map != nullptr && !ptr_load_map->b_disabled, "load map enabled (has maps)");

      auto [dx, dy]{button_center(*ptr_editor)};
      ui::main_menu_dispatch(st, env, root, click_at(root, uist, dx, dy));
      check(st.type == ui::MenuType::kMapEditor, "map editor menu");
      ui::main_menu_apply(st, env, root);
      check(visible_menu(root, "MAP_EDITOR_MENU"), "MAP_EDITOR_MENU visible");

      auto [bx, by]{button_center(*ptr_editor_back)};
      ui::main_menu_dispatch(st, env, root, click_at(root, uist, bx, by));
      check(st.type == ui::MenuType::kExtras, "map editor back to extras");

      auto [cx, cy]{button_center(*ptr_extras_back)};
      ui::main_menu_dispatch(st, env, root, click_at(root, uist, cx, cy));
      check(st.type == ui::MenuType::kMain, "extras back to main");
    }
  }

  // 不可用面板按钮：记录 open 序列（面板本体不在本票）
  {
    const ui::WidgetModel* ptr_multi{menu_button(root, "MAIN_MENU", "MULTIPLAYER_BUTTON")};
    const ui::WidgetModel* ptr_music{menu_button(root, "EXTRAS_MENU", "MUSIC_BUTTON")};
    check(ptr_multi != nullptr && ptr_music != nullptr, "panel buttons found");
    if (ptr_multi != nullptr && ptr_music != nullptr) {
      ui::main_menu_switch(st, ui::MenuType::kMain);
      ui::main_menu_apply(st, env, root);
      const auto [x, y]{button_center(*ptr_multi)};
      ui::main_menu_dispatch(st, env, root, click_at(root, uist, x, y));
      check(st.type == ui::MenuType::kNone, "multiplayer switches to None");
      check(st.vec_nav.back() == "open MULTIPLAYER_PANEL", "multiplayer panel nav record");
      ui::main_menu_switch(st, ui::MenuType::kMain);

      ui::main_menu_switch(st, ui::MenuType::kExtras);
      ui::main_menu_apply(st, env, root);
      const auto [mx, my]{button_center(*ptr_music)};
      ui::main_menu_dispatch(st, env, root, click_at(root, uist, mx, my));
      check(st.vec_nav.back() == "open MUSIC_PANEL", "music panel nav record");
    }
  }

  // QUIT_BUTTON → Game.Exit 等价
  {
    const ui::WidgetModel* ptr_quit{menu_button(root, "MAIN_MENU", "QUIT_BUTTON")};
    check(ptr_quit != nullptr, "quit button found");
    if (ptr_quit != nullptr) {
      ui::main_menu_switch(st, ui::MenuType::kMain);
      ui::main_menu_apply(st, env, root);
      const auto [x, y]{button_center(*ptr_quit)};
      ui::main_menu_dispatch(st, env, root, click_at(root, uist, x, y));
      check(st.b_quit, "quit requested");
    }
  }

  // 无战役无任务环境：MISSIONS 禁用（上游 !hasCampaign && !hasMissions）
  {
    ui::MainMenuEnv env_bare{};
    env_bare.b_has_maps = false;
    ui::WidgetModel* ptr_root{&root};
    ui::main_menu_apply_disabled(env_bare, *ptr_root);
    const ui::WidgetModel* ptr_missions2{menu_button(root, "SINGLEPLAYER_MENU", "MISSIONS_BUTTON")};
    const ui::WidgetModel* ptr_skirmish2{
        menu_button(root, "SINGLEPLAYER_MENU", "SKIRMISH_BUTTON")};
    check(ptr_missions2 != nullptr && ptr_missions2->b_disabled, "missions disabled bare env");
    check(ptr_skirmish2 != nullptr && ptr_skirmish2->b_disabled, "skirmish disabled bare env");
  }
}

void test_settings_panel() {
  const OpenalDummyGuard guard_dummy_engine;
  const auto vec_settings{load_nodes("settings.yaml")};
  const auto vec_display{load_nodes("settings-display.yaml")};
  const auto vec_audio{load_nodes("settings-audio.yaml")};
  const auto vec_hotkeys{load_nodes("settings-hotkeys.yaml")};
  if (!vec_settings.has_value() || !vec_display.has_value() || !vec_audio.has_value()
      || !vec_hotkeys.has_value())
    return;
  const auto res_loader_settings{ui::widget_loader_from_nodes(*vec_settings)};
  const auto res_loader_display{ui::widget_loader_from_nodes(*vec_display)};
  const auto res_loader_audio{ui::widget_loader_from_nodes(*vec_audio)};
  const auto res_loader_hotkeys{ui::widget_loader_from_nodes(*vec_hotkeys)};
  check(res_loader_settings.has_value() && res_loader_display.has_value()
            && res_loader_audio.has_value() && res_loader_hotkeys.has_value(),
        "settings corpus loaders build");
  if (!res_loader_settings.has_value() || !res_loader_display.has_value()
      || !res_loader_audio.has_value() || !res_loader_hotkeys.has_value())
    return;

  // 设置模型 + 热键注册表（合成最小注册表：普通/只读/未绑定三行）
  const auto res_hotkeys{input::hotkey_registry_load({*ora::game::miniyaml_from_string(
      "CycleBase: HOME\n\tContexts: player\n"
      "Sell: Z Ctrl\n\tReadonly: True\n\tContexts: player\n"
      "ViewEarth: UNKNOWN\n\tContexts: player\n",
      "synth-hotkeys")})};
  check(res_hotkeys.has_value(), "synthetic hotkey registry loads");
  if (!res_hotkeys.has_value()) return;

  const std::filesystem::path path_dir{std::filesystem::temp_directory_path()
                                       / "ora-mainmenu-test"};
  std::filesystem::create_directories(path_dir);
  const std::string str_settings_path{(path_dir / "settings.yaml").string()};

  auto res_model{ora::game::settings_load("")};
  check(res_model.has_value(), "fresh settings model loads");
  if (!res_model.has_value()) return;
  ora::game::SettingsModel& sm{*res_model};

  auto ptr_sound{std::make_unique<ora::game::sound_facade>(
      ora::game::make_sound_facade_settings("", false, sm.sound))};
  input::HotkeyRegistryModel hotkeys{*res_hotkeys};

  ui::WidgetModel root;
  root.str_type = "Container";
  root.str_id = "ROOT";
  root.bounds = ui::WidgetBounds{0, 0, 1024, 768};

  ui::SettingsPanelState st;
  st.ptr_settings = &sm;
  st.ptr_hotkeys = &hotkeys;
  st.ptr_sound = ptr_sound.get();

  const auto res_open{ui::settings_panel_open(
      st, root, *res_loader_settings, *res_loader_display, *res_loader_audio,
      *res_loader_hotkeys, menu_ctx(), str_settings_path)};
  check(res_open.has_value(), "settings panel opens");
  if (!res_open.has_value()) {
    std::println("open error: {}", res_open.error());
    return;
  }

  // SettingsLogic ctor 面：PANEL_TEMPLATE 三克隆 + tab 模板克隆（ButtonStride 0,35）
  ui::WidgetModel* ptr_container{ui::widget_find_by_id(root, "PANEL_CONTAINER")};
  ui::WidgetModel* ptr_tabs{ui::widget_find_by_id(root, "SETTINGS_TAB_CONTAINER")};
  check(ptr_container != nullptr && ptr_tabs != nullptr, "panel/tab containers found");
  if (ptr_container == nullptr || ptr_tabs == nullptr) return;
  check(ptr_container->vec_children.size() == 3, "three panel clones (template removed)");
  check(ptr_container->vec_children[0]->str_id == "DISPLAY_PANEL"
            && ptr_container->vec_children[1]->str_id == "AUDIO_PANEL"
            && ptr_container->vec_children[2]->str_id == "HOTKEYS_PANEL",
        "panel clone order = Panels registration order subset");
  check(ptr_container->vec_children[0]->b_visible && !ptr_container->vec_children[1]->b_visible
            && !ptr_container->vec_children[2]->b_visible,
        "first registered panel active");
  check(ptr_tabs->vec_children.size() == 3, "three tab buttons (template removed)");
  check(ptr_tabs->vec_children[0]->str_id == "DISPLAY_PANEL"
            && ptr_tabs->vec_children[1]->str_id == "AUDIO_PANEL"
            && ptr_tabs->vec_children[2]->str_id == "HOTKEYS_PANEL",
        "tab ids = panel ids");
  check(ptr_tabs->vec_children[0]->bounds.int4_x == 0
            && ptr_tabs->vec_children[0]->bounds.int4_y == 0
            && ptr_tabs->vec_children[1]->bounds.int4_x == 0
            && ptr_tabs->vec_children[1]->bounds.int4_y == 35
            && ptr_tabs->vec_children[2]->bounds.int4_x == 0
            && ptr_tabs->vec_children[2]->bounds.int4_y == 70,
        "tab stride = ButtonStride 0,35 from template origin");
  check(ptr_tabs->vec_children[0]->bounds.int4_width == 160
            && ptr_tabs->vec_children[0]->bounds.int4_height == 25,
        "tab size from BUTTON_TEMPLATE");

  // —— 显示页初值（分辨率/全屏接 Settings.Graphics）——
  check(st.str_window_width_text == "1024" && st.str_window_height_text == "768",
        "window text mirrors graphics.windowed_size default");
  check(st.str_mode_text == "options-display-mode.fullscreen",
        "mode text = pseudo-fullscreen fluent key (default mode)");
  const ui::WidgetModel* ptr_mode{
      ui::widget_find_by_id(root, "MODE_DROPDOWN")};
  const ui::WidgetModel* ptr_width_field{ui::widget_find_by_id(root, "WINDOW_WIDTH")};
  const ui::WidgetModel* ptr_vsync{ui::widget_find_by_id(root, "VSYNC_CHECKBOX")};
  check(ptr_mode != nullptr && ptr_width_field != nullptr && ptr_vsync != nullptr,
        "display page controls found");
  check(ptr_width_field != nullptr && ptr_width_field->str_type == "TextField",
        "window width is a TextField");

  // MODE 选项（ShowWindowModeDropdown：fullscreen/legacy-fullscreen/windowed 序）
  ui::settings_panel_set_window_mode(st, ora::game::WindowMode::kWindowed);
  check(sm.graphics.mode == ora::game::WindowMode::kWindowed, "mode dropdown writes Graphics.Mode");
  check(st.str_mode_text == "options-display-mode.windowed", "mode text windowed");
  ui::settings_panel_set_window_mode(st, ora::game::WindowMode::kFullscreen);
  check(st.str_mode_text == "options-display-mode.legacy-fullscreen",
        "mode text legacy fullscreen");
  ui::settings_panel_set_window_mode(st, ora::game::WindowMode::kPseudoFullscreen);
  check(st.str_mode_text == "options-display-mode.fullscreen", "mode text fullscreen");

  // 窗口分辨率文本（leave 时 TryParse → WindowedSize；非法文本 → 0）
  ui::settings_panel_set_window_text(st, true, "1280");
  ui::settings_panel_set_window_text(st, false, "720");
  ui::settings_panel_set_window_mode(st, ora::game::WindowMode::kWindowed);
  st.b_needs_restart |= ui::settings_panel_leave(st, ui::SettingsPanelId::kDisplay);
  check(st.b_needs_restart, "display leave needs restart");
  check(sm.graphics.windowed_size.int4_x == 1280 && sm.graphics.windowed_size.int4_y == 720,
        "leave parses window text into WindowedSize");
  check(st.b_needs_restart, "needsRestart sticky after leave");

  ui::settings_panel_set_window_text(st, true, "not-a-number");
  static_cast<void>(ui::settings_panel_leave(st, ui::SettingsPanelId::kDisplay));
  check(sm.graphics.windowed_size.int4_x == 0, "bad width text parses as 0 (int.TryParse)");

  // VSYNC 复选框（BindCheckboxPref：IsChecked ← VSync；OnClick 翻转）
  const bool b_vsync_before{sm.graphics.b_vsync};
  ui::settings_panel_toggle_vsync(st);
  check(sm.graphics.b_vsync == !b_vsync_before, "vsync checkbox toggles Graphics.VSync");

  // —— 音频页（三通道滑条接 T20 门面 + 设置）——
  ui::settings_panel_tab_click(st, ui::SettingsPanelId::kAudio);
  check(st.active == ui::SettingsPanelId::kAudio, "tab click switches active panel");
  check(container_child(root, "PANEL_CONTAINER", "AUDIO_PANEL")->b_visible
            && !container_child(root, "PANEL_CONTAINER", "DISPLAY_PANEL")->b_visible,
        "audio panel visible after tab click");
  ui::WidgetModel* ptr_sound_slider{ui::widget_find_by_id(root, "SOUND_VOLUME")};
  check(ptr_sound_slider != nullptr && ptr_sound_slider->str_type == "ExponentialSlider",
        "sound volume slider found");
  if (ptr_sound_slider != nullptr) {
    check(ptr_sound_slider->fp4_value == sm.sound.fp_sound_volume,
          "slider value mirrors SoundVolume on open");
    check(ptr_sound_slider->fp4_min_value == 0.0f && ptr_sound_slider->fp4_max_value == 1.0f,
          "slider range from chrome defaults");
    // 拖动滑条（合成鼠标）→ change 事件 → 设置 + 引擎音量
    ui::UiState uist;
    const ui::WidgetBounds b{ui::widget_event_bounds(*ptr_sound_slider)};
    std::size_t sz_base{uist.vec_events.size()};
    input::MouseInput mi{};
    mi.button = input::MouseButton::kLeft;
    mi.event = input::MouseInputEvent::kDown;
    mi.int4_x = b.int4_x + b.int4_width * 3 / 4;
    mi.int4_y = b.int4_y + b.int4_height / 2;
    mi.int4_tap_count = 1;
    static_cast<void>(ui::ui_handle_input(root, uist, mi));
    mi.event = input::MouseInputEvent::kUp;
    static_cast<void>(ui::ui_handle_input(root, uist, mi));
    const std::vector<ui::UiEventRecord> vec_drag{
        uist.vec_events.begin() + static_cast<std::ptrdiff_t>(sz_base), uist.vec_events.end()};
    ui::settings_panel_dispatch(st, vec_drag);
    check(sm.sound.fp_sound_volume > 0.5f, "sound slider drag raises SoundVolume");
    check(ora::game::sound_facade_sound_volume(*ptr_sound) == sm.sound.fp_sound_volume,
          "live engine volume follows slider (T20)");
    check(ptr_sound_slider->fp4_value == sm.sound.fp_sound_volume, "tree slider value synced");
  }

  // 音量 API 面（其余两通道）
  ui::settings_panel_set_volume(st, 1, 0.25f);
  check(sm.sound.fp_music_volume == 0.25f
            && ora::game::sound_facade_music_volume(*ptr_sound) == 0.25f,
        "music volume channel wired");
  ui::settings_panel_set_volume(st, 2, 0.75f);
  check(sm.sound.fp_video_volume == 0.75f
            && ora::game::sound_facade_video_volume(*ptr_sound) == 0.75f,
        "video volume channel wired");

  // MUTE 复选框（设置翻转 + 引擎主音量 0/1；dummy 引擎经 set 调用记录观测——
  // 上游 DummySoundEngine.Volume get 恒 0，同 oracle 记录引擎口径）
  ui::settings_panel_toggle_mute(st);
  check(sm.sound.b_mute, "mute checkbox toggles Sound.Mute");
  {
    const auto& vec_master_sets{ora::platforms::engine_master_sets(ptr_sound->engine)};
    check(vec_master_sets.size() == 1 && vec_master_sets.back() == 0.0f,
          "mute sets master volume 0");
  }
  ui::settings_panel_toggle_mute(st);
  {
    const auto& vec_master_sets{ora::platforms::engine_master_sets(ptr_sound->engine)};
    check(vec_master_sets.size() == 2 && vec_master_sets.back() == 1.0f,
          "unmute restores master 1");
  }

  // —— 输入页（改键列表接 T22 注册表）——
  ui::settings_panel_tab_click(st, ui::SettingsPanelId::kHotkeys);
  std::vector<ui::HotkeyRow> vec_rows{ui::settings_panel_hotkey_rows(hotkeys)};
  check(vec_rows.size() == 3, "hotkey rows from registry order");
  if (vec_rows.size() == 3) {
    check(vec_rows[0].str_name == "CycleBase" && vec_rows[0].str_key == "HOME None"
              && vec_rows[0].str_default == "HOME None" && !vec_rows[0].b_readonly,
          "row 0 CycleBase");
    check(vec_rows[1].str_name == "Sell" && vec_rows[1].str_key == "Z Ctrl"
              && vec_rows[1].b_readonly,
          "row 1 Sell readonly");
    check(vec_rows[2].str_name == "ViewEarth" && !vec_rows[2].b_has_duplicates,
          "row 2 ViewEarth unbound");
  }

  // 改键成功（hotkey_set + settings Keys 持久化节点）
  const input::HotkeyModel hk_new{input::Keycode::kE, input::ModifierKeys::kCtrl};
  check(ui::settings_panel_rekey(st, "CycleBase", hk_new), "rekey CycleBase succeeds");
  {
    const auto opt_idx{input::hotkey_registry_find(hotkeys, "CycleBase")};
    check(opt_idx.has_value()
              && input::hotkey_to_string(hotkeys.vec_keys[*opt_idx]) == "E Ctrl",
          "registry key updated");
  }
  bool b_keys_written{false};
  if (const ora::game::MiniYamlNode* ptr_keys{ora::game::settings_find_section(sm, "Keys")}) {
    for (const auto& node_child : ptr_keys->vec_children)
      if (node_child.str_key == "CycleBase" && node_child.str_value == "E Ctrl")
        b_keys_written = true;
  }
  check(b_keys_written, "rekey writes settings Keys node");

  // 只读拒绝
  check(!ui::settings_panel_rekey(st, "Sell", hk_new), "readonly rekey rejected");

  // 冲突行标记（ViewEarth 改到与 CycleBase 当前键 E Ctrl 相同）
  const input::HotkeyModel hk_dup{input::Keycode::kE, input::ModifierKeys::kCtrl};
  check(ui::settings_panel_rekey(st, "ViewEarth", hk_dup), "rekey ViewEarth to E Ctrl");
  vec_rows = ui::settings_panel_hotkey_rows(hotkeys);
  check(vec_rows.size() == 3 && vec_rows[0].b_has_duplicates && vec_rows[2].b_has_duplicates,
        "duplicate rows flagged");

  // —— BACK_BUTTON：leave + save + onExit ——
  ui::MainMenuState menu;
  ui::main_menu_setup(menu);
  ui::main_menu_open_settings(menu);
  ui::settings_panel_close(st, &menu);
  check(!menu.b_settings_open && menu.type == ui::MenuType::kMain, "close returns menu to Main");
  check(std::filesystem::exists(str_settings_path), "back saved settings file");

  // —— 持久化重进生效：重载设置 → 重开面板 → 初值来自落盘文件 ——
  auto res_reloaded{ora::game::settings_load(str_settings_path)};
  check(res_reloaded.has_value(), "saved settings reload");
  if (res_reloaded.has_value()) {
    ora::game::SettingsModel& sm2{*res_reloaded};
    input::HotkeyRegistryModel hotkeys2{*res_hotkeys};
    const auto res_apply{input::hotkey_registry_apply_settings(hotkeys2, {})};
    check(res_apply.has_value(), "registry apply ok");
    // 落盘 Keys 生效路径：从重载模型取 Keys 段应用
    if (const ora::game::MiniYamlNode* ptr_keys{
            ora::game::settings_find_section(sm2, "Keys")}) {
      static_cast<void>(
          input::hotkey_registry_apply_settings(hotkeys2, ptr_keys->vec_children));
      const auto opt_idx{input::hotkey_registry_find(hotkeys2, "CycleBase")};
      check(opt_idx.has_value()
                && input::hotkey_to_string(hotkeys2.vec_keys[*opt_idx]) == "E Ctrl",
            "reloaded settings Keys restore rebinding");
    }
    check(sm2.sound.fp_music_volume == 0.25f && sm2.sound.fp_video_volume == 0.75f,
          "reloaded volumes persist");
  }

  // —— RESET：显示页回默认 ——
  ui::SettingsPanelState st2;
  st2.ptr_settings = &sm;
  st2.ptr_hotkeys = &hotkeys;
  ui::settings_panel_reset(st2, ui::SettingsPanelId::kDisplay);
  check(sm.graphics.mode == ora::game::WindowMode::kPseudoFullscreen
            && sm.graphics.windowed_size.int4_x == 1024
            && sm.graphics.windowed_size.int4_y == 768,
        "reset display restores defaults (VSync 不在上游 ResetPanel 字段集)");
}

// —— 黄金对拍（oracle tree/nav/bindings 三份真源重演） ——

std::vector<std::string> split_lines(const std::string& str_text) {
  std::vector<std::string> vec_out;
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    const std::size_t sz_nl{str_text.find('\n', sz_pos)};
    std::string str_line{str_text.substr(
        sz_pos, sz_nl == std::string::npos ? std::string::npos : sz_nl - sz_pos)};
    if (!str_line.empty() && str_line.back() == '\r') str_line.pop_back();
    if (!str_line.empty() && str_line.front() != '#') vec_out.push_back(std::move(str_line));
    if (sz_nl == std::string::npos) break;
    sz_pos = sz_nl + 1;
  }
  return vec_out;
}

std::optional<std::vector<std::string>> load_golden_lines(std::string_view str_rel) {
  const auto opt_text{ora::test::read_text_file(
      std::string{ORA_MAINMENU_GOLDEN_DIR} + "/" + std::string{str_rel})};
  if (!opt_text.has_value()) return std::nullopt;
  return split_lines(*opt_text);
}

void check_golden_lines(std::string_view str_what, const std::vector<std::string>& vec_produced,
                        std::string_view str_rel) {
  const auto opt_golden{load_golden_lines(str_rel)};
  if (!opt_golden.has_value()) {
    check(false, std::format("{}: golden {} 缺失", str_what, str_rel));
    return;
  }
  const std::size_t sz_n{std::min(vec_produced.size(), opt_golden->size())};
  bool b_same{vec_produced.size() == opt_golden->size()};
  std::size_t sz_first_diff{vec_produced.size()};
  for (std::size_t sz_i{0}; sz_i < sz_n; ++sz_i)
    if (vec_produced[sz_i] != (*opt_golden)[sz_i]) {
      b_same = false;
      sz_first_diff = sz_i;
      break;
    }
  if (b_same) {
    check(true, std::format("{}: golden {} 全等（{} 行）", str_what, str_rel, vec_produced.size()));
    return;
  }
  check(false, std::format("{}: golden {} 不一致（produced {} 行 / golden {} 行）", str_what, str_rel,
                           vec_produced.size(), opt_golden->size()));
  if (sz_first_diff < sz_n)
    std::println("  first diff at line {}: produced [{}] golden [{}]", sz_first_diff + 1,
                 vec_produced[sz_first_diff], (*opt_golden)[sz_first_diff]);
}

// chrome 树 dump 公式（与 oracle DumpChromeWidgetNode / ui.cpp dump_widget_tree_node 同款）
void dump_widget_tree_node(std::string& str_out, const ora::game::MiniYamlNode& node,
                           const ui::WidgetModel* ptr_widget, int int4_depth) {
  const std::string str_indent(static_cast<std::size_t>(int4_depth), '\t');
  str_out += str_indent;
  str_out.append(ui::widget_key_type_part(node.str_key));
  str_out += '@';
  str_out.append(ui::widget_key_id_part(node.str_key));
  if (ptr_widget != nullptr)
    str_out += '\t' + std::to_string(ptr_widget->bounds.int4_x) + ','
        + std::to_string(ptr_widget->bounds.int4_y) + ',' + std::to_string(ptr_widget->bounds.int4_width)
        + ',' + std::to_string(ptr_widget->bounds.int4_height);
  else
    str_out += "\t?";
  str_out += '\n';
  for (const ora::game::MiniYamlNode& child : node.vec_children) {
    if (child.str_key == "Children") continue;
    str_out += str_indent + "\tfield\t" + child.str_key + '\t' + child.str_value + '\n';
  }
  for (const ora::game::MiniYamlNode& child : node.vec_children) {
    if (child.str_key != "Children") continue;
    for (std::size_t sz_i{0}; sz_i < child.vec_children.size(); ++sz_i) {
      const ui::WidgetModel* ptr_child{nullptr};
      if (ptr_widget != nullptr && sz_i < ptr_widget->vec_children.size())
        ptr_child = ptr_widget->vec_children[sz_i].get();
      dump_widget_tree_node(str_out, child.vec_children[sz_i], ptr_child, int4_depth + 1);
    }
  }
}

// tree.txt：五 chrome 语料装载树 bounds 真源（顶层挂哑 root，PARENT=0，窗口 1024x768）
void test_golden_tree() {
  constexpr std::string_view vec_files[]{"mainmenu.yaml", "settings.yaml", "settings-display.yaml",
                                         "settings-audio.yaml", "settings-hotkeys.yaml"};
  std::string str_dump;
  for (const std::string_view str_file : vec_files) {
    const auto vec_nodes{load_nodes(std::string{str_file})};
    if (!vec_nodes.has_value()) return;
    const auto res_loader{ui::widget_loader_from_nodes(*vec_nodes)};
    if (!res_loader.has_value()) {
      check(false, std::format("tree: {} loader: {}", str_file, res_loader.error()));
      return;
    }
    str_dump += std::format("file\t{}\n", str_file);
    for (const ora::game::MiniYamlNode& node_top : *vec_nodes) {
      ui::WidgetModel dumb_root;
      dumb_root.str_type = "Container";
      dumb_root.str_id = "ROOT";
      const auto res_widget{ui::widget_loader_load_widget(*res_loader, dumb_root, menu_ctx(),
                                                          ui::widget_key_id_part(node_top.str_key))};
      if (!res_widget.has_value()) {
        check(false, std::format("tree: {} load: {}", str_file, res_widget.error()));
        return;
      }
      dump_widget_tree_node(str_dump, node_top, dumb_root.vec_children.front().get(), 0);
    }
  }
  check_golden_lines("mainmenu tree", split_lines(str_dump), "tree.txt");
}

// nav.txt：真 MainMenuLogic 导航序列重演（init → 逐步点击 → quit）
void test_golden_nav() {
  auto ptr_tree{load_menu_tree()};
  if (ptr_tree == nullptr) return;
  ui::WidgetModel& root{*ptr_tree};

  ui::MainMenuEnv env{};
  env.b_has_maps = true;
  env.b_has_campaign = true;
  env.b_has_missions = true;
  env.b_load_panel_enabled = false;
  env.b_has_content_installer = true;

  ui::MainMenuState st;
  ui::main_menu_setup(st);
  ui::UiState uist;

  std::string str_dump;
  const auto dump_step{[&](std::string_view str_step) {
    ui::main_menu_apply(st, env, root);
    ui::main_menu_apply_disabled(env, root);
    str_dump += std::format("step\t{}\n", str_step);
    str_dump += std::format("menu\t{}\n", ui::menu_type_name(st.type));
    const auto visible{[&root](std::string_view str_id) {
      const ui::WidgetModel* ptr_w{ui::widget_find_by_id(root, str_id)};
      return ptr_w != nullptr && ptr_w->b_visible ? 1 : 0;
    }};
    str_dump += std::format("visible\tMAIN_MENU={}\tSINGLEPLAYER_MENU={}\tEXTRAS_MENU={}\tMAP_EDITOR_MENU={}\n",
                            visible("MAIN_MENU"), visible("SINGLEPLAYER_MENU"),
                            visible("EXTRAS_MENU"), visible("MAP_EDITOR_MENU"));
    const auto disabled{[&root](std::string_view str_id) {
      const ui::WidgetModel* ptr_w{ui::widget_find_by_id(root, str_id)};
      return ptr_w != nullptr && ptr_w->b_disabled ? 1 : 0;
    }};
    str_dump += std::format("disabled\tSKIRMISH={}\tMISSIONS={}\tLOAD={}\tCONTENT={}\tLOAD_MAP={}\n",
                            disabled("SKIRMISH_BUTTON"), disabled("MISSIONS_BUTTON"),
                            disabled("LOAD_BUTTON"), disabled("CONTENT_BUTTON"),
                            disabled("LOAD_MAP_BUTTON"));
  }};
  const auto drive{[&](std::string_view str_menu, std::string_view str_button) {
    const ui::WidgetModel* ptr_btn{menu_button(root, str_menu, str_button)};
    if (ptr_btn == nullptr) {
      check(false, std::format("nav: {}/{} missing", str_menu, str_button));
      return;
    }
    const auto [x, y]{button_center(*ptr_btn)};
    ui::main_menu_dispatch(st, env, root, click_at(root, uist, x, y));
  }};

  dump_step("init");
  drive("MAIN_MENU", "SINGLEPLAYER_BUTTON");
  dump_step("SINGLEPLAYER_BUTTON");
  drive("SINGLEPLAYER_MENU", "BACK_BUTTON");
  dump_step("BACK_BUTTON(singleplayer)");
  drive("MAIN_MENU", "EXTRAS_BUTTON");
  dump_step("EXTRAS_BUTTON");
  drive("EXTRAS_MENU", "MAP_EDITOR_BUTTON");
  dump_step("MAP_EDITOR_BUTTON");
  drive("MAP_EDITOR_MENU", "BACK_BUTTON");
  dump_step("BACK_BUTTON(map-editor)");
  drive("EXTRAS_MENU", "BACK_BUTTON");
  dump_step("BACK_BUTTON(extras)");
  drive("MAIN_MENU", "QUIT_BUTTON");
  str_dump += std::format("quit\t{}\n", st.b_quit ? "Success" : "?");
  check_golden_lines("mainmenu nav", split_lines(str_dump), "nav.txt");
}

// bindings.txt：三页接线真源重演（滑条/复选框绑定 + display leave 公式 + tab/scroll
// 布局 + ra 全量热键 rekey 流）
void test_golden_bindings() {
  const OpenalDummyGuard guard_dummy_engine;
  std::string str_dump;

  // —— 面板真装载（滑条/复选框初值自树读）——
  const auto vec_settings{load_nodes("settings.yaml")};
  const auto vec_display{load_nodes("settings-display.yaml")};
  const auto vec_audio{load_nodes("settings-audio.yaml")};
  const auto vec_hotkeys_chrome{load_nodes("settings-hotkeys.yaml")};
  if (!vec_settings.has_value() || !vec_display.has_value() || !vec_audio.has_value()
      || !vec_hotkeys_chrome.has_value())
    return;
  const auto res_l_settings{ui::widget_loader_from_nodes(*vec_settings)};
  const auto res_l_display{ui::widget_loader_from_nodes(*vec_display)};
  const auto res_l_audio{ui::widget_loader_from_nodes(*vec_audio)};
  const auto res_l_hotkeys{ui::widget_loader_from_nodes(*vec_hotkeys_chrome)};
  if (!res_l_settings.has_value() || !res_l_display.has_value() || !res_l_audio.has_value()
      || !res_l_hotkeys.has_value()) {
    check(false, "bindings: chrome loaders");
    return;
  }
  auto res_model{ora::game::settings_load("")};
  if (!res_model.has_value()) {
    check(false, "bindings: fresh settings");
    return;
  }
  ora::game::SettingsModel& sm{*res_model};
  auto ptr_sound{std::make_unique<ora::game::sound_facade>(
      ora::game::make_sound_facade_settings("", false, sm.sound))};
  ui::SettingsPanelState st;
  st.ptr_settings = &sm;
  st.ptr_sound = ptr_sound.get();
  ui::WidgetModel panel_root;
  panel_root.str_type = "Container";
  panel_root.str_id = "ROOT";
  panel_root.bounds = ui::WidgetBounds{0, 0, 1024, 768};
  const auto res_open{ui::settings_panel_open(st, panel_root, *res_l_settings, *res_l_display,
                                              *res_l_audio, *res_l_hotkeys, menu_ctx(), "")};
  check(res_open.has_value(), "bindings: panel opens");
  if (!res_open.has_value()) {
    std::println("open error: {}", res_open.error());
    return;
  }

  // —— 滑条绑定链（真设置模型 + T20 门面 dummy 引擎）——
  const ui::WidgetModel* ptr_slider{ui::widget_find_by_id(panel_root, "SOUND_VOLUME")};
  check(ptr_slider != nullptr, "bindings: sound slider wired");
  if (ptr_slider != nullptr)
    str_dump += std::format("slider-bind\tSOUND_VOLUME\tinitial={:.6f}\n", ptr_slider->fp4_value);
  ui::settings_panel_set_volume(st, 0, 0.75f);
  str_dump += std::format("slider-change\tSOUND_VOLUME\t0.75\tfield={:.6f}\n",
                          sm.sound.fp_sound_volume);
  str_dump += std::format("sound-volume-chain\tsetSoundVolume={:.6f}\n",
                          ora::game::sound_facade_sound_volume(*ptr_sound));
  ui::settings_panel_set_volume(st, 1, 0.25f);
  ui::settings_panel_set_volume(st, 2, 0.6f);
  str_dump += std::format("volumes\tSound={:.6f}\tMusic={:.6f}\tVideo={:.6f}\n",
                          sm.sound.fp_sound_volume, sm.sound.fp_music_volume,
                          sm.sound.fp_video_volume);

  // —— MUTE 复选框（翻转 + 引擎主音量 0/1；oracle 序：click dump 后 Mute/Unmute）——
  ui::settings_panel_toggle_mute(st);
  str_dump += std::format("checkbox-bind\tMUTE_SOUND\tchecked={}\tfield.Mute={}\n",
                          sm.sound.b_mute ? 1 : 0, sm.sound.b_mute ? "True" : "False");
  const auto& vec_master_chain{ora::platforms::engine_master_sets(ptr_sound->engine)};
  const float fp4_mute{vec_master_chain.back()};
  ui::settings_panel_toggle_mute(st);
  const float fp4_unmute{vec_master_chain.back()};
  str_dump += std::format("mute-chain\tmaster={:.6f},master={:.6f}\n", fp4_mute, fp4_unmute);

  // —— display leave 公式六例（DisplaySettingsLogic.cs:274-285）——
  const auto display_leave_case{[&](std::string_view str_name,
                                    void (*fn_mutate)(ora::game::GraphicSettingsModel&),
                                    std::string_view str_w, std::string_view str_h) {
    auto res_case{ora::game::settings_load("")};
    ora::game::SettingsModel& sm_case{*res_case};
    ui::SettingsPanelState st_case;
    st_case.ptr_settings = &sm_case;
    st_case.graphics_original = sm_case.graphics;
    fn_mutate(sm_case.graphics);
    ui::settings_panel_set_window_text(st_case, true, str_w);
    ui::settings_panel_set_window_text(st_case, false, str_h);
    const bool b_restart{ui::settings_panel_leave(st_case, ui::SettingsPanelId::kDisplay)};
    str_dump += std::format("display-leave\t{}\twindowed={},{}\trestart={}\n", str_name,
                            sm_case.graphics.windowed_size.int4_x,
                            sm_case.graphics.windowed_size.int4_y, b_restart ? 1 : 0);
  }};
  const auto no_mutate{[](ora::game::GraphicSettingsModel&) {}};
  const auto mutate_mode{[](ora::game::GraphicSettingsModel& g) {
    g.mode = ora::game::WindowMode::kWindowed;
  }};
  const auto mutate_display{[](ora::game::GraphicSettingsModel& g) { g.int4_video_display = 1; }};
  const auto mutate_profile{[](ora::game::GraphicSettingsModel& g) {
    g.gl_profile = ora::game::GlProfile::kModern;
  }};
  display_leave_case("untouched", no_mutate, "1024", "768");
  display_leave_case("mode-windowed", mutate_mode, "1024", "768");
  display_leave_case("size-changed", no_mutate, "1280", "720");
  display_leave_case("bad-text", no_mutate, "not-a-number", "x");
  display_leave_case("video-display", mutate_display, "1024", "768");
  display_leave_case("gl-profile", mutate_profile, "1024", "768");

  // —— tab 布局 + 音频页真树 scroll 布局（同一装载树）——
  const ui::WidgetModel* ptr_tabs{ui::widget_find_by_id(panel_root, "SETTINGS_TAB_CONTAINER")};
  if (ptr_tabs == nullptr || ptr_tabs->vec_children.size() < 3) {
    check(false, "bindings: tab container");
    return;
  }
  const int int4_stride_x{ptr_tabs->vec_children[1]->bounds.int4_x
                          - ptr_tabs->vec_children[0]->bounds.int4_x};
  const int int4_stride_y{ptr_tabs->vec_children[1]->bounds.int4_y
                          - ptr_tabs->vec_children[0]->bounds.int4_y};
  str_dump += std::format("tab-stride\t{},{}\n", int4_stride_x, int4_stride_y);
  for (std::size_t sz_i{0}; sz_i < 3; ++sz_i)
    str_dump += std::format("tab\t{}\t{},{}\n", sz_i, ptr_tabs->vec_children[sz_i]->bounds.int4_x,
                            ptr_tabs->vec_children[sz_i]->bounds.int4_y);

  // 三页各有 SETTINGS_SCROLLPANEL，且 tab 克隆与页面克隆同名（AUDIO_PANEL）——
  // 经 PANEL_CONTAINER 容器限定到音频页（oracle 对拍的是 settings-audio.yaml 行集）
  ui::WidgetModel* ptr_scroll{nullptr};
  if (ui::WidgetModel* ptr_panels{ui::widget_find_by_id(panel_root, "PANEL_CONTAINER")})
    if (ui::WidgetModel* ptr_audio{
            ui::widget_find_by_id(*ptr_panels, settings_panel_id_name(ui::SettingsPanelId::kAudio))})
      ptr_scroll = ui::widget_find_by_id(*ptr_audio, "SETTINGS_SCROLLPANEL");
  if (ptr_scroll != nullptr) {
    const int int4_content{ui::settings_panel_scrollpanel_layout(*ptr_scroll)};
    for (const auto& ptr_row : ptr_scroll->vec_children)
      str_dump += std::format("scroll-row\ty={}\th={}\n", ptr_row->bounds.int4_y,
                              ptr_row->bounds.int4_height);
    str_dump += std::format("scroll-content-height\t{}\n", int4_content);
  }

  // —— ra 全量热键注册表（tests/golden/hotkeys 语料）——
  const std::string str_hotkeys_dir{std::string{ORA_MAINMENU_GOLDEN_DIR} + "/../hotkeys"};
  const auto opt_files{ora::test::read_text_file(str_hotkeys_dir + "/files.txt")};
  check(opt_files.has_value(), "bindings: hotkeys files.txt readable");
  std::vector<std::vector<ora::game::MiniYamlNode>> vec_hotkey_files;
  if (opt_files.has_value()) {
    for (const std::string& str_line : split_lines(*opt_files)) {
      if (str_line.empty()) continue;
      std::string str_local{str_line};
      std::replace(str_local.begin(), str_local.end(), '|', '-');
      std::replace(str_local.begin(), str_local.end(), '/', '-');
      auto res_nodes{ora::game::miniyaml_from_file(str_hotkeys_dir + "/yaml/" + str_local)};
      if (!res_nodes.has_value()) {
        check(false, std::format("bindings: hotkey corpus {}", str_line));
        return;
      }
      vec_hotkey_files.push_back(std::move(*res_nodes));
    }
  }
  const auto res_reg{input::hotkey_registry_load(vec_hotkey_files)};
  if (!res_reg.has_value()) {
    check(false, std::format("bindings: registry: {}", res_reg.error()));
    return;
  }
  input::HotkeyRegistryModel reg{*res_reg};
  str_dump += std::format("hotkey-definitions\t{}\n", reg.vec_definitions.size());
  const auto row_of{[&reg](std::string_view str_name) {
    const auto opt_idx{input::hotkey_registry_find(reg, str_name)};
    check(opt_idx.has_value(), std::format("bindings: hotkey {} defined", str_name));
    return opt_idx.value_or(0);
  }};
  for (const std::string_view str_name :
       {"CycleBase", "ToLastEvent", "ToSelection", "ToggleChatMode"}) {
    const std::size_t sz_idx{row_of(str_name)};
    const input::HotkeyDefinitionModel& def{reg.vec_definitions[sz_idx]};
    str_dump += std::format("hotkey-row\t{}\t{}\t{}\tdefault-same={}\treadonly={}\n", def.str_name,
                            input::hotkey_to_string(reg.vec_keys[sz_idx]),
                            input::hotkey_to_string(def.hk_default),
                            reg.vec_keys[sz_idx] == def.hk_default ? 1 : 0, def.b_readonly ? 1 : 0);
  }

  // rekey 流（HotkeyManager.Set / hotkeySettings 节点语义）
  static_cast<void>(input::hotkey_set(reg, "CycleBase",
                                      input::HotkeyModel{input::Keycode::kE,
                                                         input::ModifierKeys::kCtrl}));
  str_dump += std::format("hotkey-rekey\tCycleBase\tE Ctrl\tcurrent={}\n",
                          input::hotkey_to_string(
                              reg.vec_keys[input::hotkey_registry_find(reg, "CycleBase").value()]));
  for (const auto& [str_key, str_value] : reg.vec_settings)
    str_dump += std::format("hotkey-settings-node\t{}\t{}\n", str_key, str_value);
  static_cast<void>(input::hotkey_set(
      reg, "CycleBase",
      reg.vec_definitions[input::hotkey_registry_find(reg, "CycleBase").value()].hk_default));
  str_dump += std::format("hotkey-rekey-default\tCycleBase\tnodes={}\n", reg.vec_settings.size());
  static_cast<void>(input::hotkey_set(
      reg, "ToggleChatMode", input::HotkeyModel{input::Keycode::kE, input::ModifierKeys::kNone}));
  str_dump += std::format("hotkey-readonly\tToggleChatMode\tcurrent={}\n",
                          input::hotkey_to_string(
                              reg.vec_keys[input::hotkey_registry_find(reg, "ToggleChatMode").value()]));
  static_cast<void>(input::hotkey_set(reg, "ToLastEvent",
                                      input::HotkeyModel{input::Keycode::kHome,
                                                         input::ModifierKeys::kNone}));
  static_cast<void>(input::hotkey_set(reg, "ToSelection",
                                      input::HotkeyModel{input::Keycode::kHome,
                                                         input::ModifierKeys::kNone}));
  const auto opt_dup{input::hotkey_get_first_duplicate(
      reg, input::hotkey_registry_find(reg, "ToSelection").value())};
  str_dump += std::format("hotkey-duplicate\tToSelection\t{}\n",
                          opt_dup.has_value() ? reg.vec_definitions[*opt_dup].str_name : "none");
  static_cast<void>(input::hotkey_set(
      reg, "ToSelection",
      reg.vec_definitions[input::hotkey_registry_find(reg, "ToSelection").value()].hk_default));
  str_dump += std::format(
      "hotkey-duplicate-cleared\tToLastEvent\t{}\n",
      reg.vec_definitions[input::hotkey_registry_find(reg, "ToLastEvent").value()].b_has_duplicates
          ? 1
          : 0);

  check_golden_lines("mainmenu bindings", split_lines(str_dump), "bindings.txt");
}

void run() {
  test_menu_navigation();
  test_settings_panel();
  test_golden_tree();
  test_golden_nav();
  test_golden_bindings();
}

}  // namespace

int main() {
  run();
  return ora::test::test_exit_code("mainmenu");
}
