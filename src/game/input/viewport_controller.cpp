#include "game/input/viewport_controller.hpp"

#include <cmath>

namespace ora::game::input {

namespace {

constexpr double kZoomMin{1.0};
constexpr double kZoomMax{2.0};
constexpr double kZoomStep{0.25};

HotkeyModel viewport_hotkey(const HotkeyRegistryModel& reg, std::string_view str_name) {
  return hotkey_resolve(reg, str_name);
}

bool scroll_key_hit(const HotkeyModel& hk, const KeyInput& ki) {
  return hk.key == ki.key;
}

std::optional<std::size_t> bookmark_index(const HotkeyRegistryModel& reg, const KeyInput& ki,
                                          std::string_view str_prefix) {
  for (std::size_t i{0}; i < 4; ++i)
    if (hotkey_is_activated_by(
            viewport_hotkey(reg, std::format("{}{:02}", str_prefix, i + 1)), ki))
      return i;
  return std::nullopt;
}

}

void viewport_adjust_zoom(ViewportState& viewport, double f64_dz) {
  viewport.f64_zoom = std::clamp(viewport.f64_zoom * std::exp(f64_dz), kZoomMin, kZoomMax);
}

void viewport_toggle_zoom(ViewportState& viewport) {
  if (viewport.f64_zoom < kZoomMin)
    viewport.f64_zoom = kZoomMin;
  else
    viewport.f64_zoom = viewport.f64_zoom > kZoomMin ? kZoomMin : kZoomMax;
}

void viewport_set_center(ViewportState& viewport, std::int32_t int4_x, std::int32_t int4_y) {
  viewport.int4_center_x = int4_x;
  viewport.int4_center_y = int4_y;
}

bool viewport_handle_key(ViewportState& viewport, const HotkeyRegistryModel& reg,
                         const KeyInput& ki, std::int32_t int4_map_w_px,
                         std::int32_t int4_map_h_px) {
  const std::pair<std::string_view, ScrollDirection> arr_scroll[]{
      {"MapScrollUp", ScrollDirection::kUp},
      {"MapScrollDown", ScrollDirection::kDown},
      {"MapScrollLeft", ScrollDirection::kLeft},
      {"MapScrollRight", ScrollDirection::kRight},
  };
  bool b_scroll_hotkey{false};
  for (const auto& [str_name, dir] : arr_scroll) {
    const HotkeyModel hk{viewport_hotkey(reg, str_name)};
    if (!scroll_key_hit(hk, ki)) continue;
    const bool b_hotkey{hotkey_is_activated_by(hk, ki)};
    // 上游 ViewportControllerWidget.cs:404-412：Down 且（全匹配命中 或 绑定无修饰键）
    // 置位；Up 清位。绑定无修饰键时任何修饰键组合都置位（上游 quirk，照抄）。
    const bool b_enable{ki.event == KeyInputEvent::kDown &&
                        (b_hotkey || hk.mods == ModifierKeys::kNone)};
    if (b_enable)
      viewport.scroll_directions = viewport.scroll_directions | dir;
    else
      viewport.scroll_directions = static_cast<ScrollDirection>(
          static_cast<std::uint8_t>(viewport.scroll_directions) &
          ~static_cast<std::uint8_t>(dir));
    b_scroll_hotkey = b_scroll_hotkey || b_hotkey;
  }
  if (b_scroll_hotkey) return true;

  if (ki.event != KeyInputEvent::kDown) return false;

  if (hotkey_is_activated_by(viewport_hotkey(reg, "ZoomIn"), ki)) {
    viewport_adjust_zoom(viewport, kZoomStep);
    return true;
  }
  if (hotkey_is_activated_by(viewport_hotkey(reg, "ZoomOut"), ki)) {
    viewport_adjust_zoom(viewport, -kZoomStep);
    return true;
  }
  if (hotkey_is_activated_by(viewport_hotkey(reg, "ResetZoom"), ki)) {
    viewport_toggle_zoom(viewport);
    return true;
  }

  if (hotkey_is_activated_by(viewport_hotkey(reg, "MapJumpToTopEdge"), ki)) {
    viewport_set_center(viewport, viewport.int4_center_x, 0);
    return true;
  }
  if (hotkey_is_activated_by(viewport_hotkey(reg, "MapJumpToBottomEdge"), ki)) {
    viewport_set_center(viewport, viewport.int4_center_x, int4_map_h_px);
    return true;
  }
  if (hotkey_is_activated_by(viewport_hotkey(reg, "MapJumpToLeftEdge"), ki)) {
    viewport_set_center(viewport, 0, viewport.int4_center_y);
    return true;
  }
  if (hotkey_is_activated_by(viewport_hotkey(reg, "MapJumpToRightEdge"), ki)) {
    viewport_set_center(viewport, int4_map_w_px, viewport.int4_center_y);
    return true;
  }

  if (const auto opt_save{bookmark_index(reg, ki, "MapBookmarkSave")}) {
    viewport.arr_bookmarks[*opt_save] =
        std::pair{viewport.int4_center_x, viewport.int4_center_y};
    return true;
  }
  if (const auto opt_restore{bookmark_index(reg, ki, "MapBookmarkRestore")}) {
    if (const auto& opt_pos{viewport.arr_bookmarks[*opt_restore]}) {
      viewport_set_center(viewport, opt_pos->first, opt_pos->second);
      return true;
    }
    return false;
  }

  if (hotkey_is_activated_by(viewport_hotkey(reg, "DisableUserInterface"), ki)) {
    viewport.b_disable_user_interface = true;
    return true;
  }
  if (hotkey_is_activated_by(viewport_hotkey(reg, "DisableAllUserInterface"), ki)) {
    viewport.b_disable_all_user_interface = true;
    return true;
  }

  return false;
}

}
