import std;
#include <cstdio>

#include <SDL2/SDL.h>

#include "platform/sdl2_hardware_cursor.hpp"
#include "platform/sdl2_window.hpp"

namespace ora::platform {

namespace {

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

}

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

  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

std::optional<Sdl2Window> Sdl2Window::Create(const Desc& desc) {
  if (!InitSdl2Video())
    return std::nullopt;

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

  int int4_video_display = desc.int4_video_display;
  if (int4_video_display < 0 ||
      int4_video_display >= SDL_GetNumVideoDisplays())
    int4_video_display = 0;

  int int4_width = desc.int4_width;
  int int4_height = desc.int4_height;
  if (int4_width == 0 && int4_height == 0) {
    SDL_DisplayMode mode_display{};
    if (SDL_GetCurrentDisplayMode(int4_video_display, &mode_display) == 0) {
      int4_width = mode_display.w;
      int4_height = mode_display.h;
    }
  }

  SDL_Window* ptr_window = SDL_CreateWindow(
      "OpenRACpp", SDL_WINDOWPOS_CENTERED_DISPLAY(int4_video_display),
      SDL_WINDOWPOS_CENTERED_DISPLAY(int4_video_display),
      int4_width, int4_height, ToSdlFlags(desc.mode_window, desc.b_hidden));
  if (ptr_window == nullptr) {
    std::println(stderr, "SDL_CreateWindow 失败 | SDL_CreateWindow failed: {}", SDL_GetError());
    return std::nullopt;
  }

  Sdl2Window window_result;
  window_result.ptr_window_ = ptr_window;

  int int4_win_w = 0, int4_win_h = 0, int4_draw_w = 0, int4_draw_h = 0;
  SDL_GetWindowSize(ptr_window, &int4_win_w, &int4_win_h);
  SDL_GL_GetDrawableSize(ptr_window, &int4_draw_w, &int4_draw_h);
  const float float_scale = int4_win_w > 0 ? static_cast<float>(int4_draw_w) / int4_win_w : 1.0f;
  window_result.uint8_geom_.store(PackGeom(int4_win_w, int4_win_h, float_scale), std::memory_order_relaxed);
  window_result.kind_profile_used_.store(desc.kind_profile, std::memory_order_relaxed);
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
  const int int4_intent = int4_vsync_intent_.load(std::memory_order_relaxed);
  if (int4_intent != int4_vsync_applied_.load(std::memory_order_relaxed)) {
    SDL_GL_SetSwapInterval(int4_intent);
    int4_vsync_applied_.store(int4_intent, std::memory_order_relaxed);
  }
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
        break;
    }
  }
  return b_quit;
}

void Sdl2Window::HandleWindowEvent(std::uint8_t uint1_event_id) {

  switch (uint1_event_id) {
    case SDL_WINDOWEVENT_FOCUS_LOST:
      b_input_focus_.store(false, std::memory_order_relaxed);
      break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      b_input_focus_.store(true, std::memory_order_relaxed);
      break;

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

int Sdl2Window::DisplayCount() const {
  const int int4_displays = SDL_GetNumVideoDisplays();
  return int4_displays > 0 ? int4_displays : 0;
}

int Sdl2Window::CurrentDisplay() const {
  if (ptr_window_ == nullptr)
    return 0;
  const int int4_index = SDL_GetWindowDisplayIndex(static_cast<SDL_Window*>(ptr_window_));
  return int4_index >= 0 ? int4_index : 0;
}

int2 Sdl2Window::NativeWindowSize() const {
  const WindowGeomSnapshot geom_window = Geom();
  return int2{geom_window.int4_width, geom_window.int4_height};
}

int2 Sdl2Window::EffectiveWindowSize() const {
  const WindowGeomSnapshot geom_window = Geom();
  const float fp4_modifier =
      fp4_scale_modifier_.load(std::memory_order_relaxed);
  return int2{
      static_cast<std::int32_t>(geom_window.int4_width / fp4_modifier),
      static_cast<std::int32_t>(geom_window.int4_height / fp4_modifier)};
}

float Sdl2Window::EffectiveWindowScale() const {
  return NativeWindowScale() * fp4_scale_modifier_.load(std::memory_order_relaxed);
}

void Sdl2Window::SetScaleModifier(float fp4_scale) {
  const float fp4_old = fp4_scale_modifier_.load(std::memory_order_relaxed);
  fp4_scale_modifier_.store(fp4_scale, std::memory_order_relaxed);
  if (fn_on_window_scale_changed) {
    const float fp4_native = NativeWindowScale();
    fn_on_window_scale_changed(fp4_native, fp4_native * fp4_old, fp4_native,
                               fp4_native * fp4_scale);
  }
}

void Sdl2Window::GrabWindowMouseFocus() {
  if (ptr_window_ != nullptr)
    SDL_SetWindowGrab(static_cast<SDL_Window*>(ptr_window_), SDL_TRUE);
}

void Sdl2Window::ReleaseWindowMouseFocus() {
  if (ptr_window_ != nullptr)
    SDL_SetWindowGrab(static_cast<SDL_Window*>(ptr_window_), SDL_FALSE);
}

bool Sdl2Window::TryOpenUrl(std::string_view str_url) const {
  const std::string str_null_terminated{str_url};
#if SDL_VERSION_ATLEAST(2, 0, 14)
  return SDL_OpenURL(str_null_terminated.c_str()) == 0;
#else
  (void)str_null_terminated;
  return false;
#endif
}

void Sdl2Window::SetVSyncEnabled(bool b_enabled) {
  int4_vsync_intent_.store(b_enabled ? 1 : 0, std::memory_order_relaxed);
}

void Sdl2Window::SetWindowTitle(const std::string& str_title) const {
  if (ptr_window_ != nullptr)
    SDL_SetWindowTitle(static_cast<SDL_Window*>(ptr_window_),
                       str_title.c_str());
}

std::optional<Sdl2HardwareCursor> Sdl2Window::CreateHardwareCursor(
    std::string_view str_name, int int4_width, int int4_height,
    std::span<const std::uint8_t> span_data, int2 int2_hotspot, bool b_pixel_double) {
  try {
    int int4_cur_width = int4_width;
    int int4_cur_height = int4_height;
    std::vector<std::uint8_t> vec_data{span_data.begin(), span_data.end()};

    if (Geom().float_scale > 1.5f) {
      vec_data = DoublePixelData(vec_data, int4_cur_width, int4_cur_height);
      int4_cur_width = 2 * int4_cur_width;
      int4_cur_height = 2 * int4_cur_height;
      int2_hotspot = int2{2 * int2_hotspot.X, 2 * int2_hotspot.Y};
    }

    if (b_pixel_double) {
      vec_data = DoublePixelData(vec_data, int4_cur_width, int4_cur_height);
      int4_cur_width = 2 * int4_cur_width;
      int4_cur_height = 2 * int4_cur_height;
      int2_hotspot = int2{2 * int2_hotspot.X, 2 * int2_hotspot.Y};
    }

    Sdl2HardwareCursor cursor{int4_cur_width, int4_cur_height, vec_data, int2_hotspot};
    if (cursor.Cursor() == nullptr)
      return std::nullopt;
    return cursor;
  } catch (const std::exception& ex) {
    std::println(stderr, "Failed to create hardware cursor `{}` - {}", str_name, ex.what());
    return std::nullopt;
  }
}

void Sdl2Window::SetHardwareCursor(const Sdl2HardwareCursor* ptr_cursor) {
  if (ptr_cursor != nullptr && ptr_cursor->Cursor() != nullptr) {
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetCursor(static_cast<SDL_Cursor*>(ptr_cursor->Cursor()));
  } else
    SDL_ShowCursor(SDL_FALSE);
}

void Sdl2Window::SetRelativeMouseMode(bool b_enabled) {

  SDL_SetRelativeMouseMode(b_enabled ? SDL_TRUE : SDL_FALSE);
}

std::string Sdl2Window::GetClipboardText() const {

  char* ptr_text = SDL_GetClipboardText();
  std::string str_ret{ptr_text != nullptr ? ptr_text : ""};
  SDL_free(ptr_text);
  return str_ret;
}

bool Sdl2Window::SetClipboardText(const std::string& str_text) const {

  return SDL_SetClipboardText(str_text.c_str()) == 0;
}

}
