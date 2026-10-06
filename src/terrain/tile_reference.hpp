// UPSTREAM: OpenRA.Game/Map/TileReference.cs @b6fc03f L18-51(逐语义重写)
//          Verbatim-semantics rewrite.
#pragma once
import std;

#include "meta/parse.hpp"  // Exts.TryParseUInt16/ByteInvariant 的 C++ 载体
                           // (the C++ carriers of Exts.TryParseUInt16/
                           // ByteInvariant)

namespace ora::map {

/// TerrainTile(TileReference.cs L18):ushort type + byte index
/// TerrainTile (TileReference.cs L18): a ushort type + a byte index.
struct TerrainTile {
  std::uint16_t Type{0};
  std::uint8_t Index{0};

  constexpr TerrainTile() = default;
  constexpr TerrainTile(std::uint16_t uint2_type, std::uint8_t uint1_index)
      : Type{uint2_type}, Index{uint1_index} {}

  /// GetHashCode(L21) | the upstream GetHashCode (L21).
  constexpr std::int32_t Hash() const {
    return static_cast<std::int32_t>(Type) ^
           (static_cast<std::int32_t>(Index) * 0x35C4AD3);  // 见下 | see below
  }
  // .NET ushort.GetHashCode = 值本身;byte.GetHashCode = 值本身;int 异或。
  // .NET's ushort.GetHashCode is the value itself, as is byte's; XORed.

  /// ToString(L23) | the upstream ToString (L23).
  std::string ToString() const {
    return std::format("{},{}", Type, Index);
  }

  /// TryParse(L25-41):逗号两段,u16 + u8(不变式解析;段数 != 2 或解析失败
  /// = false)| TryParse (L25-41): two comma parts, u16 + u8 (invariant
  /// parsing; a part count != 2 or a parse failure yields false).
  static bool TryParse(std::string_view s, TerrainTile& tt_out) {
    const std::size_t pos_comma = s.find(',');
    if (pos_comma == std::string_view::npos)
      return false;
    if (s.find(',', pos_comma + 1) != std::string_view::npos)
      return false;  // Span<Range>[3] 但仅接受 parts == 2 | Span<Range>[3]
                     // accepts only parts == 2

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

/// ResourceTile(TileReference.cs L44):byte type + byte index
/// ResourceTile (TileReference.cs L44): a byte type + a byte index.
struct ResourceTile {
  std::uint8_t Type{0};
  std::uint8_t Index{0};

  constexpr ResourceTile() = default;
  constexpr ResourceTile(std::uint8_t uint1_type, std::uint8_t uint1_index)
      : Type{uint1_type}, Index{uint1_index} {}

  constexpr std::int32_t Hash() const {  // L49
    return static_cast<std::int32_t>(Type) ^ static_cast<std::int32_t>(Index);
  }

  friend constexpr bool operator==(ResourceTile t_a, ResourceTile t_b) {
    return t_a.Type == t_b.Type && t_a.Index == t_b.Index;
  }
  friend constexpr bool operator!=(ResourceTile t_a, ResourceTile t_b) {
    return !(t_a == t_b);
  }
};

}  // namespace ora::map
