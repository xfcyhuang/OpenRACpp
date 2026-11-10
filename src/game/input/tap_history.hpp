#pragma once

import std;

#include "game/input/input_events.hpp"

namespace ora::game::input {

// 上游 OpenRA.Platforms.Default/MultiTapDetection.cs 的三槽移位寄存器
// （TapHistory，:19-83）。墙钟语义：Δt<250ms（相邻对链判）且位移欧氏长度 <4
// （C++ 整型实现 = dx*dx+dy*dy < 16）。时钟注入式，无头可测。
struct TapSlot {
  std::int64_t int8_time_ms{};
  std::int32_t int4_x{};
  std::int32_t int4_y{};
};

struct TapHistoryState {
  TapSlot slot_first{};
  TapSlot slot_second{};
  TapSlot slot_third{};
};

// 上游桶初始化 = (now-1s, Zero)：首击 Third-Second 判"不近"→ count=1。
void tap_history_init(TapHistoryState& st, std::int64_t int8_now_ms);

// 上游 GetTapCount（:60-72，Down 事件调）：先移位再判。
[[nodiscard]] std::int32_t tap_history_detect(TapHistoryState& st, std::int64_t int8_now_ms,
                                              std::int32_t int4_x, std::int32_t int4_y);

// 上游 LastTapCount（:74-82，Up 事件调）：同判定不移位（回放上次 Down 的计数）。
[[nodiscard]] std::int32_t tap_history_info(const TapHistoryState& st);

// 键盘按 (Keycode, Modifiers) 分桶、鼠标按按钮字节分桶（上游 Cache 双桶）。
struct TapHistoryKeyboard {
  std::map<std::pair<std::uint32_t, std::uint8_t>, TapHistoryState> map_buckets;
};

struct TapHistoryMouse {
  std::map<std::uint8_t, TapHistoryState> map_buckets;
};

[[nodiscard]] std::int32_t tap_detect_mouse(TapHistoryMouse& taps, std::uint8_t u1_button,
                                            std::int64_t int8_now_ms, std::int32_t int4_x,
                                            std::int32_t int4_y);

[[nodiscard]] std::int32_t tap_info_mouse(const TapHistoryMouse& taps, std::uint8_t u1_button);

[[nodiscard]] std::int32_t tap_detect_keyboard(TapHistoryKeyboard& taps, Keycode key,
                                               ModifierKeys mods, std::int64_t int8_now_ms);

[[nodiscard]] std::int32_t tap_info_keyboard(const TapHistoryKeyboard& taps, Keycode key,
                                             ModifierKeys mods);

}
