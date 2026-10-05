// UPSTREAM: OpenRA.Platforms.Default/Sdl2HardwareCursor.cs @b6fc03f L20-73(全文)
// + Sdl2PlatformWindow.cs @b6fc03f L364-377(DoublePixelData)、L379-405
// (CreateHardwareCursor)、L412-421(SetHardwareCursor)
// 形态映射(D87,docs/COVERAGE.md):
//  - IHardwareCursor 接口面由具体类型承担(单平台;上游接口为跨实现门面);
//  - Marshal.PtrToStructure<SDL_Surface> + Marshal.Copy → 真结构体字段直写
//    (pixels 指针 memcpy,语义 = 上游的"写表面像素区");
//  - 倍率判定的 NativeWindowScale 取 OPT-B5 几何快照的 scale
//    (drawable/window HiDPI 比;> 1.5f 的上游阈值照抄);
//  - 上游 try/catch(Exception)→ log + null 的容错面 = std::optional 返回;
//    Cursor == 0 时上游泄漏 surface(靠 GC 终结器)—— C++ 侧析构释放(偏离,
//    行为更紧;失败路径上游本就不可用该光标)。
// The whole of Sdl2HardwareCursor.cs L20-73 plus the cursor sections of
// Sdl2PlatformWindow.cs (L364-377 DoublePixelData, L379-405
// CreateHardwareCursor, L412-421 SetHardwareCursor). Shape mapping (D87,
// docs/COVERAGE.md): the IHardwareCursor interface face becomes the concrete
// type (single platform; the upstream interface is a cross-implementation
// facade); Marshal.PtrToStructure<SDL_Surface> + Marshal.Copy become a direct
// real-struct field write (memcpy into the pixels region = upstream's
// behavior); the NativeWindowScale of the doubling rule reads the OPT-B5
// geometry snapshot's scale (the drawable/window HiDPI ratio; the > 1.5f
// upstream threshold kept); upstream's try/catch(Exception)→ log + null
// tolerance becomes a std::optional return, and where upstream leaks the
// surface on a zero cursor handle (GC finalizer eventually frees it) the C++
// side destroys it (a deviation, strictly tighter — the cursor is unusable on
// that path upstream anyway).
#pragma once
import std;

#include "core/int2.hpp"

namespace ora::platform {

/// 光标位图像素倍增(Sdl2PlatformWindow.cs L364-377 的 static DoublePixelData;
/// 纯函数提出,可离线单测):2×2 邻域复制,w×h → (2w)×(2h)。
/// Cursor-bitmap pixel doubling (the static DoublePixelData of
/// Sdl2PlatformWindow.cs L364-377, extracted as a pure offline-testable
/// function): a 2×2 neighborhood copy, w×h → (2w)×(2h).
[[nodiscard]] std::vector<std::uint8_t> DoublePixelData(std::span<const std::uint8_t> span_data,
                                                        int int4_width, int int4_height);

/// SDL2 硬件光标(Sdl2HardwareCursor.cs):32bpp 表面 + CreateColorCursor 的
/// 3 次重试(Windows 上偶发失败、重试常成 —— 上游注释照抄)。
/// An SDL2 hardware cursor (Sdl2HardwareCursor.cs): a 32bpp surface plus the
/// 3-retry CreateColorCursor (it very occasionally fails on Windows and often
/// works when retried — verbatim upstream comment).
class Sdl2HardwareCursor {
 public:
  /// 失败抛 "Failed to create surface: <SDL_GetError()>"(消息逐字);句柄
  /// 为 0 表示 CreateColorCursor 三连败(调用方判 Cursor() == 0)。
  /// Throws "Failed to create surface: <SDL_GetError()>" on surface failure
  /// (message verbatim); a zero handle means the CreateColorCursor triple
  /// retry failed (callers test Cursor() == 0).
  Sdl2HardwareCursor(int int4_width, int int4_height, std::span<const std::uint8_t> span_data,
                     int2 int2_hotspot);
  ~Sdl2HardwareCursor();

  Sdl2HardwareCursor(const Sdl2HardwareCursor&) = delete;
  Sdl2HardwareCursor& operator=(const Sdl2HardwareCursor&) = delete;
  Sdl2HardwareCursor(Sdl2HardwareCursor&& other) noexcept;
  Sdl2HardwareCursor& operator=(Sdl2HardwareCursor&&) = delete;

  /// SDL 光标句柄(0 = 未建成)。
  /// The SDL cursor handle (0 = not created).
  [[nodiscard]] void* Cursor() const { return ptr_cursor_; }

 private:
  void* ptr_cursor_ = nullptr;
  void* ptr_surface_ = nullptr;
};

}  // namespace ora::platform
