#include "game/ui/widget.hpp"

#include "game/ui/expression.hpp"

namespace ora::game::ui {

void widget_initialize(WidgetModel& w, const WidgetInitCtx& ctx) {
  const int int4_parent_width{w.ptr_parent != nullptr ? w.ptr_parent->bounds.int4_width : ctx.int4_window_width};
  const int int4_parent_height{w.ptr_parent != nullptr ? w.ptr_parent->bounds.int4_height : ctx.int4_window_height};

  std::map<std::string, int> map_subs;
  if (ctx.map_substitutions != nullptr) map_subs = *ctx.map_substitutions;
  map_subs["WINDOW_WIDTH"] = ctx.int4_window_width;
  map_subs["WINDOW_HEIGHT"] = ctx.int4_window_height;
  map_subs["PARENT_WIDTH"] = int4_parent_width;
  map_subs["PARENT_HEIGHT"] = int4_parent_height;

  int int4_width{};
  if (!w.str_width.empty()) {
    if (const auto res{widget_int_eval(w.str_width, map_subs)}; res.has_value()) int4_width = *res;
  }
  int int4_height{};
  if (!w.str_height.empty()) {
    if (const auto res{widget_int_eval(w.str_height, map_subs)}; res.has_value()) int4_height = *res;
  }

  map_subs["WIDTH"] = int4_width;
  map_subs["HEIGHT"] = int4_height;

  int int4_x{};
  if (!w.str_x.empty()) {
    if (const auto res{widget_int_eval(w.str_x, map_subs)}; res.has_value()) int4_x = *res;
  }
  int int4_y{};
  if (!w.str_y.empty()) {
    if (const auto res{widget_int_eval(w.str_y, map_subs)}; res.has_value()) int4_y = *res;
  }

  w.str_default_cursor = ctx.str_default_cursor;
  w.bounds = WidgetBounds{int4_x, int4_y, int4_width, int4_height};
}

std::pair<int, int> widget_render_origin(const WidgetModel& w) {
  const std::pair<int, int> pr_offset{w.ptr_parent != nullptr
                                          ? widget_render_origin(*w.ptr_parent)
                                          : std::pair<int, int>{0, 0}};
  return {w.bounds.int4_x + pr_offset.first, w.bounds.int4_y + pr_offset.second};
}

WidgetBounds widget_event_bounds(const WidgetModel& w) {
  const auto [int4_ox, int4_oy]{widget_render_origin(w)};
  return WidgetBounds{int4_ox, int4_oy, w.bounds.int4_width, w.bounds.int4_height};
}

namespace {

bool bounds_contains(const WidgetBounds& b, int int4_x, int int4_y) {
  return int4_x >= b.int4_x && int4_x < b.int4_x + b.int4_width && int4_y >= b.int4_y
      && int4_y < b.int4_y + b.int4_height;
}

bool widget_is_visible(const WidgetModel& w) { return w.b_visible; }

}  // namespace

bool widget_event_bounds_contains(const WidgetModel& w, int int4_x, int int4_y) {
  if (bounds_contains(widget_event_bounds(w), int4_x, int4_y)) return true;
  for (const auto& ptr_child : w.vec_children)
    if (widget_is_visible(*ptr_child) && widget_event_bounds_contains(*ptr_child, int4_x, int4_y))
      return true;
  return false;
}

bool widget_take_mouse_focus(WidgetModel& w, UiState& st) {
  if (st.ptr_mouse_focus == &w) return true;
  if (st.ptr_mouse_focus != nullptr && !widget_yield_mouse_focus(*st.ptr_mouse_focus, st)) return false;
  st.ptr_mouse_focus = &w;
  return true;
}

bool widget_yield_mouse_focus(WidgetModel& w, UiState& st) {
  if (w.str_type == "Button" || w.str_type == "MenuButton" || w.str_type == "ProductionTypeButton")
    w.b_depressed = false;
  if (st.ptr_mouse_focus == &w) st.ptr_mouse_focus = nullptr;
  return true;
}

namespace {

bool is_button_type(const WidgetModel& w) {
  return w.str_type == "Button" || w.str_type == "MenuButton" || w.str_type == "ProductionTypeButton";
}

}  // namespace

bool widget_handle_mouse_input(WidgetModel& w, UiState& st, const ora::game::input::MouseInput& mi) {
  namespace input = ora::game::input;
  if (is_button_type(w)) {
    if (mi.button != input::MouseButton::kLeft) return false;
    if (mi.event == input::MouseInputEvent::kDown && !widget_take_mouse_focus(w, st)) return false;
    if (st.ptr_mouse_focus == &w && mi.event == input::MouseInputEvent::kUp)
      return widget_yield_mouse_focus(w, st);
    if (mi.event == input::MouseInputEvent::kDown) {
      if (!w.b_disabled) {
        w.b_depressed = true;
      } else {
        static_cast<void>(widget_yield_mouse_focus(w, st));
      }
    } else if (mi.event == input::MouseInputEvent::kMove && st.ptr_mouse_focus == &w) {
      w.b_depressed = bounds_contains(widget_event_bounds(w), mi.int4_x, mi.int4_y);
    }
    return w.b_depressed;
  }
  if (w.str_type == "Container") {
    return !w.b_click_through && bounds_contains(widget_event_bounds(w), mi.int4_x, mi.int4_y);
  }
  if (w.str_type == "Image") {
    return !w.b_click_through && bounds_contains(widget_event_bounds(w), mi.int4_x, mi.int4_y);
  }
  return false;
}

bool widget_handle_mouse_input_outer(WidgetModel& w, UiState& st,
                                     const ora::game::input::MouseInput& mi) {
  namespace input = ora::game::input;
  const bool b_has_focus{st.ptr_mouse_focus == &w};
  if (!(b_has_focus || (widget_is_visible(w) && widget_event_bounds_contains(w, mi.int4_x, mi.int4_y))))
    return false;

  WidgetModel* ptr_old_mouse_over{st.ptr_mouse_over};

  for (std::size_t sz_i{w.vec_children.size()}; sz_i-- > 0;)
    if (widget_handle_mouse_input_outer(*w.vec_children[sz_i], st, mi)) return true;

  if (w.b_ignore_child_mouse_over) st.ptr_mouse_over = ptr_old_mouse_over;

  if (mi.event == input::MouseInputEvent::kMove && st.ptr_mouse_over == nullptr && !w.b_ignore_mouse_over)
    st.ptr_mouse_over = &w;

  return widget_handle_mouse_input(w, st, mi);
}

bool ui_handle_input(WidgetModel& root, UiState& st, const ora::game::input::MouseInput& mi) {
  namespace input = ora::game::input;
  WidgetModel* ptr_was_mouse_over{st.ptr_mouse_over};

  if (mi.event == input::MouseInputEvent::kMove) st.ptr_mouse_over = nullptr;

  bool b_handled{false};
  if (st.ptr_mouse_focus != nullptr && widget_handle_mouse_input_outer(*st.ptr_mouse_focus, st, mi))
    b_handled = true;

  if (!b_handled && widget_handle_mouse_input_outer(root, st, mi)) b_handled = true;

  if (ptr_was_mouse_over != st.ptr_mouse_over) {
    if (ptr_was_mouse_over != nullptr) st.vec_exited.push_back(ptr_was_mouse_over);
    if (st.ptr_mouse_over != nullptr) st.vec_entered.push_back(st.ptr_mouse_over);
  }

  return b_handled;
}

bool widget_handle_key_press_outer(WidgetModel& w, UiState& st, const ora::game::input::KeyInput& ki) {
  if (!w.b_visible) return false;
  for (std::size_t sz_i{w.vec_children.size()}; sz_i-- > 0;)
    if (widget_handle_key_press_outer(*w.vec_children[sz_i], st, ki)) return true;
  return false;
}

bool ui_handle_key_press(WidgetModel& root, UiState& st, const ora::game::input::KeyInput& ki) {
  if (st.ptr_keyboard_focus != nullptr)
    return widget_handle_key_press_outer(*st.ptr_keyboard_focus, st, ki);
  return widget_handle_key_press_outer(root, st, ki);
}

const std::string* widget_get_cursor(const WidgetModel& w) {
  if (w.str_type == "Container") return nullptr;
  return &w.str_default_cursor;
}

const std::string* widget_hit_test(const WidgetModel& w, int int4_x, int int4_y) {
  if (!(widget_is_visible(w) && widget_event_bounds_contains(w, int4_x, int4_y))) return nullptr;
  for (std::size_t sz_i{w.vec_children.size()}; sz_i-- > 0;) {
    const std::string* ptr_cc{widget_hit_test(*w.vec_children[sz_i], int4_x, int4_y)};
    if (ptr_cc != nullptr) return ptr_cc;
  }
  if (bounds_contains(widget_event_bounds(w), int4_x, int4_y)) return widget_get_cursor(w);
  return nullptr;
}

}
