import std;
#include <cstdio>

#include <SDL2/SDL.h>

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

    auto* const ptr_surface = static_cast<SDL_Surface*>(ptr_surface_);
    std::memcpy(ptr_surface->pixels, span_data.data(), span_data.size());

    for (int int4_retries = 0; int4_retries < 3 && ptr_cursor_ == nullptr; int4_retries++)
      ptr_cursor_ = SDL_CreateColorCursor(ptr_surface, int2_hotspot.X, int2_hotspot.Y);
  } catch (...) {

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

}
