import std;

#include "ui/ui.hpp"

#include "gfx/viewport.hpp"

namespace ora::ui {
namespace {

std::atomic<bool> b_singleton_created_{false};

}

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

  deps_ = std::move(deps);
}

void Ui::WireMapCache(map::MapCache* ptr_map_cache) {
  deps_.ptr_map_cache = ptr_map_cache;
}

void Ui::WireModData(
    game::ModData* ptr_mod_data,
    std::function<Widget*(WidgetArgs&, Widget*, const std::string&)>
        fn_load_widget,
    std::function<Widget*(sim::World&, const std::string&, Widget*,
                          WidgetArgs&)> fn_game_load_widget) {
  deps_.ptr_mod_data = ptr_mod_data;
  deps_.fn_load_widget = std::move(fn_load_widget);
  if (fn_game_load_widget)
    deps_.fn_game_load_widget = std::move(fn_game_load_widget);
}

void Ui::CloseWindow() {
  if (!vec_windows_.empty()) {
    WindowEntry entry_hidden = std::move(vec_windows_.back());
    vec_windows_.pop_back();
    std::unique_ptr<Widget> up_detached = ptr_root_->RemoveChild(*entry_hidden.ptr_widget);
    if (up_detached == nullptr)
      up_detached = std::move(entry_hidden.up_owned);

    if (up_detached != nullptr) {
      std::vector<ChromeLogic*> vec_logic_ptrs;
      vec_logic_ptrs.reserve(up_detached->LogicObjects().size());
      for (const auto& ptr_logic : up_detached->LogicObjects())
        vec_logic_ptrs.push_back(ptr_logic.get());
      for (ChromeLogic* ptr_logic : vec_logic_ptrs)
        ptr_logic->BecameHidden();

    }
  }

  if (!vec_windows_.empty()) {
    WindowEntry& entry_restore = vec_windows_.back();
    if (entry_restore.up_owned != nullptr) {

      ptr_root_->AddChild(std::move(entry_restore.up_owned));
    }

    for (const auto& ptr_logic : entry_restore.ptr_widget->LogicObjects())
      ptr_logic->BecameVisible();
  }
}

Widget* Ui::OpenWindow(const std::string& str_id, WidgetArgs args_args) {

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

  while (!vec_windows_.empty())
    CloseWindow();

  ptr_root_->RemoveChildren();
}

void Ui::ResetTooltips() {

  HandleInput(MouseInput{MouseInputEvent::Move, MouseButton::None,
                         gfx::Viewport::int2_last_mouse_pos, int2{}, Modifiers::None, 0});
}

}
