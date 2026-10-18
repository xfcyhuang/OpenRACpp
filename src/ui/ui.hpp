// UPSTREAM: OpenRA.Game/Widgets/Widget.cs @e180e3225 L24-194(Ui 静态门面逐语义:
// 窗口栈/焦点三指针/输入分发/ResetAll/Tick/Draw/Mediator 转发)。C# static
// → 进程级单例;窗口栈条目在 widget 被 HideChild 摘出 Root 时接管所有权
// (可见栈顶由 Root 持有);WidgetLoader/Renderer.Resolution/Game.RunTime/
// ModData → Deps 注入;LastMousePos/LastMoveRunTime 写 gfx::Viewport 同名
// 静态(Viewport.cs L134-135)。
// Verbatim-semantics rewrite of the Ui static facade (the window stack /
// the focus trio / the input dispatch / ResetAll / Tick / Draw / the
// Mediator forwarding). C#'s static class becomes the process singleton;
// a stack entry takes ownership when its widget is HideChild'ed out of
// Root (the visible top is Root-owned); the WidgetLoader /
// Renderer.Resolution / Game.RunTime / ModData faces arrive via Deps;
// LastMousePos/LastMoveRunTime write the same-named gfx::Viewport statics
// (Viewport.cs L134-135).
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

namespace ora::ui {

class Ui {
 public:
  static constexpr std::int32_t kTimestep = 40;  // Widget.cs L26

  /// 进程级单例(Widget 家族静态访问面) | the process-wide singleton (the
  /// Widget family's static access face).
  static Ui& Instance();
  static bool HasInstance();

  Ui();
  ~Ui();

  Ui(const Ui&) = delete;
  Ui& operator=(const Ui&) = delete;

  struct Deps {
    game::ModData* ptr_mod_data = nullptr;
    /// WidgetLoader.LoadWidget 回调;返回值由 parent 持有(上游同构)
    /// the WidgetLoader.LoadWidget callback; the return value is owned by
    /// parent (isomorphic to upstream).
    std::function<Widget*(WidgetArgs&, Widget*, const std::string&)> fn_load_widget;
    /// WidgetLoader.ReleaseRoot 回调(无 parent 根的所有权移交;未接线 =
    /// 返回 null = 调用方自担)
    /// the WidgetLoader.ReleaseRoot callback (the parentless-root ownership
    /// handover; unwired = null = the caller fends for itself).
    std::function<std::unique_ptr<Widget>(Widget*)> fn_release_root;
    std::function<int2()> fn_window_size;    // Renderer.Resolution
    std::function<std::int64_t()> fn_clock;  // Game.RunTime(缺省恒 0 | 0 by default)
    ChromeMetrics* ptr_chrome_metrics = nullptr;
    /// Game.Renderer 静态面(字体/渲染器/scissor;widget 族绘制域)
    /// the Game.Renderer static face (fonts/renderers/scissor; the widget
    /// family's drawing domain).
    gfx::Renderer* ptr_renderer = nullptr;
    /// ChromeProvider 静态面(chrome.yaml 图像/面板)
    /// the ChromeProvider static face (chrome.yaml images/panels).
    gfx::ChromeProvider* ptr_chrome_provider = nullptr;
    /// Game.Sound.PlayNotification(modRules, null, "Sounds", name, null) 面
    /// the Game.Sound.PlayNotification(modRules, null, "Sounds", name, null)
    /// face.
    std::function<void(const game::Ruleset*, std::string_view)> fn_play_notification;
    /// Game.Settings.Game.UIScrollSpeed(缺省 50;Settings.cs L330)
    /// Game.Settings.Game.UIScrollSpeed (default 50; Settings.cs L330).
    std::function<float()> fn_ui_scroll_speed = [] { return 50.0f; };
    /// Game.Settings.Graphics.CursorDouble(缺省 false;Settings.cs L249;
    /// TooltipContainerWidget 的光标缩放消费)
    /// Game.Settings.Graphics.CursorDouble (default false; Settings.cs
    /// L249; consumed by TooltipContainerWidget's cursor scaling).
    std::function<bool()> fn_cursor_double = [] { return false; };
    /// Game.ModData.Hotkeys[name](FieldLoader.ParseHotkeyReference;缺省 =
    /// GetHotkeyReference 尾段回落)
    /// Game.ModData.Hotkeys[name] (FieldLoader.ParseHotkeyReference;
    /// defaulting to GetHotkeyReference's tail fallback).
    std::function<HotkeyReference(std::string_view)> fn_hotkey_lookup =
        [](std::string_view str_name) { return HotkeyReferenceFromNameOrCode(str_name); };
    /// Game.Renderer.Fonts[name] 的直连测试面(生产走 ptr_renderer;未接线 =
    /// null = 缺字体,widget 族统一经 widget_utils::RequireWidgetFont)
    /// Game.Renderer.Fonts[name]'s direct test face (production goes through
    /// ptr_renderer; unwired = null = missing font — the widget family
    /// funnels through widget_utils::RequireWidgetFont).
    std::function<gfx::SpriteFont*(const std::string&)> fn_try_get_font;
    /// Game.Settings.Player.CustomColors 的读面(ColorPickerLogic;未接线 =
    /// 空表)
    /// Game.Settings.Player.CustomColors' read face (ColorPickerLogic;
    /// unwired = the empty list).
    std::function<std::vector<core::Color>()> fn_get_custom_colors = [] {
      return std::vector<core::Color>{};
    };
    /// Game.Settings.Player.CustomColors 写 + Game.Settings.Save() 的落盘面
    /// the CustomColors write + Game.Settings.Save() persistence face.
    std::function<void(const std::vector<core::Color>&)> fn_store_custom_colors =
        [](const std::vector<core::Color>&) {};
    /// Game.Settings.Debug.Samples(缺省 25;Settings.cs L197;PerfGraph 的
    /// Average 计数消费)
    /// Game.Settings.Debug.Samples (default 25; Settings.cs L197; consumed by
    /// PerfGraph's Average count).
    std::function<int()> fn_debug_samples = [] { return 25; };
    std::function<void(std::function<void()>)> fn_run_after_tick =
        [](std::function<void()>) {};
    std::function<Modifiers()> fn_get_modifier_keys = [] { return Modifiers::None; };
    std::function<bool()> fn_window_has_input_focus = [] { return true; };
    std::function<GameSettingsInputFace()> fn_game_settings_input = [] {
      return GameSettingsInputFace{};
    };
    /// Game.ModData.Cursors.ContainsKey(name)(RadarWidget 的 -minimap 后缀
    /// 解析;未接线 = false = 恒裸名)
    /// Game.ModData.Cursors.ContainsKey(name) (RadarWidget's -minimap
    /// suffix resolution; unwired = false = always the bare name).
    std::function<bool(std::string_view)> fn_cursor_exists =
        [](std::string_view) { return false; };
    /// Game.LocalTick(OrderManager.LocalFrameNumber;SupportPowerTimer 的
    /// 闪烁节拍;未接线 = 0)
    /// Game.LocalTick (OrderManager.LocalFrameNumber; SupportPowerTimer's
    /// blink beat; unwired = 0).
    std::function<int()> fn_local_tick = [] { return 0; };
    /// Game.Sound 的直连面(VideoPlayerWidget 的视频音频;未接线 = null =
    /// 静默)
    /// The Game.Sound direct face (VideoPlayerWidget's video audio;
    /// unwired = null = silent).
    std::function<sound::Sound*()> fn_sound = [] { return static_cast<sound::Sound*>(nullptr); };
    /// VideoLoader.GetVideo(ModData.DefaultFileSystem.Open(name),
    /// padding, ModData.VideoLoaders)(未接线 = null = 无视频)
    /// VideoLoader.GetVideo (unwired = null = no video).
    std::function<std::unique_ptr<fmt::IVideo>(std::string_view, bool)>
        fn_load_video;
    /// Game.Cursor.Lock/Unlock(SelectDirectionalTarget 的拖拽锁定;未接线
    /// = no-op)
    /// Game.Cursor.Lock/Unlock (SelectDirectionalTarget's drag lock;
    /// unwired = no-op).
    std::function<void()> fn_cursor_lock = [] {};
    std::function<void()> fn_cursor_unlock = [] {};
  };

  void Initialize(Deps deps);

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

  void Tick() { ptr_root_->TickOuter(); }              // L106
  void PrepareRenderables() { ptr_root_->PrepareRenderablesOuter(); }  // L108

  bool WidgetsVisible() const { return b_widgets_visible_; }
  void SetWidgetsVisible(bool b_value) { b_widgets_visible_ = b_value; }

  void Draw();  // L111-117

  bool HandleInput(const MouseInput& mi_input);  // L119-147

  bool HandleKeyPress(const KeyInput& e_input);

  bool HandleTextInput(std::string_view str_text);  // L160-166

  void ResetAll();  // L168-174

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
    Widget* ptr_widget = nullptr;      // 栈视图 | the stack view
    std::unique_ptr<Widget> up_owned;  // 隐藏态所有权 | ownership while hidden
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

}  // namespace ora::ui
