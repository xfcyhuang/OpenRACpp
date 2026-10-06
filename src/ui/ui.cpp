// UPSTREAM: OpenRA.Game/Widgets/Widget.cs @b6fc03f L24-194(ui.hpp 的实现)
// The implementation of ui.hpp.
import std;

#include "ui/ui.hpp"

#include "gfx/viewport.hpp"

namespace ora::ui {
namespace {

// 单例已构造标志(HasInstance 不得触发构造)| the singleton-exists flag
// (HasInstance must never construct).
std::atomic<bool> b_singleton_created_{false};

}  // namespace

Ui& Ui::Instance() {
  static Ui instance_ui;
  b_singleton_created_.store(true, std::memory_order_relaxed);
  return instance_ui;
}

bool Ui::HasInstance() {
  return b_singleton_created_.load(std::memory_order_relaxed);
}

Ui::Ui()
    : ptr_root_(std::make_unique<ContainerWidget>()),
      last_tick_time_([] { return kTimestep; }, 0) {}

Ui::~Ui() = default;

void Ui::Initialize(Deps deps) {
  // 上游 Initialize 仅存 modData;注入面整体换装为装配点
  // upstream Initialize only stores modData; the Deps swap is the assembly
  // point for the injected faces.
  deps_ = std::move(deps);
}

void Ui::CloseWindow() {
  if (!vec_windows_.empty()) {
    WindowEntry entry_hidden = std::move(vec_windows_.back());
    vec_windows_.pop_back();

    std::unique_ptr<Widget> up_detached = ptr_root_->RemoveChild(*entry_hidden.ptr_widget);

    // 上游怪癖:RemoveChild 已 Dispose Logic,仍对快照逐个 BecameHidden
    // the upstream quirk: RemoveChild has already disposed the Logic, yet
    // BecameHidden is still called on the snapshot
    std::vector<ChromeLogic*> vec_logic_ptrs;
    vec_logic_ptrs.reserve(up_detached->LogicObjects().size());
    for (const auto& ptr_logic : up_detached->LogicObjects())
      vec_logic_ptrs.push_back(ptr_logic.get());
    for (ChromeLogic* ptr_logic : vec_logic_ptrs)
      ptr_logic->BecameHidden();

    // up_detached 于作用域末析构(widget 连同 Logic 对象)
    // up_detached dies at scope end (the widget with its Logic objects).
  }

  if (!vec_windows_.empty()) {
    WindowEntry& entry_restore = vec_windows_.back();
    if (entry_restore.up_owned != nullptr) {
      // 隐藏态条目的所有权交还 Root | a hidden entry's ownership returns
      // to Root
      ptr_root_->AddChild(std::move(entry_restore.up_owned));
    }

    for (const auto& ptr_logic : entry_restore.ptr_widget->LogicObjects())
      ptr_logic->BecameVisible();
  }
}

Widget* Ui::OpenWindow(const std::string& str_id, WidgetArgs args_args) {
  // 上游 L75-76 的 modData 补缺 + 本侧 chromeMetrics/windowSize 补缺(上游
  // 为静态取值,此处随 args 传递 —— widget.hpp 文件头)
  // upstream's L75-76 modData fill-in, plus this side's chromeMetrics/
  // windowSize fill-in (upstream fetches statically, threaded via args —
  // widget.hpp header).
  if (!args_args.ContainsKey("modData"))
    args_args = args_args.With("modData", deps_.ptr_mod_data);
  if (!args_args.ContainsKey("chromeMetrics"))
    args_args = args_args.With("chromeMetrics", deps_.ptr_chrome_metrics);
  if (!args_args.ContainsKey("windowSize"))
    args_args = args_args.With("windowSize", deps_.fn_window_size ? deps_.fn_window_size() : int2{});

  if (!deps_.fn_load_widget)
    throw std::runtime_error("Ui: no widget loader wired");
  Widget* ptr_window = deps_.fn_load_widget(args_args, ptr_root_.get(), str_id);

  if (!vec_windows_.empty()) {
    auto up_hidden = ptr_root_->HideChild(*vec_windows_.back().ptr_widget);
    vec_windows_.back().up_owned = std::move(up_hidden);
  }
  vec_windows_.push_back(WindowEntry{ptr_window, {}});
  return ptr_window;
}

Widget* Ui::CurrentWindow() const {
  return vec_windows_.empty() ? nullptr : vec_windows_.back().ptr_widget;
}

Widget* Ui::LoadWidget(const std::string& str_id, Widget* ptr_parent, WidgetArgs args_args) {
  if (!args_args.ContainsKey("modData"))
    args_args = args_args.With("modData", deps_.ptr_mod_data);
  if (!args_args.ContainsKey("chromeMetrics"))
    args_args = args_args.With("chromeMetrics", deps_.ptr_chrome_metrics);
  if (!args_args.ContainsKey("windowSize"))
    args_args = args_args.With("windowSize", deps_.fn_window_size ? deps_.fn_window_size() : int2{});

  if (!deps_.fn_load_widget)
    throw std::runtime_error("Ui: no widget loader wired");
  return deps_.fn_load_widget(args_args, ptr_parent, str_id);
}

void Ui::Draw() {
  if (!b_widgets_visible_)
    return;

  ptr_root_->DrawOuter();
}

bool Ui::HandleInput(const MouseInput& mi_input) {
  Widget* ptr_was_mouse_over = ptr_mouse_over_;

  if (mi_input.Event == MouseInputEvent::Move)
    ptr_mouse_over_ = nullptr;

  bool b_handled = false;
  if (ptr_mouse_focus_ != nullptr && ptr_mouse_focus_->HandleMouseInputOuter(mi_input))
    b_handled = true;

  if (!b_handled && ptr_root_->HandleMouseInputOuter(mi_input))
    b_handled = true;

  if (mi_input.Event == MouseInputEvent::Move) {
    gfx::Viewport::int2_last_mouse_pos = mi_input.Location;
    gfx::Viewport::int8_last_move_run_time = deps_.fn_clock ? deps_.fn_clock() : 0;
  }

  if (ptr_was_mouse_over != ptr_mouse_over_) {
    if (ptr_was_mouse_over != nullptr)
      ptr_was_mouse_over->MouseExited();

    if (ptr_mouse_over_ != nullptr)
      ptr_mouse_over_->MouseEntered();
  }

  return b_handled;
}

bool Ui::HandleKeyPress(const KeyInput& e_input) {
  if (ptr_keyboard_focus_ != nullptr)
    return ptr_keyboard_focus_->HandleKeyPressOuter(e_input);

  return ptr_root_->HandleKeyPressOuter(e_input);
}

bool Ui::HandleTextInput(std::string_view str_text) {
  if (ptr_keyboard_focus_ != nullptr)
    return ptr_keyboard_focus_->HandleTextInputOuter(str_text);

  return ptr_root_->HandleTextInputOuter(str_text);
}

void Ui::ResetAll() {
  // 上游序为 RemoveChildren → CloseWindow 循环(GC 下悬引用安全);C++ 可见
  // 栈顶由 Root 持有,先清 Root 会析构它 —— 逆序排水等价终态(COVERAGE 登记)
  while (!vec_windows_.empty())
    CloseWindow();

  ptr_root_->RemoveChildren();
}

void Ui::ResetTooltips() {
  // Issue a no-op mouse move to force any tooltips to be recalculated(上游注释)
  HandleInput(MouseInput{MouseInputEvent::Move, MouseButton::None,
                         gfx::Viewport::int2_last_mouse_pos, int2{}, Modifiers::None, 0});
}

}  // namespace ora::ui
