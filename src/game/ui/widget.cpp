module ora.game.ui;

import std;
import ora.game.input;

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

bool is_slider_type(const WidgetModel& w) {
  return w.str_type == "Slider" || w.str_type == "ExponentialSlider";
}

bool is_button_type(const WidgetModel& w) {
  return w.str_type == "Button" || w.str_type == "MenuButton" || w.str_type == "ProductionTypeButton"
      || w.str_type == "Checkbox" || w.str_type == "DropDownButton";
}

}  // namespace

float widget_slider_value_from_px(const WidgetModel& w, int int4_x) {
  const WidgetBounds rb{widget_event_bounds(w)};
  return w.fp4_min_value
      + (w.fp4_max_value - w.fp4_min_value)
      * (static_cast<float>(int4_x) - 0.5f * static_cast<float>(rb.int4_height))
      / static_cast<float>(rb.int4_width - rb.int4_height);
}

void widget_slider_update_value(WidgetModel& w, UiState& st, float fp4_new) {
  const float fp4_old{w.fp4_value};
  w.fp4_value = std::clamp(fp4_new, w.fp4_min_value, w.fp4_max_value);
  if (w.fp4_value != fp4_old)
    st.vec_events.push_back(UiEventRecord{w.str_id, std::format("change {:.6f}", w.fp4_value)});
}

bool widget_slider_thumb_contains(const WidgetModel& w, int int4_x, int int4_y) {
  const SliderGeometry geo{widget_slider_geometry(w)};
  return int4_x >= geo.int4_thumb_x && int4_x < geo.int4_thumb_x + geo.int4_thumb_w
      && int4_y >= geo.int4_thumb_y && int4_y < geo.int4_thumb_y + geo.int4_thumb_h;
}

SliderGeometry widget_slider_geometry(const WidgetModel& w) {
  const WidgetBounds rb{widget_event_bounds(w)};
  const int int4_thumb_pos{static_cast<int>(
      0.5f * static_cast<float>(rb.int4_height)
      + static_cast<float>(rb.int4_width - rb.int4_height)
      * (w.fp4_value - w.fp4_min_value) / (w.fp4_max_value - w.fp4_min_value))};
  SliderGeometry geo;
  geo.int4_track_origin_x = rb.int4_x + rb.int4_height / 2;
  geo.int4_thumb_x = static_cast<int>(
      static_cast<float>(rb.int4_x) + static_cast<float>(int4_thumb_pos)
      - static_cast<float>(rb.int4_height) / 2.0f);
  geo.int4_thumb_y = rb.int4_y;
  geo.int4_thumb_w = rb.int4_height;
  geo.int4_thumb_h = rb.int4_height;
  geo.int4_track_x = geo.int4_track_origin_x - 1;
  geo.int4_track_y = rb.int4_y + (rb.int4_height - w.int4_track_height) / 2;
  geo.int4_track_w = rb.int4_width - rb.int4_height + 2;
  geo.int4_track_h = w.int4_track_height;
  return geo;
}

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
  if (is_button_type(w)) w.b_depressed = false;
  if (st.ptr_mouse_focus == &w) st.ptr_mouse_focus = nullptr;
  return true;
}

bool widget_handle_mouse_input(WidgetModel& w, UiState& st, const ora::game::input::MouseInput& mi) {
  namespace input = ora::game::input;
  if (is_button_type(w)) {
    if (mi.button != input::MouseButton::kLeft) return false;
    if (mi.event == input::MouseInputEvent::kDown && !widget_take_mouse_focus(w, st)) return false;

    if (st.ptr_mouse_focus == &w && mi.event == input::MouseInputEvent::kUp && mi.int4_tap_count == 2
        && w.b_has_double_click) {
      if (!w.b_disabled) {
        st.vec_events.push_back(UiEventRecord{w.str_id, "dblclick"});
        return widget_yield_mouse_focus(w, st);
      }
    } else if (st.ptr_mouse_focus == &w && mi.event == input::MouseInputEvent::kUp) {
      if (w.b_depressed && !w.b_disabled) st.vec_events.push_back(UiEventRecord{w.str_id, "click"});
      return widget_yield_mouse_focus(w, st);
    }

    if (mi.event == input::MouseInputEvent::kDown) {
      if (!w.b_disabled) {
        if (w.b_record_mouse_down)
          st.vec_events.push_back(
              UiEventRecord{w.str_id, std::format("down({},{})", mi.int4_x, mi.int4_y)});
        w.b_depressed = true;
      } else {
        static_cast<void>(widget_yield_mouse_focus(w, st));
      }
    } else if (mi.event == input::MouseInputEvent::kMove && st.ptr_mouse_focus == &w) {
      w.b_depressed = bounds_contains(widget_event_bounds(w), mi.int4_x, mi.int4_y);
    }
    return w.b_depressed;
  }
  if (is_slider_type(w)) {
    if (mi.button != input::MouseButton::kLeft) return false;
    if (w.b_disabled) return false;
    if (mi.event == input::MouseInputEvent::kDown && !widget_take_mouse_focus(w, st)) return false;
    if (st.ptr_mouse_focus != &w) return false;

    if (mi.event == input::MouseInputEvent::kUp) {
      w.b_moving = false;
      static_cast<void>(widget_yield_mouse_focus(w, st));
    } else if (mi.event == input::MouseInputEvent::kDown) {
      w.b_moving = true;
      widget_slider_update_value(
          w, st, widget_slider_value_from_px(w, mi.int4_x - widget_event_bounds(w).int4_x));
    } else if (mi.event == input::MouseInputEvent::kMove && w.b_moving) {
      widget_slider_update_value(
          w, st, widget_slider_value_from_px(w, mi.int4_x - widget_event_bounds(w).int4_x));
    }

    return widget_slider_thumb_contains(w, mi.int4_x, mi.int4_y);
  }
  if (w.str_type == "Container" || w.str_type == "Image" || w.str_type == "Background") {
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

  if (mi.event == input::MouseInputEvent::kMove) {
    st.ptr_mouse_over = nullptr;
    st.int4_last_mouse_x = mi.int4_x;
    st.int4_last_mouse_y = mi.int4_y;
  }

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

WidgetModel* widget_find_by_id(WidgetModel& w, std::string_view str_id) {
  // 上游 Get：自身命中先于子树（self-first 深度优先）
  if (w.str_id == str_id) return &w;
  for (const auto& ptr_child : w.vec_children)
    if (WidgetModel* ptr_found{widget_find_by_id(*ptr_child, str_id)}) return ptr_found;
  return nullptr;
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
