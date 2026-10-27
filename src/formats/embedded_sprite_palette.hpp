#pragma once
import std;

namespace ora::fmt {

class EmbeddedSpritePalette {
 public:
  EmbeddedSpritePalette(std::optional<std::vector<std::uint32_t>> vec_file_palette = std::nullopt,
                        std::optional<std::map<int, std::vector<std::uint32_t>>> map_frame_palettes = std::nullopt)
      : vec_file_palette_(std::move(vec_file_palette)),
        map_frame_palettes_(std::move(map_frame_palettes)) {}

  bool TryGetPaletteForFrame(int int4_frame, std::span<const std::uint32_t>& vec_palette) const {
    const std::vector<std::uint32_t>* ptr_palette = nullptr;
    if (map_frame_palettes_.has_value()) {
      const auto iter = map_frame_palettes_->find(int4_frame);
      if (iter != map_frame_palettes_->end())
        ptr_palette = &iter->second;
    }
    if (ptr_palette == nullptr)
      ptr_palette = vec_file_palette_.has_value() ? &*vec_file_palette_ : nullptr;

    if (ptr_palette == nullptr)
      return false;
    vec_palette = *ptr_palette;
    return true;
  }

 private:
  std::optional<std::vector<std::uint32_t>> vec_file_palette_;
  std::optional<std::map<int, std::vector<std::uint32_t>>> map_frame_palettes_;
};

}
