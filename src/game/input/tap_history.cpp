module ora.game.input;

import std;

namespace ora::game::input {

namespace {

constexpr std::int64_t kTapWindowMs{250};
constexpr std::int64_t kTapInitOffsetMs{1000};
constexpr std::int64_t kTapDistanceSqThreshold{16};  // 欧氏长度 <4 的整型平方形式

// 上游 CloseEnough（MultiTapDetection.cs:47-52）：a.Time - b.Time < 250ms
// （有符号比较，照抄）且位移欧氏长度 <4。
bool tap_close_enough(const TapSlot& slot_a, const TapSlot& slot_b) {
  const std::int64_t int8_dt{slot_a.int8_time_ms - slot_b.int8_time_ms};
  if (int8_dt >= kTapWindowMs) return false;
  const std::int64_t int8_dx{slot_a.int4_x - slot_b.int4_x};
  const std::int64_t int8_dy{slot_a.int4_y - slot_b.int4_y};
  return int8_dx * int8_dx + int8_dy * int8_dy < kTapDistanceSqThreshold;
}

std::int32_t tap_judge(const TapHistoryState& st) {
  if (!tap_close_enough(st.slot_third, st.slot_second)) return 1;
  if (!tap_close_enough(st.slot_second, st.slot_first)) return 2;
  return 3;
}

TapHistoryState& keyboard_bucket(TapHistoryKeyboard& taps, Keycode key, ModifierKeys mods,
                                 std::int64_t int8_now_ms) {
  const std::pair<std::uint32_t, std::uint8_t> bucket_key{
      static_cast<std::uint32_t>(key), static_cast<std::uint8_t>(mods)};
  auto [it, inserted]{taps.map_buckets.try_emplace(
      bucket_key, TapHistoryState{TapSlot{int8_now_ms - kTapInitOffsetMs, 0, 0},
                                  TapSlot{int8_now_ms - kTapInitOffsetMs, 0, 0},
                                  TapSlot{int8_now_ms - kTapInitOffsetMs, 0, 0}})};
  return it->second;
}

TapHistoryState& mouse_bucket(TapHistoryMouse& taps, std::uint8_t u1_button,
                              std::int64_t int8_now_ms) {
  auto [it, inserted]{taps.map_buckets.try_emplace(
      u1_button, TapHistoryState{TapSlot{int8_now_ms - kTapInitOffsetMs, 0, 0},
                                 TapSlot{int8_now_ms - kTapInitOffsetMs, 0, 0},
                                 TapSlot{int8_now_ms - kTapInitOffsetMs, 0, 0}})};
  return it->second;
}

}

void tap_history_init(TapHistoryState& st, std::int64_t int8_now_ms) {
  st.slot_first = TapSlot{int8_now_ms - kTapInitOffsetMs, 0, 0};
  st.slot_second = st.slot_first;
  st.slot_third = st.slot_first;
}

std::int32_t tap_history_detect(TapHistoryState& st, std::int64_t int8_now_ms, std::int32_t int4_x,
                                std::int32_t int4_y) {
  st.slot_first = st.slot_second;
  st.slot_second = st.slot_third;
  st.slot_third = TapSlot{int8_now_ms, int4_x, int4_y};
  return tap_judge(st);
}

std::int32_t tap_history_info(const TapHistoryState& st) { return tap_judge(st); }

std::int32_t tap_detect_mouse(TapHistoryMouse& taps, std::uint8_t u1_button,
                              std::int64_t int8_now_ms, std::int32_t int4_x, std::int32_t int4_y) {
  return tap_history_detect(mouse_bucket(taps, u1_button, int8_now_ms), int8_now_ms, int4_x,
                            int4_y);
}

std::int32_t tap_info_mouse(const TapHistoryMouse& taps, std::uint8_t u1_button) {
  const auto it{taps.map_buckets.find(u1_button)};
  if (it == taps.map_buckets.end()) return 1;
  return tap_history_info(it->second);
}

std::int32_t tap_detect_keyboard(TapHistoryKeyboard& taps, Keycode key, ModifierKeys mods,
                                 std::int64_t int8_now_ms) {
  return tap_history_detect(keyboard_bucket(taps, key, mods, int8_now_ms), int8_now_ms, 0, 0);
}

std::int32_t tap_info_keyboard(const TapHistoryKeyboard& taps, Keycode key, ModifierKeys mods) {
  const auto it{taps.map_buckets.find(
      std::pair<std::uint32_t, std::uint8_t>{static_cast<std::uint32_t>(key),
                                             static_cast<std::uint8_t>(mods)})};
  if (it == taps.map_buckets.end()) return 1;
  return tap_history_info(it->second);
}

}
