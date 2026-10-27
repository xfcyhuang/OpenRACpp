#pragma once
import std;
#include "core/text.hpp"

namespace ora::core {

struct Color {
  std::uint32_t argb{};

  static Color FromArgb(int int4_red, int int4_green, int int4_blue) {
    return FromArgb(255, int4_red, int4_green, int4_blue);
  }
  static Color FromArgb(int int4_alpha, int int4_red, int int4_green, int int4_blue) {

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

  float GetBrightness() const {
    const std::uint8_t u8_max = std::max({R(), G(), B()});
    return static_cast<float>(u8_max) / 255.0f;
  }

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

  std::string ToString() const {
    const auto hex = [](std::uint8_t u8_v) {
      static constexpr char kHexDigits[] = "0123456789ABCDEF";
      return std::string{kHexDigits[u8_v >> 4], kHexDigits[u8_v & 0xF]};
    };
    if (A() == 255)
      return hex(R()) + hex(G()) + hex(B());
    return hex(R()) + hex(G()) + hex(B()) + hex(A());
  }

  std::array<float, 3> ToLinear() const {
    const float fp4_a = static_cast<float>(A());
    return {SrgbToLinear(static_cast<float>(R()) / fp4_a),
            SrgbToLinear(static_cast<float>(G()) / fp4_a),
            SrgbToLinear(static_cast<float>(B()) / fp4_a)};
  }

  static Color FromLinear(std::uint8_t u8_a, float fp4_r, float fp4_g, float fp4_b) {
    const auto round_channel = [](float fp4_c, float fp4_alpha) {

      const float fp4_rounded = std::nearbyint(LinearToSrgb(fp4_c) * fp4_alpha);
      const std::int64_t int8_v = static_cast<std::int64_t>(fp4_rounded);
      return static_cast<std::uint8_t>(std::clamp<std::int64_t>(int8_v, 0, 255));
    };
    return FromArgb(u8_a, round_channel(fp4_r, u8_a), round_channel(fp4_g, u8_a),
                    round_channel(fp4_b, u8_a));
  }

  static std::array<float, 3> HsvToRgb(float fp4_h, float fp4_s, float fp4_v) {
    const float fp4_px = std::abs(fp4_h * 6.0f - 3.0f);
    const float fp4_py = std::abs(std::fmod(fp4_h + 2.0f / 3.0f, 1.0f) * 6.0f - 3.0f);
    const float fp4_pz = std::abs(std::fmod(fp4_h + 1.0f / 3.0f, 1.0f) * 6.0f - 3.0f);
    const auto lerp = [](float fp4_a, float fp4_b, float fp4_t) { return fp4_a + fp4_t * (fp4_b - fp4_a); };
    const auto clamp01 = [](float fp4_c) { return std::clamp(fp4_c, 0.0f, 1.0f); };
    return {fp4_v * lerp(1.0f, clamp01(fp4_px - 1.0f), fp4_s),
            fp4_v * lerp(1.0f, clamp01(fp4_py - 1.0f), fp4_s),
            fp4_v * lerp(1.0f, clamp01(fp4_pz - 1.0f), fp4_s)};
  }

  static std::array<float, 3> RgbToHsv(std::uint8_t u8_r, std::uint8_t u8_g, std::uint8_t u8_b) {
    return RgbToHsv(u8_r / 255.0f, u8_g / 255.0f, u8_b / 255.0f);
  }

  static std::array<float, 3> RgbToHsv(float fp4_r, float fp4_g, float fp4_b) {
    const float fp4_max = std::max(fp4_r, std::max(fp4_g, fp4_b));
    const float fp4_min = std::min(fp4_r, std::min(fp4_g, fp4_b));
    const float fp4_delta = fp4_max - fp4_min;
    const float fp4_v = fp4_max;
    if (fp4_delta == 0.0f)
      return {0.0f, 0.0f, fp4_v};

    float fp4_hue;
    if (fp4_r == fp4_max)
      fp4_hue = (fp4_g - fp4_b) / (6.0f * fp4_delta);
    else if (fp4_g == fp4_max)
      fp4_hue = (fp4_b - fp4_r) / (6.0f * fp4_delta) + 1.0f / 3.0f;
    else
      fp4_hue = (fp4_r - fp4_g) / (6.0f * fp4_delta) + 2.0f / 3.0f;

    float fp4_h = fp4_hue - static_cast<float>(static_cast<std::int32_t>(fp4_hue));
    if (fp4_h < 0.0f)
      fp4_h += 1.0f;
    return {fp4_h, fp4_delta / fp4_max, fp4_v};
  }

  static Color FromAhsv(int int4_alpha, float fp4_h, float fp4_s, float fp4_v) {
    const auto [fp4_r, fp4_g, fp4_b] = HsvToRgb(fp4_h, fp4_s, fp4_v);

    const auto round = [](float fp4_x) {
      const float fp4_rounded = std::nearbyint(255.0f * fp4_x);
      const std::int64_t int8_v = static_cast<std::int64_t>(fp4_rounded);
      return static_cast<int>(std::clamp<std::int64_t>(int8_v, 0, 255));
    };
    return FromArgb(int4_alpha, round(fp4_r), round(fp4_g), round(fp4_b));
  }
  static Color FromAhsv(float fp4_h, float fp4_s, float fp4_v) {
    return FromAhsv(255, fp4_h, fp4_s, fp4_v);
  }

  std::tuple<int, float, float, float> ToAhsv() const {
    const auto [fp4_h, fp4_s, fp4_v] = RgbToHsv(R(), G(), B());
    return {A(), fp4_h, fp4_s, fp4_v};
  }

 private:

  static float SrgbToLinear(float fp4_c) {

    return fp4_c <= 0.04045f
               ? fp4_c / 12.92f
               : static_cast<float>(std::pow((fp4_c + 0.055f) / 1.055f, 2.4));
  }

  static float LinearToSrgb(float fp4_c) {
    return fp4_c <= 0.0031308f
               ? fp4_c * 12.92f
               : 1.055f * static_cast<float>(std::pow(fp4_c, 1.0f / 2.4f)) - 0.055f;
  }

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

}
