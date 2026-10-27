#pragma once
import std;

#include "core/color.hpp"

namespace ora::gfx {

class HardwarePalette;

inline constexpr std::int32_t kPaletteSize = 256;

struct IPalette {

  virtual std::uint32_t At(std::int32_t int4_index) const = 0;

  virtual void CopyToArray(std::span<std::byte> vec_destination, std::int32_t int4_destination_offset) const = 0;

  virtual ~IPalette() = default;
};

struct IPaletteRemap {
  virtual core::Color GetRemappedColor(core::Color color_original, std::int32_t int4_index) const = 0;
  virtual ~IPaletteRemap() = default;
};

class ImmutablePalette : public IPalette {
 public:

  ImmutablePalette(const IPalette& palette_p);

  ImmutablePalette(const IPalette& palette_p, const IPaletteRemap& remap_r);

  explicit ImmutablePalette(std::span<const std::uint32_t> vec_source_colors);

  ImmutablePalette(std::span<const std::byte> vec_rgb_triplets, std::span<const std::int32_t> vec_remap_transparent,
                   std::span<const std::int32_t> vec_remap_shadow);

  std::uint32_t At(std::int32_t int4_index) const override { return arr_colors_[static_cast<std::size_t>(int4_index)]; }
  void CopyToArray(std::span<std::byte> vec_destination, std::int32_t int4_destination_offset) const override;

 private:
  std::array<std::uint32_t, kPaletteSize> arr_colors_{};
};

class MutablePalette : public IPalette {
 public:
  MutablePalette(const IPalette& palette_p);

  std::uint32_t At(std::int32_t int4_index) const override { return arr_colors_[static_cast<std::size_t>(int4_index)]; }

  void SetAt(std::int32_t int4_index, std::uint32_t uint4_color) {
    arr_colors_[static_cast<std::size_t>(int4_index)] = uint4_color;
  }

  void SetColor(std::int32_t int4_index, core::Color color_c) {
    SetAt(int4_index, color_c.ToArgb());
  }

  void SetFromPalette(const IPalette& palette_p);
  void ApplyRemap(const IPaletteRemap& remap_r);
  void CopyToArray(std::span<std::byte> vec_destination, std::int32_t int4_destination_offset) const override;

 private:
  std::array<std::uint32_t, kPaletteSize> arr_colors_{};
};

class PlayerColorRemap : public IPaletteRemap {
 public:
  PlayerColorRemap(std::span<const std::int32_t> vec_remap_indices, core::Color color_c)
      : vec_remap_indices_{vec_remap_indices.begin(), vec_remap_indices.end()} {

    const auto [fp4_r, fp4_g, fp4_b] = color_c.ToLinear();
    const auto arr_hsv = core::Color::RgbToHsv(fp4_r, fp4_g, fp4_b);
    fp4_hue_ = arr_hsv[0];
    fp4_saturation_ = arr_hsv[1];
    fp4_value_ = arr_hsv[2];
  }

  core::Color GetRemappedColor(core::Color color_original, std::int32_t int4_index) const override {
    if (std::ranges::find(vec_remap_indices_, int4_index) == vec_remap_indices_.end())
      return color_original;

    const auto [fp4_r, fp4_g, fp4_b] = color_original.ToLinear();
    const float fp4_value_orig = std::max(fp4_r, std::max(fp4_g, fp4_b));
    const auto arr_rgb = core::Color::HsvToRgb(fp4_hue_, fp4_saturation_, fp4_value_orig * fp4_value_);
    return core::Color::FromLinear(color_original.A(), arr_rgb[0], arr_rgb[1], arr_rgb[2]);
  }

 private:
  std::vector<std::int32_t> vec_remap_indices_;
  float fp4_hue_ = 0.0f;
  float fp4_saturation_ = 0.0f;
  float fp4_value_ = 0.0f;
};

class PaletteReference {
 public:
  PaletteReference(std::string_view str_name, std::int32_t int4_index, const IPalette& palette,
                   const HardwarePalette& hardware_palette);

  const std::string& Name() const { return str_name_; }
  const IPalette& Palette() const { return *ptr_palette_; }

  void SetPalette(const IPalette& palette) { ptr_palette_ = &palette; }

  std::int32_t TextureIndex() const { return int4_texture_index_; }

  bool HasColorShift() const;

 private:
  std::string str_name_;
  const IPalette* ptr_palette_ = nullptr;
  std::int32_t int4_texture_index_ = 0;
  const HardwarePalette* ptr_hardware_palette_ = nullptr;

  mutable std::uint32_t uint4_shift_epoch_cached_ = ~std::uint32_t{0};
  mutable bool b_shift_cached_ = false;
};

}
