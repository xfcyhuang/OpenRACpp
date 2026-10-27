#pragma once
import std;

#include "game/input.hpp"
#include "net/tick_time.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class Viewport;
class Renderer;
class ChromeProvider;
class SpriteFont;
class WorldRenderer;
}

namespace ora::sound {
class Sound;
}

namespace ora::fmt {
struct IVideo;
}

namespace ora::game {
class Ruleset;
}

namespace ora::net {
class ConnectionTarget;
class NetworkConnection;
class OrderManager;
}

namespace ora::map {
class MapCache;
}

namespace ora {
class MersenneTwister;
}

namespace ora::ui {

struct ServerSettingsFace {
  std::string Name;
  int ListenPort = 1234;
  bool AdvertiseOnline = true;
  bool AdvertiseOnLocalNetwork = true;
  std::string Password;
  bool DiscoverNatDevices = false;
  std::string Map;
};

class Ui {
 public:
  static constexpr std::int32_t kTimestep = 40;

  static Ui& Instance();
  static bool HasInstance();

  Ui();
  ~Ui();

  Ui(const Ui&) = delete;
  Ui& operator=(const Ui&) = delete;

  struct Deps {
    game::ModData* ptr_mod_data = nullptr;

    std::function<Widget*(WidgetArgs&, Widget*, const std::string&)> fn_load_widget;

    std::function<std::unique_ptr<Widget>(Widget*)> fn_release_root;
    std::function<int2()> fn_window_size;
    std::function<std::int64_t()> fn_clock;
    ChromeMetrics* ptr_chrome_metrics = nullptr;

    gfx::Renderer* ptr_renderer = nullptr;

    gfx::ChromeProvider* ptr_chrome_provider = nullptr;

    std::function<void(const game::Ruleset*, std::string_view)> fn_play_notification;

    std::function<float()> fn_ui_scroll_speed = [] { return 50.0f; };

    std::function<bool()> fn_cursor_double = [] { return false; };

    std::function<HotkeyReference(std::string_view)> fn_hotkey_lookup =
        [](std::string_view str_name) { return HotkeyReferenceFromNameOrCode(str_name); };

    std::function<gfx::SpriteFont*(const std::string&)> fn_try_get_font;

    std::function<std::vector<core::Color>()> fn_get_custom_colors = [] {
      return std::vector<core::Color>{};
    };

    std::function<void(const std::vector<core::Color>&)> fn_store_custom_colors =
        [](const std::vector<core::Color>&) {};

    std::function<int()> fn_debug_samples = [] { return 25; };
    std::function<void(std::function<void()>)> fn_run_after_tick =
        [](std::function<void()>) {};
    std::function<Modifiers()> fn_get_modifier_keys = [] { return Modifiers::None; };
    std::function<bool()> fn_window_has_input_focus = [] { return true; };
    std::function<GameSettingsInputFace()> fn_game_settings_input = [] {
      return GameSettingsInputFace{};
    };

    std::function<bool(std::string_view)> fn_cursor_exists =
        [](std::string_view) { return false; };

    std::function<int()> fn_local_tick = [] { return 0; };

    std::function<std::int32_t()> fn_render_frame = [] { return 0; };

    std::function<std::function<void()>(std::function<void()>)>
        fn_add_after_game_start = [](std::function<void()>) {
          return std::function<void()>{};
        };

    std::function<sound::Sound*()> fn_sound = [] { return static_cast<sound::Sound*>(nullptr); };

    std::function<std::unique_ptr<fmt::IVideo>(std::string_view, bool)>
        fn_load_video;

    std::function<void()> fn_cursor_lock = [] {};
    std::function<void()> fn_cursor_unlock = [] {};

    std::function<bool()> fn_cash_ticks = [] { return true; };
    std::function<void(std::int32_t, std::function<void()>)>
        fn_run_after_delay = [](std::int32_t, std::function<void()> fn_a) {
          fn_a();
        };

    std::function<bool()> fn_hide_cursor = [] { return false; };
    std::function<void(bool)> fn_set_hide_cursor = [](bool) {};

    std::function<void()> fn_disconnect = [] {};
    std::function<void()> fn_load_shell_map = [] {};
    std::function<void()> fn_restart_game = [] {};
    std::function<void(const std::string&)> fn_load_editor =
        [](const std::string&) {};
    std::function<bool(const sim::World*)> fn_is_current_world =
        [](const sim::World*) { return true; };

    std::function<void()> fn_exit = [] {};

    std::function<std::function<void()>(
        std::function<void(const net::ConnectionTarget&)>)>
        fn_subscribe_on_remote_direct_connect =
            [](std::function<void(const net::ConnectionTarget&)>) {
              return std::function<void()>{};
            };
    std::function<std::function<void()>(std::function<void()>)>
        fn_subscribe_before_game_start =
            [](std::function<void()>) { return std::function<void()>{}; };
    std::function<std::function<void()>(std::function<void()>)>
        fn_subscribe_on_shellmap_loaded =
            [](std::function<void()>) { return std::function<void()>{}; };

    std::function<std::function<void()>(
        std::function<void(net::OrderManager*, const std::string&,
                           net::NetworkConnection*)>)>
        fn_subscribe_connection_state_changed =
            [](std::function<void(net::OrderManager*, const std::string&,
                                  net::NetworkConnection*)>) {
              return std::function<void()>{};
            };

    std::function<void(const std::string&)> fn_join_replay =
        [](const std::string&) {};
    std::function<bool()> fn_check_version = [] { return false; };
    std::function<void(const std::string&, const std::string&,
                       std::function<void()>)>
        fn_fetch_news = [](const std::string&, const std::string&,
                           std::function<void()> fn_on_fetched) {
          fn_on_fetched();
        };

    std::function<std::string(const std::string&, bool)> fn_create_local_server =
        [](const std::string& str_uid, bool) { return str_uid; };
    std::function<void()> fn_close_server = [] {};

    std::function<void(const std::string&, const std::vector<std::string>&)>
        fn_start_mission_server =
            [](const std::string&, const std::vector<std::string>&) {};

    std::function<MersenneTwister*()> fn_cosmetic_random =
        [] { return static_cast<MersenneTwister*>(nullptr); };

    std::function<void(const std::string&, const std::string&,
                       std::function<void()>, std::function<void()>)>
        fn_connection_connect =
            [](const std::string&, const std::string&,
               std::function<void()> fn_on_connected,
               std::function<void()>) { fn_on_connected(); };
    std::function<bool()> fn_should_show_introduction_prompt =
        [] { return false; };
    std::function<bool()> fn_should_show_system_info_prompt =
        [] { return false; };
    std::function<bool()> fn_settings_fetch_news = [] { return true; };
    std::function<std::string()> fn_server_map_get =
        [] { return std::string{}; };
    std::function<void(const std::string&)> fn_server_map_set =
        [](const std::string&) {};
    std::function<void()> fn_settings_save = [] {};
    std::function<std::string()> fn_create_parameter_string =
        [] { return std::string{}; };
    std::function<void(const std::string&)> fn_write_news_cache =
        [](const std::string&) {};
    std::function<int()> fn_mp_game_filters_get = [] { return 15; };
    std::function<void(int)> fn_mp_game_filters_set = [](int) {};
    std::function<bool()> fn_allow_downloading = [] { return true; };
    std::function<std::string()> fn_player_last_server_get = [] {
      return std::string{"localhost:1234"};
    };
    std::function<void(const std::string&)> fn_player_last_server_set =
        [](const std::string&) {};
    std::function<void(const net::ConnectionTarget&, const std::string&)>
        fn_join_server = [](const net::ConnectionTarget&, const std::string&) {
        };
    std::function<std::string()> fn_engine_version = [] {
      return std::string{"dev"};
    };
    std::function<std::string()> fn_session_guid = [] {
      return std::string{};
    };
    std::function<void(const std::string&,
                       std::function<void(bool, std::string)>)>
        fn_fetch_server_list =
            [](const std::string&,
               std::function<void(bool, std::string)> fn_done) {
              fn_done(false, std::string{});
            };

    std::function<ServerSettingsFace()> fn_server_settings_get = [] {
      return ServerSettingsFace{};
    };
    std::function<void(const ServerSettingsFace&)> fn_server_settings_set =
        [](const ServerSettingsFace&) {};

    std::function<std::string()> fn_player_name = [] {
      return std::string{"Commander"};
    };

    std::function<void(const std::string&)> fn_player_name_set =
        [](const std::string&) {};

    std::function<std::function<void()>(std::function<void()>)>
        fn_subscribe_lobby_info_changed =
            [](std::function<void()>) { return std::function<void()>{}; };

    std::function<bool(const ServerSettingsFace&, int&, std::string&,
                       net::ConnectionTarget&)>
        fn_create_server = [](const ServerSettingsFace&, int& int4_code,
                              std::string& str_message,
                              net::ConnectionTarget&) {
          int4_code = 0;
          str_message = "CreateServer is not wired.";
          return false;
        };

    std::function<bool()> fn_is_host = [] { return false; };

    std::function<core::Color()> fn_player_color_get = [] {
      return core::Color{};
    };
    std::function<void(core::Color)> fn_player_color_set = [](core::Color) {};

    std::function<void(Widget&, core::Color, const std::string*,
                       gfx::WorldRenderer*, std::function<void(core::Color)>)>
        fn_show_color_dropdown =
            [](Widget&, core::Color, const std::string*,
               gfx::WorldRenderer*, std::function<void(core::Color)>) {};

    std::function<std::vector<std::string>()> fn_asset_browser_palettes =
        [] { return std::vector<std::string>{}; };
    std::function<std::vector<std::string>()>
        fn_asset_browser_color_picker_palettes = [] {
          return std::vector<std::string>{};
        };
    map::MapCache* ptr_map_cache = nullptr;

    std::function<Widget*(sim::World&, const std::string&, Widget*,
                         WidgetArgs&)>
        fn_game_load_widget;
    std::function<void()> fn_take_screenshot = [] {};
  };

  void Initialize(Deps deps);
  const Deps& DepsForTest() const { return deps_; }

  void WireModData(
      game::ModData* ptr_mod_data,
      std::function<Widget*(WidgetArgs&, Widget*,
                            const std::string&)> fn_load_widget,
      std::function<Widget*(sim::World&, const std::string&, Widget*,
                            WidgetArgs&)> fn_game_load_widget = {});
  void WireMapCache(map::MapCache* ptr_map_cache);

  gfx::Renderer* RendererDeps() const { return deps_.ptr_renderer; }
  gfx::ChromeProvider* ChromeProviderDeps() const { return deps_.ptr_chrome_provider; }
  ChromeMetrics* ChromeMetricsDeps() const { return deps_.ptr_chrome_metrics; }
  game::ModData* ModDataDeps() const { return deps_.ptr_mod_data; }
  void PlayNotification(const game::Ruleset* ptr_rules, std::string_view str_notification) const {
    if (deps_.fn_play_notification)
      deps_.fn_play_notification(ptr_rules, str_notification);
  }
  float UiScrollSpeed() const { return deps_.fn_ui_scroll_speed(); }
  bool CursorDouble() const { return deps_.fn_cursor_double(); }
  bool CursorExists(std::string_view str_name) const {
    return deps_.fn_cursor_exists(str_name);
  }
  int LocalTick() const { return deps_.fn_local_tick(); }
  sound::Sound* SoundDeps() const { return deps_.fn_sound(); }
  void LockCursor() const { deps_.fn_cursor_lock(); }
  void UnlockCursor() const { deps_.fn_cursor_unlock(); }
  bool CashTicks() const { return deps_.fn_cash_ticks(); }
  std::function<std::unique_ptr<fmt::IVideo>(std::string_view, bool)>
  LoadVideoDeps() const {
    return deps_.fn_load_video;
  }
  int DebugSamples() const { return deps_.fn_debug_samples(); }
  Modifiers GetModifierKeys() const { return deps_.fn_get_modifier_keys(); }
  bool WindowHasInputFocus() const { return deps_.fn_window_has_input_focus(); }
  GameSettingsInputFace GameSettingsInput() const {
    return deps_.fn_game_settings_input();
  }
  std::int32_t SelectionDeadzone() const {
    return deps_.fn_game_settings_input().int4_selection_deadzone;
  }
  void RunAfterTickDefer(std::function<void()> fn_action) const {
    deps_.fn_run_after_tick(std::move(fn_action));
  }
  void RunAfterDelayDefer(std::int32_t int4_delay_ms,
                          std::function<void()> fn_action) const {
    deps_.fn_run_after_delay(int4_delay_ms, std::move(fn_action));
  }
  bool HideCursor() const { return deps_.fn_hide_cursor(); }
  void SetHideCursor(bool b_value) const { deps_.fn_set_hide_cursor(b_value); }
  bool IsCurrentWorldDeps(const sim::World* ptr_world) const {
    return deps_.fn_is_current_world(ptr_world);
  }
  void DisconnectDeps() const { deps_.fn_disconnect(); }
  void LoadShellMapDeps() const { deps_.fn_load_shell_map(); }
  void RestartGameDeps() const { deps_.fn_restart_game(); }
  void LoadEditorDeps(const std::string& str_uid) const {
    deps_.fn_load_editor(str_uid);
  }
  void ExitDeps() const { deps_.fn_exit(); }
  bool SettingsFetchNewsDeps() const {
    return deps_.fn_settings_fetch_news();
  }
  bool CheckVersionDeps() const { return deps_.fn_check_version(); }
  bool ShouldShowIntroductionPromptDeps() const {
    return deps_.fn_should_show_introduction_prompt();
  }
  bool ShouldShowSystemInfoPromptDeps() const {
    return deps_.fn_should_show_system_info_prompt();
  }
  std::string CreateParameterStringDeps() const {
    return deps_.fn_create_parameter_string();
  }
  void FetchNewsDeps(const std::string& str_url,
                     const std::string& str_cache_file,
                     std::function<void()> fn_on_fetched) const {
    deps_.fn_fetch_news(str_url, str_cache_file, std::move(fn_on_fetched));
  }
  std::string ServerMapGetDeps() const { return deps_.fn_server_map_get(); }
  void ServerMapSetDeps(const std::string& str_uid) const {
    deps_.fn_server_map_set(str_uid);
  }
  void SettingsSaveDeps() const { deps_.fn_settings_save(); }
  std::string CreateLocalServerDeps(const std::string& str_uid,
                                    bool b_skirmish) const {
    return deps_.fn_create_local_server(str_uid, b_skirmish);
  }
  void CloseServerDeps() const { deps_.fn_close_server(); }
  void StartMissionServerDeps(
      const std::string& str_uid,
      const std::vector<std::string>& vec_orders) const {
    deps_.fn_start_mission_server(str_uid, vec_orders);
  }
  MersenneTwister* CosmeticRandomDeps() const {
    return deps_.fn_cosmetic_random();
  }
  void ConnectionConnectDeps(const std::string& str_endpoint,
                             const std::string& str_password,
                             std::function<void()> fn_on_connected,
                             std::function<void()> fn_on_abort) const {
    deps_.fn_connection_connect(str_endpoint, str_password,
                                std::move(fn_on_connected),
                                std::move(fn_on_abort));
  }
  std::function<void()> SubscribeOnRemoteDirectConnect(
      std::function<void(const net::ConnectionTarget&)> fn_handler) const {
    return deps_.fn_subscribe_on_remote_direct_connect(std::move(fn_handler));
  }
  std::function<void()> SubscribeBeforeGameStart(
      std::function<void()> fn_handler) const {
    return deps_.fn_subscribe_before_game_start(std::move(fn_handler));
  }
  std::function<void()> SubscribeOnShellmapLoaded(
      std::function<void()> fn_handler) const {
    return deps_.fn_subscribe_on_shellmap_loaded(std::move(fn_handler));
  }
  std::function<void()> SubscribeConnectionStateChanged(
      std::function<void(net::OrderManager*, const std::string&,
                         net::NetworkConnection*)> fn_handler) const {
    return deps_.fn_subscribe_connection_state_changed(std::move(fn_handler));
  }
  void JoinReplayDeps(const std::string& str_path) const {
    deps_.fn_join_replay(str_path);
  }
  map::MapCache* MapCacheDeps() const { return deps_.ptr_map_cache; }
  HotkeyReference Hotkey(std::string_view str_name) const {
    return deps_.fn_hotkey_lookup(str_name);
  }
  std::int64_t RunTime() const { return deps_.fn_clock ? deps_.fn_clock() : 0; }
  int2 Resolution() const { return deps_.fn_window_size ? deps_.fn_window_size() : int2{}; }
  HotkeyReference HotkeyLookup(std::string_view str_name) const {
    return deps_.fn_hotkey_lookup(str_name);
  }
  gfx::SpriteFont* TryGetFontDeps(const std::string& str_key) const {
    return deps_.fn_try_get_font ? deps_.fn_try_get_font(str_key) : nullptr;
  }
  std::vector<core::Color> CustomColorsDeps() const { return deps_.fn_get_custom_colors(); }
  void StoreCustomColorsDeps(const std::vector<core::Color>& vec_colors) const {
    deps_.fn_store_custom_colors(vec_colors);
  }
  int MPGameFiltersGetDeps() const { return deps_.fn_mp_game_filters_get(); }
  void MPGameFiltersSetDeps(int int4_filters) const {
    deps_.fn_mp_game_filters_set(int4_filters);
  }
  bool AllowDownloadingDeps() const { return deps_.fn_allow_downloading(); }
  std::string PlayerLastServerGetDeps() const {
    return deps_.fn_player_last_server_get();
  }
  void PlayerLastServerSetDeps(const std::string& str_server) const {
    deps_.fn_player_last_server_set(str_server);
  }
  void JoinServerDeps(const net::ConnectionTarget& target_endpoint,
                      const std::string& str_password) const {
    deps_.fn_join_server(target_endpoint, str_password);
  }
  std::string EngineVersionDeps() const { return deps_.fn_engine_version(); }
  std::string SessionGuidDeps() const { return deps_.fn_session_guid(); }
  void FetchServerListDeps(
      const std::string& str_url,
      std::function<void(bool, std::string)> fn_done) const {
    deps_.fn_fetch_server_list(str_url, std::move(fn_done));
  }
  ServerSettingsFace ServerSettingsGetDeps() const {
    return deps_.fn_server_settings_get();
  }
  void ServerSettingsSetDeps(const ServerSettingsFace& face_server) const {
    deps_.fn_server_settings_set(face_server);
  }
  std::string PlayerNameDeps() const { return deps_.fn_player_name(); }
  void PlayerNameSetDeps(const std::string& str_name) const {
    deps_.fn_player_name_set(str_name);
  }
  std::function<void()> SubscribeLobbyInfoChanged(
      std::function<void()> fn_handler) const {
    return deps_.fn_subscribe_lobby_info_changed(std::move(fn_handler));
  }
  bool CreateServerDeps(const ServerSettingsFace& face_server,
                        int& int4_error_code, std::string& str_error_message,
                        net::ConnectionTarget& target_endpoint) const {
    return deps_.fn_create_server(face_server, int4_error_code,
                                  str_error_message, target_endpoint);
  }
  bool IsHostDeps() const { return deps_.fn_is_host(); }
  core::Color PlayerColorGetDeps() const {
    return deps_.fn_player_color_get();
  }
  void PlayerColorSetDeps(core::Color color_value) const {
    deps_.fn_player_color_set(color_value);
  }
  void ShowColorDropDownDeps(
      Widget& widget_dropdown, core::Color color_initial,
      const std::string* str_faction, gfx::WorldRenderer* ptr_world_renderer,
      std::function<void(core::Color)> fn_on_exit) const {
    deps_.fn_show_color_dropdown(widget_dropdown, color_initial, str_faction,
                                 ptr_world_renderer, std::move(fn_on_exit));
  }
  std::vector<std::string> AssetBrowserPalettesDeps() const {
    return deps_.fn_asset_browser_palettes();
  }
  std::vector<std::string> AssetBrowserColorPickerPalettesDeps() const {
    return deps_.fn_asset_browser_color_picker_palettes();
  }

  Widget* MouseFocusWidget() const { return ptr_mouse_focus_; }
  void SetMouseFocusWidget(Widget* ptr_widget) { ptr_mouse_focus_ = ptr_widget; }
  Widget* KeyboardFocusWidget() const { return ptr_keyboard_focus_; }
  void SetKeyboardFocusWidget(Widget* ptr_widget) { ptr_keyboard_focus_ = ptr_widget; }
  Widget* MouseOverWidget() const { return ptr_mouse_over_; }
  void SetMouseOverWidget(Widget* ptr_widget) { ptr_mouse_over_ = ptr_widget; }

  Mediator& mediator() { return mediator_; }

  Widget* Root() const { return ptr_root_.get(); }

  net::TickTime& LastTickTime() { return last_tick_time_; }

  void CloseWindow();

  Widget* OpenWindow(const std::string& str_id, WidgetArgs args_args);
  Widget* OpenWindow(const std::string& str_id) { return OpenWindow(str_id, WidgetArgs{}); }

  Widget* CurrentWindow() const;

  Widget* LoadWidget(const std::string& str_id, Widget* ptr_parent, WidgetArgs args_args);

  std::unique_ptr<Widget> ReleaseWidget(Widget* ptr_widget) {
    if (deps_.fn_release_root)
      return deps_.fn_release_root(ptr_widget);
    return nullptr;
  }

  void Tick() { ptr_root_->TickOuter(); }
  void PrepareRenderables() { ptr_root_->PrepareRenderablesOuter(); }

  bool WidgetsVisible() const { return b_widgets_visible_; }
  void SetWidgetsVisible(bool b_value) { b_widgets_visible_ = b_value; }

  void Draw();

  bool HandleInput(const MouseInput& mi_input);

  bool HandleKeyPress(const KeyInput& e_input);

  bool HandleTextInput(std::string_view str_text);

  void ResetAll();

  void ResetTooltips();

  template <class T>
  void Subscribe(INotificationHandler<T>& handler_instance) {
    mediator_.Subscribe(handler_instance);
  }

  template <class T>
  void Unsubscribe(INotificationHandler<T>& handler_instance) {
    mediator_.Unsubscribe(handler_instance);
  }

  template <class T>
  void Send(const T& notification_notification) {
    mediator_.Send(notification_notification);
  }

 private:
  struct WindowEntry {
    Widget* ptr_widget = nullptr;
    std::unique_ptr<Widget> up_owned;
  };

  Mediator mediator_;
  std::unique_ptr<Widget> ptr_root_;
  net::TickTime last_tick_time_;

  Widget* ptr_mouse_focus_ = nullptr;
  Widget* ptr_keyboard_focus_ = nullptr;
  Widget* ptr_mouse_over_ = nullptr;

  std::vector<WindowEntry> vec_windows_;
  bool b_widgets_visible_ = true;

  Deps deps_;
};

}
