// UPSTREAM: OpenRA.Game/Primitives/Color.cs @7d57605 L20-232,374(值类型核心 + TryParse/ToString;
//          HSV/HSL/线性 gamma 转换属渲染域,Phase 4 随 gfx 移植)
//          Value-type core + TryParse/ToString (the HSV/HSL/linear-gamma
//          conversions belong to the rendering domain and arrive with gfx in
//          Phase 4).
//
// 语义要点 / Semantic notes:
//  - TryParse:Trim 后长度必须恰为 6(RRGGBB)或 8(RRGGBBAA),十六进制大小写均可
//    TryParse: after Trim the length must be exactly 6 (RRGGBB) or 8
//    (RRGGBBAA); hex digits are case-insensitive.
//  - ToString:A==255 输出 RRGGBB,否则 RRGGBBAA,大写十六进制
//    ToString: RRGGBB when A==255, otherwise RRGGBBAA, uppercase hex.
#pragma once
import std;
#include "core/text.hpp"

namespace ora::core {

/// OpenRA 的 Color 值类型(Color.cs L20):ARGB 打包 uint32,通道序 A<<24|R<<16|G<<8|B
/// The OpenRA Color value type (Color.cs L20): ARGB packed into uint32 with
/// channel order A<<24|R<<16|G<<8|B.
struct Color {
  std::uint32_t argb{};

  // 显式底层构造通道序;默认构造 = 0x00000000(C# default)
  // Channel-order-aware constructors; default = 0x00000000 (C# default).
  static Color FromArgb(int int4_red, int int4_green, int int4_blue) {
    return FromArgb(255, int4_red, int4_green, int4_blue);
  }
  static Color FromArgb(int int4_alpha, int int4_red, int int4_green, int int4_blue) {
    // C#: (uint)(((byte)alpha << 24) + ((byte)red << 16) + ((byte)green << 8) + (byte)blue)
    // 通道截断到 byte 后打包(负数经 byte 转换回绕)
    // Channels truncate to byte before packing (negative values wrap via the
    // byte conversion).
    const auto u8_a = static_cast<std::uint8_t>(int4_alpha);
    const auto u8_r = static_cast<std::uint8_t>(int4_red);
    const auto u8_g = static_cast<std::uint8_t>(int4_green);
    const auto u8_b = static_cast<std::uint8_t>(int4_blue);
    return FromArgbRaw((std::uint32_t{u8_a} << 24) + (std::uint32_t{u8_r} << 16) +
                       (std::uint32_t{u8_g} << 8) + u8_b);
  }
  static Color FromArgb(std::uint32_t uint4_argb) { return FromArgbRaw(uint4_argb); }
  static Color FromArgbRaw(std::uint32_t uint4_argb) {
    Color color_ret;
    color_ret.argb = uint4_argb;
    return color_ret;
  }

  std::uint8_t A() const { return static_cast<std::uint8_t>(argb >> 24); }
  std::uint8_t R() const { return static_cast<std::uint8_t>(argb >> 16); }
  std::uint8_t G() const { return static_cast<std::uint8_t>(argb >> 8); }
  std::uint8_t B() const { return static_cast<std::uint8_t>(argb); }
  std::uint32_t ToArgb() const { return argb; }

  bool operator==(const Color&) const = default;

  /// TryParse(Color.cs L164-184):Trim → 长度 6/8 → 每两位十六进制解析
  /// TryParse (Color.cs L164-184): Trim → length 6/8 → parse each hex pair.
  static bool TryParse(std::string_view sv_value, Color& color_out) {
    color_out = Color{};
    sv_value = TrimNetWhiteSpace(sv_value);
    if (sv_value.size() != 6 && sv_value.size() != 8)
      return false;

    std::uint8_t u8_alpha{255};
    std::uint8_t u8_red{}, u8_green{}, u8_blue{};
    if (!TryParseHexByte(sv_value.substr(0, 2), u8_red) ||
        !TryParseHexByte(sv_value.substr(2, 2), u8_green) ||
        !TryParseHexByte(sv_value.substr(4, 2), u8_blue))
      return false;

    if (sv_value.size() == 8 && !TryParseHexByte(sv_value.substr(6, 2), u8_alpha))
      return false;

    color_out = FromArgb(u8_alpha, u8_red, u8_green, u8_blue);
    return true;
  }

  /// ToString(Color.cs L226-232):CryptoUtil.ToHex → 大写十六进制串
  /// ToString (Color.cs L226-232): CryptoUtil.ToHex → uppercase hex string.
  std::string ToString() const {
    const auto hex = [](std::uint8_t u8_v) {
      static constexpr char kHexDigits[] = "0123456789ABCDEF";
      return std::string{kHexDigits[u8_v >> 4], kHexDigits[u8_v & 0xF]};
    };
    if (A() == 255)
      return hex(R()) + hex(G()) + hex(B());
    return hex(R()) + hex(G()) + hex(B()) + hex(A());
  }

 private:
  /// byte.TryParse(NumberStyles.HexNumber):两位恰消费,越界/非十六进制失败
  /// byte.TryParse(NumberStyles.HexNumber): exactly two hex digits consumed;
  /// anything else fails.
  static bool TryParseHexByte(std::string_view sv, std::uint8_t& u8_out) {
    if (sv.size() != 2)
      return false;
    std::uint8_t u8_v{};
    for (const char chr_c : sv) {
      std::uint8_t u8_d;
      if (chr_c >= '0' && chr_c <= '9')
        u8_d = static_cast<std::uint8_t>(chr_c - '0');
      else if (chr_c >= 'a' && chr_c <= 'f')
        u8_d = static_cast<std::uint8_t>(chr_c - 'a' + 10);
      else if (chr_c >= 'A' && chr_c <= 'F')
        u8_d = static_cast<std::uint8_t>(chr_c - 'A' + 10);
      else
        return false;
      u8_v = static_cast<std::uint8_t>((u8_v << 4) | u8_d);
    }
    u8_out = u8_v;
    return true;
  }
};

}  // namespace ora::core
