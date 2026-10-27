#pragma once
import std;

#include "cvec.hpp"

namespace ora {

enum class MapGridType : std::uint8_t { Rectangular, RectangularIsometric };

enum class SubCell : std::uint8_t {
  Invalid = 0xFF,
  Any = 0xFE,
  FullCell = 0,
  First = 1,
};

struct CPos;

struct MPos {
  std::int32_t U{0};
  std::int32_t V{0};

  constexpr MPos() = default;
  constexpr MPos(std::int32_t int4_u, std::int32_t int4_v) : U{int4_u}, V{int4_v} {}

  static constexpr MPos Zero() { return MPos{0, 0}; }

  constexpr MPos Clamp(Rectangle const& rect_r) const {
    return MPos{std::min(rect_r.Right(), std::max(U, rect_r.Left())),
                std::min(rect_r.Bottom(), std::max(V, rect_r.Top()))};
  }

  constexpr std::int32_t Hash() const { return U ^ V; }

  constexpr CPos ToCPos(MapGridType grid_type) const;

  friend constexpr bool operator==(MPos uv_a, MPos uv_b) { return uv_a.U == uv_b.U && uv_a.V == uv_b.V; }
  friend constexpr bool operator!=(MPos uv_a, MPos uv_b) { return !(uv_a == uv_b); }
};

struct PPos {
  std::int32_t U{0};
  std::int32_t V{0};

  constexpr PPos() = default;
  constexpr PPos(std::int32_t int4_u, std::int32_t int4_v) : U{int4_u}, V{int4_v} {}

  static constexpr PPos Zero() { return PPos{0, 0}; }

  constexpr PPos Clamp(Rectangle const& rect_r) const {
    return PPos{std::min(rect_r.Right(), std::max(U, rect_r.Left())),
                std::min(rect_r.Bottom(), std::max(V, rect_r.Top()))};
  }

  constexpr std::int32_t Hash() const { return U ^ V; }

  friend constexpr bool operator==(PPos uv_a, PPos uv_b) { return uv_a.U == uv_b.U && uv_a.V == uv_b.V; }
  friend constexpr bool operator!=(PPos uv_a, PPos uv_b) { return !(uv_a == uv_b); }
};

constexpr MPos ToMPos(PPos puv_v) { return MPos{puv_v.U, puv_v.V}; }
constexpr PPos ToPPos(MPos uv_v) { return PPos{uv_v.U, uv_v.V}; }

struct CPos {
  std::int32_t Bits{0};

  constexpr CPos() = default;
  constexpr explicit CPos(std::int32_t int4_bits) : Bits{int4_bits} {}
  constexpr CPos(std::int32_t int4_x, std::int32_t int4_y) : CPos{int4_x, int4_y, 0} {}
  constexpr CPos(std::int32_t int4_x, std::int32_t int4_y, std::uint8_t uint1_layer)
      : Bits{(int4_x & 0xFFF) << 20 | (int4_y & 0xFFF) << 8 | int4_layer_value(uint1_layer)} {}

  static constexpr CPos Zero() { return CPos{0, 0, 0}; }

  constexpr std::int32_t X() const { return Bits >> 20; }

  constexpr std::int32_t Y() const {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(Bits >> 4)) >> 4;
  }

  constexpr std::uint8_t Layer() const { return static_cast<std::uint8_t>(Bits); }

  constexpr std::int32_t Hash() const { return Bits; }

  constexpr MPos ToMPos(MapGridType grid_type) const {
    if (grid_type == MapGridType::Rectangular)
      return MPos{X(), Y()};

    const std::int32_t int4_v{X() + Y()};
    const std::int32_t int4_u{(int4_v - (int4_v & 1)) / 2 - Y()};
    return MPos{int4_u, int4_v};
  }

  friend constexpr CPos operator+(CVec v_a, CPos cell_b) {
    return CPos{v_a.X + cell_b.X(), v_a.Y + cell_b.Y(), cell_b.Layer()};
  }
  friend constexpr CPos operator+(CPos cell_a, CVec v_b) {
    return CPos{cell_a.X() + v_b.X, cell_a.Y() + v_b.Y, cell_a.Layer()};
  }
  friend constexpr CPos operator-(CPos cell_a, CVec v_b) {
    return CPos{cell_a.X() - v_b.X, cell_a.Y() - v_b.Y, cell_a.Layer()};
  }
  friend constexpr CVec operator-(CPos cell_a, CPos cell_b) {
    return CVec{cell_a.X() - cell_b.X(), cell_a.Y() - cell_b.Y()};
  }
  friend constexpr bool operator==(CPos cell_a, CPos cell_b) { return cell_a.Bits == cell_b.Bits; }
  friend constexpr bool operator!=(CPos cell_a, CPos cell_b) { return !(cell_a == cell_b); }

  friend constexpr bool operator<(CPos cell_a, CPos cell_b) { return cell_a.Bits < cell_b.Bits; }

 private:

  static constexpr std::int32_t int4_layer_value(std::uint8_t uint1_layer) {
    return static_cast<std::int32_t>(uint1_layer);
  }
};

constexpr CPos MPos::ToCPos(MapGridType grid_type) const {
  if (grid_type == MapGridType::Rectangular)
    return CPos{U, V};

  const std::int32_t int4_y{(V - (V & 1)) / 2 - U};
  const std::int32_t int4_x{V - int4_y};
  return CPos{int4_x, int4_y};
}

}

template <>
struct std::hash<ora::CPos> {
  size_t operator()(const ora::CPos& cell) const noexcept {
    return std::hash<std::int32_t>{}(cell.Bits);
  }
};
