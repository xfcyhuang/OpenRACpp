#pragma once

import std;

#include "game/input/input_events.hpp"

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
  std::vector<std::string> vec_logic;
  std::string str_default_cursor;
  std::vector<std::pair<std::string, std::string>> vec_fields;
  std::vector<std::unique_ptr<WidgetModel>> vec_children;
  WidgetModel* ptr_parent{};
  WidgetBounds bounds{};
};

struct UiState {
  WidgetModel* ptr_mouse_focus{};
  WidgetModel* ptr_keyboard_focus{};
  WidgetModel* ptr_mouse_over{};
  std::vector<WidgetModel*> vec_entered;
  std::vector<WidgetModel*> vec_exited;
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

[[nodiscard]] const std::string* widget_hit_test(const WidgetModel& w, int int4_x, int int4_y);

}
