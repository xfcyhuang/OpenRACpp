#pragma once

import std;

#include "game/input/hotkey.hpp"
#include "game/input/hotkey_registry.hpp"

namespace ora::game::input {

// 键盘滚动方向位（上游 ScrollDirection，ViewportControllerWidget 键面）。
enum class ScrollDirection : std::uint8_t {
  kNone = 0,
  kUp = 1,
  kDown = 2,
  kLeft = 4,
  kRight = 8,
};

[[nodiscard]] constexpr ScrollDirection operator|(ScrollDirection a, ScrollDirection b) {
  return static_cast<ScrollDirection>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr ScrollDirection operator&(ScrollDirection a, ScrollDirection b) {
  return static_cast<ScrollDirection>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr ScrollDirection operator~(ScrollDirection a) {
  return static_cast<ScrollDirection>(~static_cast<std::uint8_t>(a) & 0x0f);
}

[[nodiscard]] constexpr bool has_scroll_direction(ScrollDirection dirs, ScrollDirection dir) {
  return (dirs & dir) != ScrollDirection::kNone;
}

// 无头视口状态（上游 Viewport + ViewportControllerWidget 键消费面）。
// 坐标 = world px；zoom 默认 [1,2]（Viewport.cs:92-93 上游缺省，mod 覆盖未建模）。
struct ViewportState {
  std::int32_t int4_center_x{};
  std::int32_t int4_center_y{};
  double f64_zoom{1.0};
  std::array<std::optional<std::pair<std::int32_t, std::int32_t>>, 4> arr_bookmarks{};
  ScrollDirection scroll_directions{ScrollDirection::kNone};
  bool b_disable_user_interface{};
  bool b_disable_all_user_interface{};
};

// Viewport.AdjustZoom（Viewport.cs:102-106）：zoom *= exp(dz)，夹 [1,2]。
void viewport_adjust_zoom(ViewportState& viewport, double f64_dz);

// Viewport.ToggleZoom（Viewport.cs:118-126，ResetZoom 热键消费）：zoom > min → min；
// zoom == min → max。
void viewport_toggle_zoom(ViewportState& viewport);

void viewport_set_center(ViewportState& viewport, std::int32_t int4_x, std::int32_t int4_y);

// ViewportControllerWidget.HandleKeyPress（:400-490）键面：
// MapScrollUp/Down/Left/Right —— Down 置方向位（isHotkey 或绑定无修饰键时置位）、
// Up 清位（上游 :404-412 quirk：绑定 Modifiers==None 时任何修饰键按下也算命中）；
// ZoomIn/ZoomOut = AdjustZoom(±0.25)；MapJumpTo*Edge = 视口中心钉到地图边
// （int4_map_w_px/int4_map_h_px = 地图 world px 尺寸）；MapBookmarkSave/Restore01..04；
// DisableUserInterface/DisableAllUserInterface 位。全走注册表活查询（全匹配激活）。
[[nodiscard]] bool viewport_handle_key(ViewportState& viewport, const HotkeyRegistryModel& reg,
                                       const KeyInput& ki, std::int32_t int4_map_w_px,
                                       std::int32_t int4_map_h_px);

}
