#pragma once

import std;

namespace ora::game::input {

enum class MouseButton : std::uint8_t {
  kNone = 0,
  kLeft = 1,
  kMiddle = 2,
  kRight = 3,
};

enum class MouseInputEvent : std::uint8_t {
  kNone = 0,
  kDown,
  kUp,
  kMove,
};

enum class ModifierKeys : std::uint8_t {
  kNone = 0,
  kCtrl = 1,
  kAlt = 2,
  kShift = 4,
  kMeta = 8,
};

[[nodiscard]] constexpr ModifierKeys operator&(ModifierKeys a, ModifierKeys b) {
  return static_cast<ModifierKeys>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr ModifierKeys operator|(ModifierKeys a, ModifierKeys b) {
  return static_cast<ModifierKeys>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

constexpr ModifierKeys& operator|=(ModifierKeys& a, ModifierKeys b) { a = a | b; return a; }

[[nodiscard]] constexpr bool has_modifier(ModifierKeys mods, ModifierKeys mod) {
  return (mods & mod) != ModifierKeys::kNone;
}

struct MouseInput {
  MouseInputEvent event{};
  MouseButton button{};
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_delta_x{};
  std::int32_t int4_delta_y{};
  ModifierKeys mods{};
  std::int32_t int4_scroll_y{};
  std::int32_t int4_tap_count{1};
};

}
