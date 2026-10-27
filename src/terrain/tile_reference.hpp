#pragma once
import std;

#include "meta/parse.hpp"

namespace ora::map {

struct TerrainTile {
  std::uint16_t Type{0};
  std::uint8_t Index{0};

  constexpr TerrainTile() = default;
  constexpr TerrainTile(std::uint16_t uint2_type, std::uint8_t uint1_index)
      : Type{uint2_type}, Index{uint1_index} {}

  constexpr std::int32_t Hash() const {
    return static_cast<std::int32_t>(Type) ^
           (static_cast<std::int32_t>(Index) * 0x35C4AD3);
  }

  std::string ToString() const {
    return std::format("{},{}", Type, Index);
  }

  static bool TryParse(std::string_view s, TerrainTile& tt_out) {
    const std::size_t pos_comma = s.find(',');
    if (pos_comma == std::string_view::npos)
      return false;
    if (s.find(',', pos_comma + 1) != std::string_view::npos)
      return false;

    std::uint16_t uint2_type = 0;
    std::uint8_t uint1_index = 0;
    if (!meta::TryParseUInt16Invariant(s.substr(0, pos_comma), uint2_type) ||
        !meta::TryParseByteInvariant(s.substr(pos_comma + 1), uint1_index))
      return false;

    tt_out = TerrainTile{uint2_type, uint1_index};
    return true;
  }

  friend constexpr bool operator==(TerrainTile t_a, TerrainTile t_b) {
    return t_a.Type == t_b.Type && t_a.Index == t_b.Index;
  }
  friend constexpr bool operator!=(TerrainTile t_a, TerrainTile t_b) {
    return !(t_a == t_b);
  }
};

struct ResourceTile {
  std::uint8_t Type{0};
  std::uint8_t Index{0};

  constexpr ResourceTile() = default;
  constexpr ResourceTile(std::uint8_t uint1_type, std::uint8_t uint1_index)
      : Type{uint1_type}, Index{uint1_index} {}

  constexpr std::int32_t Hash() const {
    return static_cast<std::int32_t>(Type) ^ static_cast<std::int32_t>(Index);
  }

  friend constexpr bool operator==(ResourceTile t_a, ResourceTile t_b) {
    return t_a.Type == t_b.Type && t_a.Index == t_b.Index;
  }
  friend constexpr bool operator!=(ResourceTile t_a, ResourceTile t_b) {
    return !(t_a == t_b);
  }
};

}
