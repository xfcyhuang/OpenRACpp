#pragma once
import std;

#include "core/int2.hpp"

namespace ora::platform {

[[nodiscard]] std::vector<std::uint8_t> DoublePixelData(std::span<const std::uint8_t> span_data,
                                                        int int4_width, int int4_height);

class Sdl2HardwareCursor {
 public:

  Sdl2HardwareCursor(int int4_width, int int4_height, std::span<const std::uint8_t> span_data,
                     int2 int2_hotspot);
  ~Sdl2HardwareCursor();

  Sdl2HardwareCursor(const Sdl2HardwareCursor&) = delete;
  Sdl2HardwareCursor& operator=(const Sdl2HardwareCursor&) = delete;
  Sdl2HardwareCursor(Sdl2HardwareCursor&& other) noexcept;
  Sdl2HardwareCursor& operator=(Sdl2HardwareCursor&&) = delete;

  [[nodiscard]] void* Cursor() const { return ptr_cursor_; }

 private:
  void* ptr_cursor_ = nullptr;
  void* ptr_surface_ = nullptr;
};

}
