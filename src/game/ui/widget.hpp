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

[[nodiscard]] const std::string* widget_hit_test(const WidgetModel& w, int int4_x, int int4_y);

}
