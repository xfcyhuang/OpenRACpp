// UPSTREAM: OpenRA.Platforms.Default/Sdl2PlatformWindow.cs @b6fc03f L34-587(窗口创建/模式/GL 属性段)
// OPT-B5(docs/OPTIMIZATION_TRACKER.md):窗口几何为打包 atomic<u64> 快照,getter 无锁 ——
// 替代上游每个属性 getter 各取一把 lock(Sdl2PlatformWindow.cs L42-120,scissor 等热路径
// 每帧多次读)。OPT-A5:GL 上下文由渲染线程创建并持有(统一线程模型,上游
// Sdl2PlatformWindow.cs L345-353 的 Windows 窗口模式单线程特例不复刻)。
// Window creation/modes/GL attributes per upstream; OPT-B5
// (docs/OPTIMIZATION_TRACKER.md): window geometry is a packed atomic<u64>
// snapshot with lock-free getters — replacing the per-getter locks of
// upstream (Sdl2PlatformWindow.cs L42-120, read several times per frame on
// hot paths like scissor). OPT-A5: the GL context is created and owned by the
// render thread (a unified threading model; the upstream Windows-windowed
// single-thread special case of Sdl2PlatformWindow.cs L345-353 is not
// replicated).
#pragma once
import std;

#include "core/int2.hpp"

namespace ora::platform {

class Sdl2HardwareCursor;  // 前置:CreateHardwareCursor 返回 optional<光标>| forward: CreateHardwareCursor returns optional<cursor>

/// 窗口模式(Sdl2PlatformWindow.cs 的 WindowMode 枚举)
/// Window modes (the WindowMode enum of Sdl2PlatformWindow.cs)
enum class WindowMode : std::uint8_t {
  Windowed,         // 窗口化 | windowed
  PseudoFullscreen, // 无边框桌面全屏(FULLSCREEN_DESKTOP)| borderless desktop fullscreen
  Fullscreen,       // 真全屏(独占分辨率切换)| real fullscreen
};

/// GL 档位(上游 GLProfile;Legacy 暂不实现)
/// GL profiles (upstream GLProfile; Legacy is not implemented yet)
enum class GLProfileKind : std::uint8_t {
  Modern,    // GL 3.2 Core(Sdl2PlatformWindow.cs L550-553)
  Embedded,  // GLES 3.0(Sdl2PlatformWindow.cs L556-559)
};

/// 窗口几何快照:一次 relaxed load 取全部热路径属性(OPT-B5)。
/// Window-geometry snapshot: one relaxed load for all hot-path properties (OPT-B5).
struct WindowGeomSnapshot {
  std::int32_t int4_width;    // 窗口逻辑尺寸 | logical window size
  std::int32_t int4_height;
  float float_scale;          // 绘制尺寸/窗口尺寸的 HiDPI 比 | HiDPI drawable/window ratio
};

/// SDL2 窗口 + GL 上下文宿主(主线程创建窗口与泵事件;GL 上下文归渲染线程)。
/// SDL2 window + GL-context host (window creation and event pumping on the
/// main thread; the GL context belongs to the render thread).
class Sdl2Window {
 public:
  struct Desc {
    std::int32_t int4_width = 1024;
    std::int32_t int4_height = 768;
    WindowMode mode_window = WindowMode::Windowed;
    GLProfileKind kind_profile = GLProfileKind::Modern;
    bool b_hidden = true;  // 测试默认隐藏窗口 | hidden window by default for tests
  };

  /// 创建窗口(失败返回 std::nullopt,错误经 stderr)
  /// Creates the window (std::nullopt on failure, error via stderr).
  static std::optional<Sdl2Window> Create(const Desc& desc);

  Sdl2Window(Sdl2Window&& other) noexcept;
  Sdl2Window(const Sdl2Window&) = delete;
  Sdl2Window& operator=(const Sdl2Window&) = delete;
  Sdl2Window& operator=(Sdl2Window&&) = delete;
  ~Sdl2Window();

  /// 在**调用线程**创建 GL 上下文(渲染线程调用 = OPT-A5 统一模型)。
  /// Creates the GL context on the *calling* thread (called by the render
  /// thread under the OPT-A5 unified model).
  void* CreateGlContext();

  /// 绑定/解绑 GL 上下文到调用线程(ctx 为空 = 解绑)。
  /// Binds/unbinds the GL context to the calling thread (nullptr unbinds).
  bool MakeGlCurrent(void* ptr_context);

  /// 交换后台缓冲(须在 GL 上下文线程)。
  /// Swaps the back buffer (must run on the GL-context thread).
  void SwapBuffers();

  /// 泵事件(主线程):窗口几何变化即时更新原子快照;返回是否收到退出请求。
  /// Pumps events (main thread): window-geometry changes update the atomic
  /// snapshot immediately; returns whether a quit request was received.
  bool PumpEvents();

  /// 窗口事件处理(Sdl2PlatformWindow.cs L81-112 的状态面:焦点/挂起/几何;
  /// PumpEvents 与 Sdl2Input::PumpInput 共用)。event id = SDL_WindowEventID。
  /// Window-event handling (the state plane of Sdl2PlatformWindow.cs L81-112:
  /// focus/suspend/geometry; shared by PumpEvents and Sdl2Input::PumpInput).
  /// The event id is an SDL_WindowEventID.
  void HandleWindowEvent(std::uint8_t uint1_event_id);

  /// 输入焦点(Sdl2PlatformWindow.cs L85-91;默认 false,FOCUS_GAINED 置位)。
  /// Input focus (Sdl2PlatformWindow.cs L85-91; false until FOCUS_GAINED).
  bool HasInputFocus() const { return b_input_focus_.load(std::memory_order_relaxed); }

  /// 是否挂起(隐藏/最小化;Sdl2PlatformWindow.cs L98-108)。
  /// Whether suspended (hidden/minimized; Sdl2PlatformWindow.cs L98-108).
  bool IsSuspended() const { return b_suspended_.load(std::memory_order_relaxed); }

  /// 几何快照(OPT-B5:无锁一次读)。
  /// Geometry snapshot (OPT-B5: one lock-free read).
  WindowGeomSnapshot Geom() const;

  /// 创建硬件光标(Sdl2PlatformWindow.cs L379-405):非 macOS 且窗口
  /// scale > 1.5f 时先像素倍增(本平台恒满足"非 macOS"),pixelDouble 再
  /// 倍增一次;建败返回 nullopt(D87:上游 try/catch → log + null)。
  /// Creates a hardware cursor (Sdl2PlatformWindow.cs L379-405): pixel-
  /// doubles first when non-macOS and the window scale > 1.5f (always
  /// "non-macOS" on this platform), then again when pixelDouble; a build
  /// failure returns nullopt (D87: upstream's try/catch → log + null).
  [[nodiscard]] std::optional<Sdl2HardwareCursor> CreateHardwareCursor(
      std::string_view str_name, int int4_width, int int4_height,
      std::span<const std::uint8_t> span_data, int2 int2_hotspot, bool b_pixel_double);

  /// 设置当前光标(Sdl2PlatformWindow.cs L412-421):空指针 = 隐藏系统光标
  /// (软光标路径)。
  /// Sets the current cursor (Sdl2PlatformWindow.cs L412-421): a null pointer
  /// hides the system cursor (the software-cursor path).
  void SetHardwareCursor(const Sdl2HardwareCursor* ptr_cursor);

  /// SDL 窗口句柄(平台内部互通用;调用方不得直接调 SDL 窗口 API 之外的接口)
  /// SDL window handle (platform-internal interoperability).
  void* NativeHandle() const { return ptr_window_; }

 private:
  Sdl2Window() = default;
  void* ptr_window_ = nullptr;                      // SDL_Window*
  void* ptr_gl_context_ = nullptr;                  // SDL_GLContext(渲染线程所有)
  std::atomic<std::uint64_t> uint8_geom_{};         // {w:16|h:16|scale(8.8 定点):16|保留:16}
  std::atomic<bool> b_input_focus_{false};          // 焦点(OPT-B5 同款原子量)| focus (OPT-B5-style atomic)
  std::atomic<bool> b_suspended_{false};            // 挂起 | suspended
};

/// SDL 视频子系统初始化(引用计数;进程级一次物理初始化)。
/// SDL video-subsystem init (reference counted; one physical init per process).
bool InitSdl2Video();
void ShutdownSdl2Video();

}  // namespace ora::platform
