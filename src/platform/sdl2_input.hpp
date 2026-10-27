#pragma once
import std;

#include "core/int2.hpp"
#include "platform/keycode.hpp"
#include "platform/sdl2_window.hpp"

namespace ora {

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
  return (static_cast<std::uint8_t>(v_m) & static_cast<std::uint8_t>(v_flag)) ==
         static_cast<std::uint8_t>(v_flag);
}

struct MouseInput {
  MouseInputEvent Event;
  MouseButton Button;
  int2 Location;
  int2 Delta;
  Modifiers ModifierFlags;
  std::int32_t MultiTapCount;
};

enum class KeyInputEvent : std::uint8_t { Down, Up };

struct KeyInput {
  KeyInputEvent Event;
  Keycode Key;
  Modifiers ModifierFlags;
  std::int32_t MultiTapCount;
  char16_t UnicodeChar;
  bool IsRepeat;
};

struct IInputHandler {
  virtual ~IInputHandler() = default;
  virtual void ModifierKeys(Modifiers mods) = 0;
  virtual void OnKeyInput(const KeyInput& input) = 0;
  virtual void OnMouseInput(const MouseInput& input) = 0;
  virtual void OnTextInput(std::string_view str_text) = 0;
};

struct NullInputHandler : IInputHandler {
  void ModifierKeys(Modifiers) override {}
  void OnKeyInput(const KeyInput&) override {}
  void OnMouseInput(const MouseInput&) override {}
  void OnTextInput(std::string_view) override {}
};

struct TapRelease {
  std::int64_t int8_time_ms = 0;
  int2 Location{0, 0};
};

class TapHistory {
 public:

  explicit TapHistory(std::int64_t int8_now_ms)
      : FirstRelease{int8_now_ms - 1000, int2{0, 0}},
        SecondRelease{int8_now_ms - 1000, int2{0, 0}},
        ThirdRelease{int8_now_ms - 1000, int2{0, 0}} {}

  static bool CloseEnough(const TapRelease& v_a, const TapRelease& v_b);

  std::int32_t GetTapCount(int2 xy, std::int64_t int8_now_ms);

  std::int32_t LastTapCount() const;

  TapRelease FirstRelease, SecondRelease, ThirdRelease;
};

class MultiTapDetection {
 public:
  std::int32_t DetectFromMouse(std::uint8_t uint1_button, int2 xy, std::int64_t int8_now_ms);
  std::int32_t InfoFromMouse(std::uint8_t uint1_button) const;
  std::int32_t DetectFromKeyboard(Keycode key, Modifiers mods, std::int64_t int8_now_ms);
  std::int32_t InfoFromKeyboard(Keycode key, Modifiers mods) const;

 private:
  TapHistory& KeyHistory(Keycode key, Modifiers mods, std::int64_t int8_now_ms);
  TapHistory& ClickHistory(std::uint8_t uint1_button, std::int64_t int8_now_ms);

  std::map<std::uint64_t, TapHistory> map_key_history_;
  std::map<std::uint8_t, TapHistory> map_click_history_;
};

class Sdl2Input {
 public:

  using FnNowMs = std::int64_t (*)();

  explicit Sdl2Input(platform::Sdl2Window& window, FnNowMs fn_now_ms = DefaultNowMs)
      : window_(window), fn_now_ms_(fn_now_ms) {}

  bool PumpInput(IInputHandler& handler, std::optional<int2> locked_mouse_position = std::nullopt);

  static std::string GetClipboardText();
  static bool SetClipboardText(std::string_view str_text);

  static int2 EventPosition(const platform::Sdl2Window& device, std::int32_t int4_x, std::int32_t int4_y);

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
  MouseButton last_button_bits_ = MouseButton::None;
};

}
