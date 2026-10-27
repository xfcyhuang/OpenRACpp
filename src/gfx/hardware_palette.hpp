#pragma once
import std;

#include "gfx/palette.hpp"
#include "gfx/texture.hpp"

namespace ora::gfx {

struct IPaletteModifier {
  virtual void AdjustPalette(std::map<std::string, MutablePalette, std::less<>>& map_mutable_palettes) = 0;
  virtual ~IPaletteModifier() = default;
};

class HardwarePalette {
 public:

  explicit HardwarePalette(RenderThread* ptr_render);

  HardwarePalette(const HardwarePalette&) = delete;
  HardwarePalette& operator=(const HardwarePalette&) = delete;

  bool Contains(std::string_view str_name) const;

  const IPalette& GetPalette(std::string_view str_name) const;

  std::int32_t GetPaletteIndex(std::string_view str_name) const;

  void AddPalette(std::string_view str_name, ImmutablePalette palette_p, bool b_allow_modifiers);

  void ReplacePalette(std::string_view str_name, const IPalette& palette_p);

  void SetColorShift(std::string_view str_name, float float_hue_offset, float float_sat_offset,
                     float float_value_multiplier, float float_min_hue, float float_max_hue);

  bool HasColorShift(std::string_view str_name) const;

  void Initialize();

  void ApplyModifiers(std::span<IPaletteModifier* const> vec_palette_mods);

  void CopyBufferToTexture();

  std::int32_t Height() const { return int4_height_; }

  std::uint32_t ShiftEpoch() const { return uint4_shift_epoch_; }

  Texture* TextureOrNull() { return opt_texture_ ? &*opt_texture_ : nullptr; }
  Texture* ColorShiftsOrNull() { return opt_color_shifts_ ? &*opt_color_shifts_ : nullptr; }

  bool FullDirty() const { return b_full_dirty_; }
  bool ShiftsDirty() const { return b_shifts_dirty_; }
  bool RowDirty(std::int32_t int4_row) const {
    return int4_row < static_cast<std::int32_t>(vecb_dirty_rows_.size()) && vecb_dirty_rows_[int4_row];
  }

  bool LastUploadFull() const { return b_last_upload_full_; }

  std::span<const std::byte> BufferForTest() const { return vec_buffer_; }

 private:

  void CopyPaletteToBuffer(std::int32_t int4_index, const IPalette& palette_p);

  void CopyModifiablePalettesToBuffer();

  RenderThread* ptr_render_ = nullptr;
  std::optional<Texture> opt_texture_;
  std::optional<Texture> opt_color_shifts_;

  std::int32_t int4_height_ = 0;
  std::map<std::string, ImmutablePalette, std::less<>> map_palettes_;
  std::map<std::string, MutablePalette, std::less<>> map_mutable_palettes_;
  std::map<std::string, std::int32_t, std::less<>> map_indices_;
  std::vector<std::byte> vec_buffer_;
  std::vector<float> vec_color_shift_buffer_;

  bool b_full_dirty_ = true;
  bool b_shifts_dirty_ = true;
  bool b_last_upload_full_ = true;
  std::vector<bool> vecb_dirty_rows_;
  std::uint32_t uint4_shift_epoch_ = 0;
};

}
