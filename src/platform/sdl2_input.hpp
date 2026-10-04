// UPSTREAM: OpenRA.Game/Input/IInputHandler.cs @7d57605 L17-83(数据面)+
// OpenRA.Platforms.Default/Sdl2Input.cs L19-252(事件泵)+ MultiTapDetection.cs L17-83(多击检测)
// 形态适配:上游 PumpInput 内对 Game.Exit() 的直接调用改为返回值 b_quit_requested
// (Game 编排归 Phase 5);TapHistory/MultiTapDetection 的 DateTime.Now 改为
// steady_clock 毫秒注入(单调时钟;上游 DateTime.Now 的偶发回退不是契约行为),
// 键盘缓存键打包为 u64(keycode | mods<<32)替代 (Keycode, Modifiers) 元组键。
// The data plane of IInputHandler.cs plus the event pump of Sdl2Input.cs and
// the multi-tap detection of MultiTapDetection.cs. Shape adaptations: the
// direct Game.Exit() calls inside upstream's PumpInput become a returned
// b_quit_requested (Game orchestration is Phase 5); TapHistory/MultiTapDetection
// take steady-clock milliseconds injected instead of DateTime.Now (a monotonic
// clock; upstream's incidental DateTime.Now setbacks are not contract
// behavior), and the keyboard cache key packs into u64 (keycode | mods<<32)
// in place of the (Keycode, Modifiers) tuple key.
#pragma once
import std;

#include "core/int2.hpp"
#include "platform/keycode.hpp"
#include "platform/sdl2_window.hpp"

namespace ora {

// —— IInputHandler.cs L25-45:输入事件数据面(枚举值/位值逐字对照)——
// —— IInputHandler.cs L25-45: the input-event data plane (enum/bit values verbatim) ——

enum class MouseInputEvent : std::uint8_t { Down, Move, Up, Scroll };

enum class MouseButton : std::uint8_t {
  None = 0,
  Left = 1,
  Right = 2,
  Middle = 4,
};

constexpr MouseButton operator|(MouseButton v_a, MouseButton v_b) {
  return static_cast<MouseButton>(static_cast<std::uint8_t>(v_a) | static_cast<std::uint8_t>(v_b));
}
constexpr MouseButton& operator|=(MouseButton& v_a, MouseButton v_b) { v_a = v_a | v_b; return v_a; }
constexpr MouseButton operator&(MouseButton v_a, MouseButton v_b) {
  return static_cast<MouseButton>(static_cast<std::uint8_t>(v_a) & static_cast<std::uint8_t>(v_b));
}
constexpr MouseButton operator~(MouseButton v_a) {
  return static_cast<MouseButton>(~static_cast<std::uint8_t>(v_a) & 0x07);
}

enum class Modifiers : std::uint8_t {
  None = 0,
  Shift = 1,
  Alt = 2,
  Ctrl = 4,
  Meta = 8,
};

constexpr Modifiers operator|(Modifiers v_a, Modifiers v_b) {
  return static_cast<Modifiers>(static_cast<std::uint8_t>(v_a) | static_cast<std::uint8_t>(v_b));
}
constexpr Modifiers& operator|=(Modifiers& v_a, Modifiers v_b) { v_a = v_a | v_b; return v_a; }
constexpr bool HasModifier(Modifiers v_m, Modifiers v_flag) {
  return (static_cast<std::uint8_t>(v_m) & static_cast<std::uint8_t>(v_flag)) != 0;
}

/// 鼠标输入事件(IInputHandler.cs L26 的 record struct;字段名保留上游)。
/// A mouse-input event (the record struct of IInputHandler.cs L26; upstream
/// field names kept).
struct MouseInput {
  MouseInputEvent Event;
  MouseButton Button;
  int2 Location;
  int2 Delta;
  Modifiers ModifierFlags;  // 上游字段名 Modifiers 与类型名同名,C++ 加后缀 | clashes with the type name in C++, hence the suffix
  std::int32_t MultiTapCount;
};

enum class KeyInputEvent : std::uint8_t { Down, Up };

/// 键盘输入事件(IInputHandler.cs L75-83)。UnicodeChar 保留上游 (char)sym
/// 的 16 位截断语义,故为 char16_t。
/// A key-input event (IInputHandler.cs L75-83). UnicodeChar keeps upstream's
/// 16-bit (char)sym truncation semantics, hence char16_t.
struct KeyInput {
  KeyInputEvent Event;
  Keycode Key;
  Modifiers ModifierFlags;
  std::int32_t MultiTapCount;
  char16_t UnicodeChar;
  bool IsRepeat;
};

/// 输入汇(IInputHandler.cs L17-23)。实现在主线程被 PumpInput 调用。
/// The input sink (IInputHandler.cs L17-23); implementations are invoked on
/// the main thread by PumpInput.
struct IInputHandler {
  virtual ~IInputHandler() = default;
  virtual void ModifierKeys(Modifiers mods) = 0;
  virtual void OnKeyInput(const KeyInput& input) = 0;
  virtual void OnMouseInput(const MouseInput& input) = 0;
  virtual void OnTextInput(std::string_view str_text) = 0;
};

/// 空汇(InputHandler.cs L14-21)。
/// The null sink (InputHandler.cs L14-21).
struct NullInputHandler : IInputHandler {
  void ModifierKeys(Modifiers) override {}
  void OnKeyInput(const KeyInput&) override {}
  void OnMouseInput(const MouseInput&) override {}
  void OnTextInput(std::string_view) override {}
};

// —— MultiTapDetection.cs L45-83:三槽多击历史 ——
// —— MultiTapDetection.cs L45-83: the three-slot tap history ——

/// 一次释放的时间与位置(MultiTapDetection.cs L47 的 (DateTime, int2) 元组)。
/// One release's time and location (the (DateTime, int2) tuple of
/// MultiTapDetection.cs L47).
struct TapRelease {
  std::int64_t int8_time_ms = 0;  // steady_clock 毫秒 | steady-clock milliseconds
  int2 Location{0, 0};
};

class TapHistory {
 public:
  /// 构造预填 1 秒前的过去时间(MultiTapDetection.cs L50:三槽 CloseEnough
  /// 初始为 false,首次 GetTapCount 必得 1)。
  /// The constructor back-fills all slots with a time one second in the past
  /// (MultiTapDetection.cs L50: CloseEnough is initially false, so the first
  /// GetTapCount must return 1).
  explicit TapHistory(std::int64_t int8_now_ms)
      : FirstRelease{int8_now_ms - 1000, int2{0, 0}},
        SecondRelease{int8_now_ms - 1000, int2{0, 0}},
        ThirdRelease{int8_now_ms - 1000, int2{0, 0}} {}

  static bool CloseEnough(const TapRelease& v_a, const TapRelease& v_b);

  /// 记录一次释放并给出连击数(MultiTapDetection.cs L60-72)。
  /// Records one release and yields the tap count (MultiTapDetection.cs L60-72).
  std::int32_t GetTapCount(int2 xy, std::int64_t int8_now_ms);

  /// 只读给出最近一次的连击数(MultiTapDetection.cs L74-82;Up 事件用)。
  /// Read-only tap count of the most recent release (MultiTapDetection.cs
  /// L74-82; used by Up events).
  std::int32_t LastTapCount() const;

  TapRelease FirstRelease, SecondRelease, ThirdRelease;
};

/// 多击检测(MultiTapDetection.cs L17-43;上游 static + Cache,此处实例化
/// 以持有注入时钟 —— 一窗口一实例,主线程专用)。缓存永不过期(上游
/// Cache = 无 TTL 的工厂字典),键盘历史会随 (键, 修饰符) 组合数缓慢增长,
/// 与上游一致。
/// Multi-tap detection (MultiTapDetection.cs L17-43; upstream is static with
/// Cache — instantiated here to hold the injected clock: one instance per
/// window, main thread only). The caches never expire (upstream's Cache is a
/// TTL-free factory dictionary); the keyboard history grows slowly with the
/// (key, modifier) combinations, matching upstream.
class MultiTapDetection {
 public:
  std::int32_t DetectFromMouse(std::uint8_t uint1_button, int2 xy, std::int64_t int8_now_ms);
  std::int32_t InfoFromMouse(std::uint8_t uint1_button) const;
  std::int32_t DetectFromKeyboard(Keycode key, Modifiers mods, std::int64_t int8_now_ms);
  std::int32_t InfoFromKeyboard(Keycode key, Modifiers mods) const;

 private:
  TapHistory& KeyHistory(Keycode key, Modifiers mods, std::int64_t int8_now_ms);
  TapHistory& ClickHistory(std::uint8_t uint1_button, std::int64_t int8_now_ms);

  std::map<std::uint64_t, TapHistory> map_key_history_;   // 键 = keycode | mods<<32 | key = keycode | mods<<32
  std::map<std::uint8_t, TapHistory> map_click_history_;
};

/// SDL2 事件泵 → 引擎输入事件(Sdl2Input.cs L19-252)。
/// The SDL2 event pump feeding engine input events (Sdl2Input.cs L19-252).
class Sdl2Input {
 public:
  /// 时钟源(默认 steady_clock 毫秒;测试可注入)。
  /// The clock source (steady-clock milliseconds by default; injectable for tests).
  using FnNowMs = std::int64_t (*)();

  explicit Sdl2Input(platform::Sdl2Window& window, FnNowMs fn_now_ms = DefaultNowMs)
      : window_(window), fn_now_ms_(fn_now_ms) {}

  /// 泵一轮事件并分发到 handler(Sdl2Input.cs L64-250)。
  /// 返回是否收到退出请求(SDL_QUIT,或 Windows 下 Alt+F4 / macOS 下 Cmd+Q
  /// —— 上游直接调 Game.Exit(),退出编排归 Phase 5 的调用方)。
  /// locked_mouse_position 非空时,所有位置类事件改用该坐标(相对鼠标模式)。
  /// Pumps one round of events and dispatches to the handler
  /// (Sdl2Input.cs L64-250). Returns whether an exit request was received
  /// (SDL_QUIT, or Alt+F4 on Windows / Cmd+Q on macOS — upstream calls
  /// Game.Exit() directly here; the exit orchestration belongs to the Phase 5
  /// caller). When locked_mouse_position is engaged, all positional events
  /// use that coordinate instead (relative-mouse mode).
  bool PumpInput(IInputHandler& handler, std::optional<int2> locked_mouse_position = std::nullopt);

  static std::string GetClipboardText();
  static bool SetClipboardText(std::string_view str_text);

  /// 事件坐标 → 有效窗口坐标(Sdl2Input.cs L43-62)。Windows/Linux:表面坐标
  /// 须除以 HiDPI scale;分数部分按远离零舍入(Math.Sign(v)/2f + v*s 后截断)。
  /// Event coordinates → effective-window coordinates (Sdl2Input.cs L43-62).
  /// Windows/Linux: surface coordinates must be divided by the HiDPI scale;
  /// fractional parts round away from zero (truncate after Math.Sign(v)/2f + v*s).
  static int2 EventPosition(const platform::Sdl2Window& device, std::int32_t int4_x, std::int32_t int4_y);

  /// EventPosition 的单轴舍入(C# (int)(Math.Sign(v)/2f + v*s) 逐语义;
  /// 提取为纯函数以供单测固化边界)。
  /// The per-axis rounding of EventPosition (the exact semantics of C#
  /// (int)(Math.Sign(v)/2f + v*s); extracted as a pure function so tests can
  /// pin the boundaries).
  static std::int32_t ScaleAwayFromZero(std::int32_t int4_v, float float_s) {
    const float float_round = static_cast<float>(int2::SignOf(int4_v)) / 2.0f;
    return static_cast<std::int32_t>(float_round + int4_v * float_s);
  }

  static MouseButton MakeButton(std::uint8_t uint1_button);
  static Modifiers MakeModifiers(std::uint16_t uint2_raw_kmod);

  static std::int64_t DefaultNowMs();

 private:
  platform::Sdl2Window& window_;
  MultiTapDetection multitap_;
  FnNowMs fn_now_ms_;
  MouseButton last_button_bits_ = MouseButton::None;  // 按下位图(Sdl2Input.cs L21)| the pressed-buttons bitmap
};

}  // namespace ora
