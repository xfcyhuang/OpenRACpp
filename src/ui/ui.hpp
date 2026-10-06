// UPSTREAM: OpenRA.Game/Widgets/Widget.cs @b6fc03f L24-194(Ui 静态门面逐语义:
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

#include "net/tick_time.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class Viewport;
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
    std::function<int2()> fn_window_size;    // Renderer.Resolution
    std::function<std::int64_t()> fn_clock;  // Game.RunTime(缺省恒 0 | 0 by default)
    ChromeMetrics* ptr_chrome_metrics = nullptr;
  };

  void Initialize(Deps deps);

  Widget* MouseFocusWidget() const { return ptr_mouse_focus_; }
  void SetMouseFocusWidget(Widget* ptr_widget) { ptr_mouse_focus_ = ptr_widget; }
  Widget* KeyboardFocusWidget() const { return ptr_keyboard_focus_; }
  void SetKeyboardFocusWidget(Widget* ptr_widget) { ptr_keyboard_focus_ = ptr_widget; }
  Widget* MouseOverWidget() const { return ptr_mouse_over_; }
  void SetMouseOverWidget(Widget* ptr_widget) { ptr_mouse_over_ = ptr_widget; }

  Mediator& mediator() { return mediator_; }

  Widget* Root() const { return ptr_root_.get(); }

  /// 步长恒 kTimestep(L30) | the timestep fixed at kTimestep (L30).
  net::TickTime& LastTickTime() { return last_tick_time_; }

  /// L46-66:Pop → RemoveChild → BecameHidden 序照抄,含对已 Dispose Logic
  /// 仍通知的上游怪癖(快照指针在析构前逐个调用)
  /// L46-66: the pop → RemoveChild → BecameHidden order kept, including the
  /// upstream quirk of notifying already-disposed Logic (snapshot pointers
  /// called one by one before destruction).
  void CloseWindow();

  /// L68-83 | the OpenWindow family (L68-83).
  Widget* OpenWindow(const std::string& str_id, WidgetArgs args_args);
  Widget* OpenWindow(const std::string& str_id) { return OpenWindow(str_id, WidgetArgs{}); }

  Widget* CurrentWindow() const;

  /// L90-104;不入树时须传 Root 持有 | L90-104; pass Root even off-tree.
  Widget* LoadWidget(const std::string& str_id, Widget* ptr_parent, WidgetArgs args_args);

  void Tick() { ptr_root_->TickOuter(); }              // L106
  void PrepareRenderables() { ptr_root_->PrepareRenderablesOuter(); }  // L108

  bool WidgetsVisible() const { return b_widgets_visible_; }
  void SetWidgetsVisible(bool b_value) { b_widgets_visible_ = b_value; }

  void Draw();  // L111-117

  bool HandleInput(const MouseInput& mi_input);  // L119-147

  /// 有键盘焦点走焦点 widget,否则 Root(L152-158)
  /// keyboard focus goes to the focused widget, else Root (L152-158).
  bool HandleKeyPress(const KeyInput& e_input);

  bool HandleTextInput(std::string_view str_text);  // L160-166

  void ResetAll();  // L168-174

  /// no-op 鼠标移动强制重算(L176-181) | a no-op mouse move forces
  /// recalculation (L176-181).
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
