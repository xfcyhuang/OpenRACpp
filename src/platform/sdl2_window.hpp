#pragma once
import std;

#include "core/int2.hpp"

namespace ora::platform {

class Sdl2HardwareCursor;

enum class WindowMode : std::uint8_t {
  Windowed,
  PseudoFullscreen,
  Fullscreen,
};

enum class GLProfileKind : std::uint8_t {
  Modern,
  Embedded,
};

struct WindowGeomSnapshot {
  std::int32_t int4_width;
  std::int32_t int4_height;
  float float_scale;
};

class Sdl2Window {
 public:
  struct Desc {
    std::int32_t int4_width = 1024;
    std::int32_t int4_height = 768;
    WindowMode mode_window = WindowMode::Windowed;
    GLProfileKind kind_profile = GLProfileKind::Modern;
    bool b_hidden = true;

    int int4_video_display = 0;
  };

  static std::optional<Sdl2Window> Create(const Desc& desc);

  Sdl2Window(Sdl2Window&& other) noexcept;
  Sdl2Window(const Sdl2Window&) = delete;
  Sdl2Window& operator=(const Sdl2Window&) = delete;
  Sdl2Window& operator=(Sdl2Window&&) = delete;
  ~Sdl2Window();

  void* CreateGlContext();

  bool MakeGlCurrent(void* ptr_context);

  void SwapBuffers();

  bool PumpEvents();

  void HandleWindowEvent(std::uint8_t uint1_event_id);

  bool HasInputFocus() const { return b_input_focus_.load(std::memory_order_relaxed); }

  bool IsSuspended() const { return b_suspended_.load(std::memory_order_relaxed); }

  WindowGeomSnapshot Geom() const;

  [[nodiscard]] std::optional<Sdl2HardwareCursor> CreateHardwareCursor(
      std::string_view str_name, int int4_width, int int4_height,
      std::span<const std::uint8_t> span_data, int2 int2_hotspot, bool b_pixel_double);

  void SetHardwareCursor(const Sdl2HardwareCursor* ptr_cursor);

  void SetRelativeMouseMode(bool b_enabled);

  std::string GetClipboardText() const;

  bool SetClipboardText(const std::string& str_text) const;

  float NativeWindowScale() const { return Geom().float_scale; }

  int DisplayCount() const;
  int CurrentDisplay() const;
  int2 NativeWindowSize() const;
  int2 EffectiveWindowSize() const;
  float EffectiveWindowScale() const;
  GLProfileKind GLProfileUsed() const {
    return kind_profile_used_.load(std::memory_order_relaxed);
  }
  void SetScaleModifier(float fp4_scale);
  void GrabWindowMouseFocus();
  void ReleaseWindowMouseFocus();
  bool TryOpenUrl(std::string_view str_url) const;
  void SetVSyncEnabled(bool b_enabled);
  void SetWindowTitle(const std::string& str_title) const;

  std::function<void(float, float, float, float)> fn_on_window_scale_changed;

  void* NativeHandle() const { return ptr_window_; }

 private:
  Sdl2Window() = default;
  void* ptr_window_ = nullptr;
  void* ptr_gl_context_ = nullptr;
  std::atomic<std::uint64_t> uint8_geom_{};
  std::atomic<bool> b_input_focus_{false};
  std::atomic<bool> b_suspended_{false};
  std::atomic<float> fp4_scale_modifier_{1.0f};
  std::atomic<int> int4_vsync_intent_{-1};
  std::atomic<int> int4_vsync_applied_{-2};
  std::atomic<GLProfileKind> kind_profile_used_{GLProfileKind::Modern};
};

bool InitSdl2Video();
void ShutdownSdl2Video();

}
