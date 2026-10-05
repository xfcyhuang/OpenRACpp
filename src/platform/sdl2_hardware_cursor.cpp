// UPSTREAM: OpenRA.Platforms.Default/Sdl2HardwareCursor.cs @b6fc03f L20-73(实现)
// + Sdl2PlatformWindow.cs L364-377(DoublePixelData 实现)
// Implementation of the hardware cursor; the pixel masks
// (0x00FF0000/0x0000FF00/0x000000FF/0xFF000000) and the 3-retry loop are
// verbatim upstream.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include <SDL2/SDL.h>  // 第三方 C 头(std_import_check 白名单登记) | third-party C header (registered in the std_import_check whitelist)

#include "platform/sdl2_hardware_cursor.hpp"

namespace ora::platform {

std::vector<std::uint8_t> DoublePixelData(std::span<const std::uint8_t> span_data,
                                          int int4_width, int int4_height) {
  std::vector<std::uint8_t> vec_scaled(4 * span_data.size(), 0);
  for (int int4_y = 0; int4_y < int4_height; int4_y++) {
    for (int int4_x = 0; int4_x < int4_width; int4_x++) {
      const std::size_t int4_a = 4 * (static_cast<std::size_t>(int4_y) * int4_width + int4_x);
      const std::size_t int4_b = 8 * (2 * static_cast<std::size_t>(int4_y) * int4_width + int4_x);
      const std::size_t int4_c = int4_b + 8 * static_cast<std::size_t>(int4_width);
      for (int int4_i = 0; int4_i < 4; int4_i++)
        vec_scaled[int4_b + int4_i] = vec_scaled[int4_b + 4 + int4_i] =
            vec_scaled[int4_c + int4_i] = vec_scaled[int4_c + 4 + int4_i] =
                span_data[int4_a + int4_i];
    }
  }

  return vec_scaled;
}

Sdl2HardwareCursor::Sdl2HardwareCursor(int int4_width, int int4_height,
                                       std::span<const std::uint8_t> span_data,
                                       int2 int2_hotspot) {
  try {
    ptr_surface_ = SDL_CreateRGBSurface(0, int4_width, int4_height, 32, 0x00FF0000, 0x0000FF00,
                                        0x000000FF, 0xFF000000);
    if (ptr_surface_ == nullptr)
      throw std::runtime_error{std::format("Failed to create surface: {}", SDL_GetError())};

    // 上游 Marshal.PtrToStructure<SDL_Surface> + Marshal.Copy(data, 0,
    // sur.pixels, len) 的真结构体直写形态。
    // The real-struct form of upstream's Marshal.PtrToStructure<SDL_Surface>
    // + Marshal.Copy(data, 0, sur.pixels, len).
    auto* const ptr_surface = static_cast<SDL_Surface*>(ptr_surface_);
    std::memcpy(ptr_surface->pixels, span_data.data(), span_data.size());

    // Windows 上偶发失败、重试常成(上游注释照抄)。
    // Very occasionally fails on Windows, but often works when retried
    // (verbatim upstream comment).
    for (int int4_retries = 0; int4_retries < 3 && ptr_cursor_ == nullptr; int4_retries++)
      ptr_cursor_ = SDL_CreateColorCursor(ptr_surface, int2_hotspot.X, int2_hotspot.Y);
  } catch (...) {
    // 上游 catch → Dispose → rethrow 的等价物(释放已建资源再抛)。
    // The equivalent of upstream's catch → Dispose → rethrow (release the
    // partially-built resources, then rethrow).
    if (ptr_cursor_ != nullptr) {
      SDL_FreeCursor(static_cast<SDL_Cursor*>(ptr_cursor_));
      ptr_cursor_ = nullptr;
    }
    if (ptr_surface_ != nullptr) {
      SDL_FreeSurface(static_cast<SDL_Surface*>(ptr_surface_));
      ptr_surface_ = nullptr;
    }
    throw;
  }
}

Sdl2HardwareCursor::Sdl2HardwareCursor(Sdl2HardwareCursor&& other) noexcept
    : ptr_cursor_{other.ptr_cursor_}, ptr_surface_{other.ptr_surface_} {
  other.ptr_cursor_ = nullptr;
  other.ptr_surface_ = nullptr;
}

Sdl2HardwareCursor::~Sdl2HardwareCursor() {
  if (ptr_cursor_ != nullptr) {
    SDL_FreeCursor(static_cast<SDL_Cursor*>(ptr_cursor_));
    ptr_cursor_ = nullptr;
  }

  if (ptr_surface_ != nullptr) {
    SDL_FreeSurface(static_cast<SDL_Surface*>(ptr_surface_));
    ptr_surface_ = nullptr;
  }
}

}  // namespace ora::platform
