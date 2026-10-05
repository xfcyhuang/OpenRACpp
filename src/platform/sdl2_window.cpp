// UPSTREAM: OpenRA.Platforms.Default/Sdl2PlatformWindow.cs @b6fc03f L124-215,270-353,538-587
// 实现:窗口创建/模式切换式样、GL 属性序列(DOUBLEBUFFER=1,RGB888,ALPHA=0;Modern=3.2
// Core / Embedded=3.0 ES)、HiDPI scale = drawable/window(Sdl2PlatformWindow.cs L270-276)、
// 事件泵。几何快照为打包原子量(OPT-B5)。
// Implementation: window creation/mode styles, the GL attribute sequence
// (DOUBLEBUFFER=1, RGB888, ALPHA=0; Modern=3.2 Core / Embedded=3.0 ES), HiDPI
// scale = drawable/window (Sdl2PlatformWindow.cs L270-276), and the event
// pump. The geometry snapshot is a packed atomic (OPT-B5).
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include <SDL2/SDL.h>  // 第三方 C 头(std_import_check 白名单登记) | third-party C header (registered in the std_import_check whitelist)

#include "platform/sdl2_window.hpp"

namespace ora::platform {

namespace {

/// 几何打包:int4_width(16)|int4_height(16)|scale 8.8 定点(16)|保留(16)
/// Geometry packing: width(16) | height(16) | scale in 8.8 fixed point (16) | reserved (16).
std::uint64_t PackGeom(std::int32_t int4_width, std::int32_t int4_height, float float_scale) {
  const std::uint16_t uint2_w = static_cast<std::uint16_t>(int4_width);
  const std::uint16_t uint2_h = static_cast<std::uint16_t>(int4_height);
  const std::uint16_t uint2_scale = static_cast<std::uint16_t>(float_scale * 256.0f);
  return static_cast<std::uint64_t>(uint2_w) | (static_cast<std::uint64_t>(uint2_h) << 16) |
         (static_cast<std::uint64_t>(uint2_scale) << 32);
}

SDL_WindowFlags ToSdlFlags(WindowMode mode_window, bool b_hidden) {
  std::uint32_t uint4_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
  if (b_hidden)
    uint4_flags |= SDL_WINDOW_HIDDEN;
  switch (mode_window) {
    case WindowMode::Fullscreen:
      uint4_flags |= SDL_WINDOW_FULLSCREEN;
      break;
    case WindowMode::PseudoFullscreen:
      uint4_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
      break;
    case WindowMode::Windowed:
      break;
  }
  return static_cast<SDL_WindowFlags>(uint4_flags);
}

}  // namespace

bool InitSdl2Video() {
  static std::int32_t int4_refs = 0;
  if (int4_refs++ == 0) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
      std::println(stderr, "SDL_INIT_VIDEO 失败 | SDL_INIT_VIDEO failed: {}", SDL_GetError());
      --int4_refs;
      return false;
    }
  }
  return true;
}

void ShutdownSdl2Video() {
  // 引用计数与 InitSdl2Video 成对(规范:显式生命周期);进程退出由 SDL_Quit 收尾
  // Reference counting pairs with InitSdl2Video (guideline: explicit
  // lifetimes); SDL_Quit finalizes at process exit.
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

std::optional<Sdl2Window> Sdl2Window::Create(const Desc& desc) {
  if (!InitSdl2Video())
    return std::nullopt;

  // GL 属性序列(Sdl2PlatformWindow.cs L538-542 + L548-559)
  // The GL attribute sequence (Sdl2PlatformWindow.cs L538-542 + L548-559).
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
  switch (desc.kind_profile) {
    case GLProfileKind::Modern:
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
      break;
    case GLProfileKind::Embedded:
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
      break;
  }

  SDL_Window* ptr_window = SDL_CreateWindow(
      "OpenRACpp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      desc.int4_width, desc.int4_height, ToSdlFlags(desc.mode_window, desc.b_hidden));
  if (ptr_window == nullptr) {
    std::println(stderr, "SDL_CreateWindow 失败 | SDL_CreateWindow failed: {}", SDL_GetError());
    return std::nullopt;
  }

  Sdl2Window window_result;
  window_result.ptr_window_ = ptr_window;

  // 初始几何快照:drawable/window 比即 HiDPI scale(Sdl2PlatformWindow.cs L270-276)
  // Initial geometry snapshot: drawable/window ratio is the HiDPI scale
  // (Sdl2PlatformWindow.cs L270-276).
  int int4_win_w = 0, int4_win_h = 0, int4_draw_w = 0, int4_draw_h = 0;
  SDL_GetWindowSize(ptr_window, &int4_win_w, &int4_win_h);
  SDL_GL_GetDrawableSize(ptr_window, &int4_draw_w, &int4_draw_h);
  const float float_scale = int4_win_w > 0 ? static_cast<float>(int4_draw_w) / int4_win_w : 1.0f;
  window_result.uint8_geom_.store(PackGeom(int4_win_w, int4_win_h, float_scale), std::memory_order_relaxed);
  return window_result;
}

Sdl2Window::Sdl2Window(Sdl2Window&& other) noexcept
    : ptr_window_(std::exchange(other.ptr_window_, nullptr)),
      ptr_gl_context_(std::exchange(other.ptr_gl_context_, nullptr)),
      uint8_geom_(other.uint8_geom_.load(std::memory_order_relaxed)) {}

Sdl2Window::~Sdl2Window() {
  if (ptr_gl_context_ != nullptr)
    SDL_GL_DeleteContext(ptr_gl_context_);
  if (ptr_window_ != nullptr)
    SDL_DestroyWindow(static_cast<SDL_Window*>(ptr_window_));
}

void* Sdl2Window::CreateGlContext() {
  SDL_GLContext ptr_context = SDL_GL_CreateContext(static_cast<SDL_Window*>(ptr_window_));
  if (ptr_context == nullptr) {
    std::println(stderr, "SDL_GL_CreateContext 失败 | SDL_GL_CreateContext failed: {}", SDL_GetError());
    return nullptr;
  }
  // 记录到成员:窗口析构时删除(第二批复核修正 —— 第一批漏存,上下文
  // 泄漏悬挂于已销毁窗口,后续上下文创建时驱动崩)。
  // Record on the member so the window destructor deletes it (second-batch
  // review fix — the first batch never stored it; the leaked context dangled
  // off a destroyed window and crashed the driver on later context creation).
  ptr_gl_context_ = ptr_context;
  return ptr_context;
}

bool Sdl2Window::MakeGlCurrent(void* ptr_context) {
  if (SDL_GL_MakeCurrent(static_cast<SDL_Window*>(ptr_window_), ptr_context) != 0) {
    std::println(stderr, "SDL_GL_MakeCurrent 失败 | SDL_GL_MakeCurrent failed: {}", SDL_GetError());
    return false;
  }
  return true;
}

void Sdl2Window::SwapBuffers() {
  SDL_GL_SwapWindow(static_cast<SDL_Window*>(ptr_window_));
}

bool Sdl2Window::PumpEvents() {
  bool b_quit = false;
  SDL_Event event_sdl;
  while (SDL_PollEvent(&event_sdl) != 0) {
    switch (event_sdl.type) {
      case SDL_QUIT:
        b_quit = true;
        break;
      case SDL_WINDOWEVENT:
        HandleWindowEvent(event_sdl.window.event);
        break;
      default:
        break;  // 输入映射由 Sdl2Input::PumpInput 承担 | input mapping is Sdl2Input::PumpInput's job
    }
  }
  return b_quit;
}

void Sdl2Window::HandleWindowEvent(std::uint8_t uint1_event_id) {
  // Sdl2PlatformWindow.cs L81-112 的状态面(几何/焦点/挂起)
  // The state plane of Sdl2PlatformWindow.cs L81-112 (geometry/focus/suspend).
  switch (uint1_event_id) {
    case SDL_WINDOWEVENT_FOCUS_LOST:
      b_input_focus_.store(false, std::memory_order_relaxed);
      break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      b_input_focus_.store(true, std::memory_order_relaxed);
      break;
    // 显示器间移动引发的 DPI 变化(Sdl2Input.cs L93-95)
    // DPI changes from moving between displays (Sdl2Input.cs L93-95).
    case SDL_WINDOWEVENT_SIZE_CHANGED: {
      int int4_win_w = 0, int4_win_h = 0, int4_draw_w = 0, int4_draw_h = 0;
      SDL_GetWindowSize(static_cast<SDL_Window*>(ptr_window_), &int4_win_w, &int4_win_h);
      SDL_GL_GetDrawableSize(static_cast<SDL_Window*>(ptr_window_), &int4_draw_w, &int4_draw_h);
      const float float_scale = int4_win_w > 0 ? static_cast<float>(int4_draw_w) / int4_win_w : 1.0f;
      uint8_geom_.store(PackGeom(int4_win_w, int4_win_h, float_scale), std::memory_order_relaxed);
      break;
    }
    case SDL_WINDOWEVENT_HIDDEN:
    case SDL_WINDOWEVENT_MINIMIZED:
      b_suspended_.store(true, std::memory_order_relaxed);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
    case SDL_WINDOWEVENT_SHOWN:
    case SDL_WINDOWEVENT_MAXIMIZED:
    case SDL_WINDOWEVENT_RESTORED:
      b_suspended_.store(false, std::memory_order_relaxed);
      break;
    default:
      break;
  }
}

WindowGeomSnapshot Sdl2Window::Geom() const {
  const std::uint64_t uint8_packed = uint8_geom_.load(std::memory_order_relaxed);
  return WindowGeomSnapshot{
      .int4_width = static_cast<std::int32_t>(uint8_packed & 0xFFFF),
      .int4_height = static_cast<std::int32_t>((uint8_packed >> 16) & 0xFFFF),
      .float_scale = static_cast<float>((uint8_packed >> 32) & 0xFFFF) / 256.0f,
  };
}

}  // namespace ora::platform
