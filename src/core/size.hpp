// UPSTREAM: OpenRA.Game/Primitives/Size.cs @b6fc03f L17-58(值类型逐语义;
//ToInt2/LatticeToInt2/ToRectangle 等图形面消费点暂无 —— 随消费批扩展)
// UPSTREAM: OpenRA.Game/Primitives/Size.cs @b6fc03f L17-58 (the value-type
// semantics; the graphics-side consumers ToInt2/LatticeToInt2/ToRectangle
// have no call sites yet — extended with their consuming batches).
#pragma once
import std;

namespace ora {

/// Size(Size.cs L17):宽高值对 | Size (Size.cs L17): a width/height pair.
struct Size {
  std::int32_t Width{0};
  std::int32_t Height{0};

  constexpr Size() = default;
  constexpr Size(std::int32_t int4_width, std::int32_t int4_height)
      : Width{int4_width}, Height{int4_height} {}

  constexpr bool IsEmpty() const { return Width == 0 && Height == 0; }  // L48

  friend constexpr bool operator==(Size sz_a, Size sz_b) {  // L27
    return sz_a.Width == sz_b.Width && sz_a.Height == sz_b.Height;
  }
  friend constexpr bool operator!=(Size sz_a, Size sz_b) {  // L32
    return !(sz_a == sz_b);
  }
  friend constexpr Size operator+(Size sz_a, Size sz_b) {  // L22
    return Size{sz_a.Width + sz_b.Width, sz_a.Height + sz_b.Height};
  }
  friend constexpr Size operator-(Size sz_a, Size sz_b) {  // L37
    return Size{sz_a.Width - sz_b.Width, sz_a.Height - sz_b.Height};
  }
};

}  // namespace ora
