#pragma once
import std;

namespace ora {

struct Size {
  std::int32_t Width{0};
  std::int32_t Height{0};

  constexpr Size() = default;
  constexpr Size(std::int32_t int4_width, std::int32_t int4_height)
      : Width{int4_width}, Height{int4_height} {}

  constexpr bool IsEmpty() const { return Width == 0 && Height == 0; }

  friend constexpr bool operator==(Size sz_a, Size sz_b) {
    return sz_a.Width == sz_b.Width && sz_a.Height == sz_b.Height;
  }
  friend constexpr bool operator!=(Size sz_a, Size sz_b) {
    return !(sz_a == sz_b);
  }
  friend constexpr Size operator+(Size sz_a, Size sz_b) {
    return Size{sz_a.Width + sz_b.Width, sz_a.Height + sz_b.Height};
  }
  friend constexpr Size operator-(Size sz_a, Size sz_b) {
    return Size{sz_a.Width - sz_b.Width, sz_a.Height - sz_b.Height};
  }
};

}
