// UPSTREAM: OpenRA.Platforms.Default/Sdl2Input.cs @7d57605 L19-252 + MultiTapDetection.cs L17-83
// 事件泵逐事件移植:修饰符泵前一次(SDL_GetModState)、motion 合并
// (pendingMotion 只保留最后一条,左/中/右键事件先 flush)、X1X2 转伪键盘
// (MOUSE4/MOUSE5)、滚轮位置取 SDL_GetMouseState、文本输入 UTF-8、
// Windows Alt+F4 与 macOS Cmd+Q 退出特例(退出以返回值上报,Phase 5 接 Game)。
// X1X2 分支的 IsRepeat 在上游读 e.key.repeat(mouse 事件 union 覆盖读,落点
// 是 which 的第二字节,实际恒 0);此处取 false,详见 docs/COVERAGE.md 输入偏离条目。
// The event pump ported event by event: modifiers sampled once before the
// pump (SDL_GetModState), motion coalescing (pendingMotion keeps only the
// last sample; left/middle/right button events flush it first), X1X2 as
// pseudo-keyboard (MOUSE4/MOUSE5), wheel positions from SDL_GetMouseState,
// UTF-8 text input, and the Windows Alt+F4 / macOS Cmd+Q exit special cases
// (exit is reported via the return value; Game wiring is Phase 5). The IsRepeat
// of the X1X2 branch reads e.key.repeat upstream (a union overlay on the mouse
// event landing in the second byte of `which`, effectively always 0); false is
// used here — see the input-deviation entry in docs/COVERAGE.md.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include <SDL2/SDL.h>  // 第三方 C 头(std_import_check 白名单登记) | third-party C header (registered in the std_import_check whitelist)

#include "platform/sdl2_input.hpp"

namespace ora {

// —— MultiTapDetection.cs L54-58 ——
// —— MultiTapDetection.cs L54-58 ——

bool TapHistory::CloseEnough(const TapRelease& v_a, const TapRelease& v_b) {
  // TimeSpan.FromMilliseconds(250) 以内的整数毫秒差 + int2.Length(<4)的
  // ISqrt 语义(int2.hpp 与上游 int2.cs L51 同式)。
  // An integer millennial difference within TimeSpan.FromMilliseconds(250)
  // plus the ISqrt semantics of int2.Length (int2.hpp matches int2.cs L51).
  return v_a.int8_time_ms - v_b.int8_time_ms < 250 &&
         (v_a.Location - v_b.Location).Length() < 4;
}

std::int32_t TapHistory::GetTapCount(int2 xy, std::int64_t int8_now_ms) {
  FirstRelease = SecondRelease;
  SecondRelease = ThirdRelease;
  ThirdRelease = TapRelease{int8_now_ms, xy};

  if (!CloseEnough(ThirdRelease, SecondRelease))
    return 1;
  if (!CloseEnough(SecondRelease, FirstRelease))
    return 2;
  return 3;
}

std::int32_t TapHistory::LastTapCount() const {
  if (!CloseEnough(ThirdRelease, SecondRelease))
    return 1;
  if (!CloseEnough(SecondRelease, FirstRelease))
    return 2;
  return 3;
}

// —— MultiTapDetection.cs L19-42:Cache 工厂 = 预填 1 秒前的 TapHistory ——
// —— MultiTapDetection.cs L19-42: the Cache factory = a TapHistory back-filled one second ——

TapHistory& MultiTapDetection::KeyHistory(Keycode key, Modifiers mods, std::int64_t int8_now_ms) {
  const std::uint64_t uint8_key = static_cast<std::uint64_t>(static_cast<std::int32_t>(key)) |
                                  static_cast<std::uint64_t>(static_cast<std::uint8_t>(mods)) << 32;
  const auto [it, b_inserted] = map_key_history_.try_emplace(uint8_key, int8_now_ms);
  return it->second;
}

TapHistory& MultiTapDetection::ClickHistory(std::uint8_t uint1_button, std::int64_t int8_now_ms) {
  const auto [it, b_inserted] = map_click_history_.try_emplace(uint1_button, int8_now_ms);
  return it->second;
}

std::int32_t MultiTapDetection::DetectFromMouse(std::uint8_t uint1_button, int2 xy, std::int64_t int8_now_ms) {
  return ClickHistory(uint1_button, int8_now_ms).GetTapCount(xy, int8_now_ms);
}

std::int32_t MultiTapDetection::InfoFromMouse(std::uint8_t uint1_button) const {
  const auto it = map_click_history_.find(uint1_button);
  return it != map_click_history_.end() ? it->second.LastTapCount() : 1;
}

std::int32_t MultiTapDetection::DetectFromKeyboard(Keycode key, Modifiers mods, std::int64_t int8_now_ms) {
  return KeyHistory(key, mods, int8_now_ms).GetTapCount(int2{0, 0}, int8_now_ms);
}

std::int32_t MultiTapDetection::InfoFromKeyboard(Keycode key, Modifiers mods) const {
  const auto it = map_key_history_.find(static_cast<std::uint64_t>(static_cast<std::int32_t>(key)) |
                                         static_cast<std::uint64_t>(static_cast<std::uint8_t>(mods)) << 32);
  return it != map_key_history_.end() ? it->second.LastTapCount() : 1;
}

// —— Sdl2Input.cs L26-41:按钮/修饰符换算 ——
// —— Sdl2Input.cs L26-41: button/modifier conversion ——

MouseButton Sdl2Input::MakeButton(std::uint8_t uint1_button) {
  return uint1_button == SDL_BUTTON_LEFT ? MouseButton::Left
       : uint1_button == SDL_BUTTON_RIGHT ? MouseButton::Right
       : uint1_button == SDL_BUTTON_MIDDLE ? MouseButton::Middle
       : MouseButton::None;
}

Modifiers Sdl2Input::MakeModifiers(std::uint16_t uint2_raw_kmod) {
  // LGUI/RGUI 各自测位后并入 Meta(上游 L36-40 的两行分别置 Meta)。
  // LGUI/RGUI are tested separately and merged into Meta (upstream L36-40 sets
  // Meta on each line).
  Modifiers mods = Modifiers::None;
  if ((uint2_raw_kmod & KMOD_ALT) != 0)
    mods |= Modifiers::Alt;
  if ((uint2_raw_kmod & KMOD_CTRL) != 0)
    mods |= Modifiers::Ctrl;
  if ((uint2_raw_kmod & KMOD_LGUI) != 0)
    mods |= Modifiers::Meta;
  if ((uint2_raw_kmod & KMOD_RGUI) != 0)
    mods |= Modifiers::Meta;
  if ((uint2_raw_kmod & KMOD_SHIFT) != 0)
    mods |= Modifiers::Shift;
  return mods;
}

std::int64_t Sdl2Input::DefaultNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

int2 Sdl2Input::EventPosition(const platform::Sdl2Window& device, std::int32_t int4_x, std::int32_t int4_y) {
#ifdef __APPLE__
  // macOS(Sdl2Input.cs L54-59):事件已是有效坐标,仅需在用户 scale 修饰
  // 生效时按 Native/Effective 比换算。scale 修饰器随 macOS 构建批次接线。
  // macOS (Sdl2Input.cs L54-59): events are already effective coordinates;
  // only a user scale modifier needs the Native/Effective ratio. The scale
  // modifier lands with the macOS build batch.
  return int2{int4_x, int4_y};
#else
  // Windows/Linux(Sdl2Input.cs L47-52):事件给的是表面坐标,须换算到有效
  // 窗口坐标;s = 1/EffectiveWindowScale = window/drawable = 1/HiDPI scale。
  // 分数部分远离零舍入:截断 (Sign(x)/2f + x*s)(C# (int) 向零,前置 ±0.5
  // 即构成远离零)。
  // Windows/Linux (Sdl2Input.cs L47-52): events arrive in surface
  // coordinates and must be scaled to effective-window ones;
  // s = 1/EffectiveWindowScale = window/drawable = 1/HiDPI scale. Fractional
  // parts round away from zero: truncate (Sign(x)/2f + x*s) (C# (int) goes
  // towards zero; prepending ±0.5 yields away-from-zero).
  const float float_scale = device.Geom().float_scale;
  if (float_scale != 1.0f) {
    const float float_s = 1.0f / float_scale;
    return int2{ScaleAwayFromZero(int4_x, float_s), ScaleAwayFromZero(int4_y, float_s)};
  }
  return int2{int4_x, int4_y};
#endif
}

std::string Sdl2Input::GetClipboardText() {
  char* str_sdl = SDL_GetClipboardText();
  std::string str_result = str_sdl != nullptr ? str_sdl : "";
  SDL_free(str_sdl);
  return str_result;
}

bool Sdl2Input::SetClipboardText(std::string_view str_text) {
  // string_view 非必 NUL 结尾:走 string 构造一次(C# 侧语义即值拷贝)。
  // A string_view is not necessarily NUL-terminated: route through a
  // one-shot string construction (the C# side value-copies anyway).
  return SDL_SetClipboardText(std::string(str_text).c_str()) == 0;
}

bool Sdl2Input::PumpInput(IInputHandler& handler, std::optional<int2> locked_mouse_position) {
  const Modifiers mods = MakeModifiers(SDL_GetModState());
  handler.ModifierKeys(mods);
  std::optional<MouseInput> pending_motion;
  bool b_quit_requested = false;

  SDL_Event event_sdl;
  while (SDL_PollEvent(&event_sdl) != 0) {
    switch (event_sdl.type) {
      case SDL_QUIT:
        // macOS 上拦截 Cmd+Q 免其直接退游戏(Sdl2Input.cs L74-78)
        // On macOS, intercept Cmd+Q so it does not abruptly exit (Sdl2Input.cs L74-78).
#ifdef __APPLE__
        if (!HasModifier(mods, Modifiers::Meta))
          b_quit_requested = true;
#else
        b_quit_requested = true;
#endif
        break;

      case SDL_WINDOWEVENT:
        window_.HandleWindowEvent(event_sdl.window.event);
        break;

      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
        // 鼠标 1/2/3 为鼠标输入;4/5 视作(伪)键盘输入(Sdl2Input.cs L117-118)
        // Mouse 1/2/3 are mouse inputs; 4/5 are (pseudo) keyboard inputs
        // (Sdl2Input.cs L117-118).
        if (event_sdl.button.button == SDL_BUTTON_LEFT ||
            event_sdl.button.button == SDL_BUTTON_MIDDLE ||
            event_sdl.button.button == SDL_BUTTON_RIGHT) {
          if (pending_motion.has_value()) {
            handler.OnMouseInput(*pending_motion);
            pending_motion.reset();
          }

          const MouseButton button = MakeButton(event_sdl.button.button);
          if (event_sdl.type == SDL_MOUSEBUTTONDOWN)
            last_button_bits_ = last_button_bits_ | button;
          else
            last_button_bits_ = static_cast<MouseButton>(
                static_cast<std::uint8_t>(last_button_bits_) & ~static_cast<std::uint8_t>(button));

          const int2 input = locked_mouse_position.value_or(int2{event_sdl.button.x, event_sdl.button.y});
          const int2 pos = EventPosition(window_, input.X, input.Y);

          if (event_sdl.type == SDL_MOUSEBUTTONDOWN)
            handler.OnMouseInput(MouseInput{
                .Event = MouseInputEvent::Down, .Button = button, .Location = pos,
                .Delta = int2{0, 0}, .ModifierFlags = mods,
                .MultiTapCount = multitap_.DetectFromMouse(event_sdl.button.button, pos, fn_now_ms_())});
          else
            handler.OnMouseInput(MouseInput{
                .Event = MouseInputEvent::Up, .Button = button, .Location = pos,
                .Delta = int2{0, 0}, .ModifierFlags = mods,
                .MultiTapCount = multitap_.InfoFromMouse(event_sdl.button.button)});
        }

        if (event_sdl.button.button == SDL_BUTTON_X1 ||
            event_sdl.button.button == SDL_BUTTON_X2) {
          const Keycode key_code = event_sdl.button.button == SDL_BUTTON_X1
                                       ? Keycode::MOUSE4
                                       : Keycode::MOUSE5;
          const KeyInputEvent type = event_sdl.type == SDL_MOUSEBUTTONDOWN
                                         ? KeyInputEvent::Down
                                         : KeyInputEvent::Up;
          // 上游 UnicodeChar 字面 '?';IsRepeat 恒 false(见文件头注)
          // Upstream's UnicodeChar is the literal '?'; IsRepeat is false (see the header note).
          handler.OnKeyInput(KeyInput{
              .Event = type, .Key = key_code, .ModifierFlags = mods,
              .MultiTapCount = event_sdl.type == SDL_MOUSEBUTTONDOWN
                                   ? multitap_.DetectFromKeyboard(key_code, mods, fn_now_ms_())
                                   : multitap_.InfoFromKeyboard(key_code, mods),
              .UnicodeChar = u'?', .IsRepeat = false});
        }
        break;
      }

      case SDL_MOUSEMOTION: {
        const int2 mouse_pos{event_sdl.motion.x, event_sdl.motion.y};
        const int2 input = locked_mouse_position.value_or(mouse_pos);
        const int2 pos = EventPosition(window_, input.X, input.Y);

        const int2 delta = !locked_mouse_position.has_value()
                               ? EventPosition(window_, event_sdl.motion.xrel, event_sdl.motion.yrel)
                               : mouse_pos - *locked_mouse_position;

        pending_motion = MouseInput{
            .Event = MouseInputEvent::Move, .Button = last_button_bits_, .Location = pos,
            .Delta = delta, .ModifierFlags = mods, .MultiTapCount = 0};
        break;
      }

      case SDL_MOUSEWHEEL: {
        std::int32_t int4_x = 0, int4_y = 0;
        SDL_GetMouseState(&int4_x, &int4_y);
        const int2 pos = EventPosition(window_, int4_x, int4_y);
        handler.OnMouseInput(MouseInput{
            .Event = MouseInputEvent::Scroll, .Button = MouseButton::None, .Location = pos,
            .Delta = int2{0, event_sdl.wheel.y}, .ModifierFlags = mods, .MultiTapCount = 0});
        break;
      }

      case SDL_TEXTINPUT:
        handler.OnTextInput(std::string_view(event_sdl.text.text));
        break;

      case SDL_KEYDOWN:
      case SDL_KEYUP: {
        const auto key_code = static_cast<Keycode>(event_sdl.key.keysym.sym);
        const KeyInputEvent type = event_sdl.type == SDL_KEYDOWN ? KeyInputEvent::Down : KeyInputEvent::Up;

        const std::int32_t int4_tap = event_sdl.type == SDL_KEYDOWN
                                          ? multitap_.DetectFromKeyboard(key_code, mods, fn_now_ms_())
                                          : multitap_.InfoFromKeyboard(key_code, mods);

        const KeyInput key_event{
            .Event = type, .Key = key_code, .ModifierFlags = mods,
            .MultiTapCount = int4_tap,
            .UnicodeChar = static_cast<char16_t>(event_sdl.key.keysym.sym),
            .IsRepeat = event_sdl.key.repeat != 0};

        // Windows 用户特例:Alt+F4 直接退出(Sdl2Input.cs L236-239)
        // Windows special case: Alt+F4 exits directly (Sdl2Input.cs L236-239).
#ifdef _WIN32
        if (event_sdl.key.keysym.sym == SDLK_F4 && HasModifier(mods, Modifiers::Alt))
          b_quit_requested = true;
        else
          handler.OnKeyInput(key_event);
#else
        handler.OnKeyInput(key_event);
#endif
        break;
      }

      default:
        break;
    }
  }

  if (pending_motion.has_value())
    handler.OnMouseInput(*pending_motion);
  return b_quit_requested;
}

}  // namespace ora
