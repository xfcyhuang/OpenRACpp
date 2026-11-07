#pragma once

import std;

#include "game/input/input_events.hpp"

namespace ora::game::input {

// 上游 OpenRA.Game/Input/Hotkey.cs 的纯值对 (Keycode, Modifiers)。
struct HotkeyModel {
  Keycode key{Keycode::kUnknown};
  ModifierKeys mods{ModifierKeys::kNone};

  [[nodiscard]] friend bool operator==(const HotkeyModel&, const HotkeyModel&) = default;
};

inline constexpr HotkeyModel kHotkeyInvalid{};

[[nodiscard]] constexpr bool hotkey_is_valid(const HotkeyModel& hk) {
  return hk.key != Keycode::kUnknown;
}

// 上游 Hotkey.operator==（Hotkey.cs:59-66）：左操作数 key 为 UNKNOWN 一票否决，
// Invalid == Invalid 亦为 false。冲突检测/改键写回/激活判定落点全部经此函数。
[[nodiscard]] constexpr bool hotkey_equal(const HotkeyModel& lhs, const HotkeyModel& rhs) {
  if (lhs.key == Keycode::kUnknown) return false;
  return lhs.key == rhs.key && lhs.mods == rhs.mods;
}

// 上游 Hotkey.TryParse（Hotkey.cs:27-45）：首个空格切两段，key 段枚举名
// 忽略大小写，mods 段逗号分隔 flags 忽略大小写；三 token（如 "A B C"）拒绝。
[[nodiscard]] std::expected<HotkeyModel, std::string> hotkey_try_parse(std::string_view str_text);

// 上游 Hotkey.ToString()："KEY Mod1, Mod2"（Modifiers:F canonical 序）。
[[nodiscard]] std::string hotkey_to_string(const HotkeyModel& hk);

[[nodiscard]] std::optional<Keycode> keycode_from_name(std::string_view str_name);
[[nodiscard]] std::string_view keycode_name(Keycode key);

[[nodiscard]] std::optional<ModifierKeys> modifiers_from_string(std::string_view str_text);
[[nodiscard]] std::string modifiers_to_string(ModifierKeys mods);

// 上游 HotkeyReference.IsActivatedBy（HotkeyReference.cs:40-44）：Key 与
// Modifiers **双双相等**（全匹配，不做按位包含）；不走 UNKNOWN 否决的 hotkey_equal。
[[nodiscard]] constexpr bool hotkey_is_activated_by(const HotkeyModel& hk, const KeyInput& ki) {
  return hk.key == ki.key && hk.mods == ki.mods;
}

// 上游 Hotkey.FromKeyInput。
[[nodiscard]] constexpr HotkeyModel hotkey_from_key_input(const KeyInput& ki) {
  return HotkeyModel{ki.key, ki.mods};
}

}
