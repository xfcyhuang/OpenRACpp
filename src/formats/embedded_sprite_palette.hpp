// UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/EmbeddedSpritePalette.cs
// @b6fc03f L16-35(全文逐语义)
// 帧级调色板协商:帧专属字典命中优先,否则文件级兜底;两者皆无 → false。
// 形态:Dictionary<int,uint[]> → 有序 map(仅 TryGetValue 语义,无序敏);
// uint[](null)→ optional<vector>(帧级字典的 null 值条目上游返回 false,
// C++ 空 vector 视为"有"—— 加载路径不可达,值恒为构造产物)。消费方:
// PaletteFromEmbeddedSpritePalette(Phase 5 traits 接线)。
// The per-frame palette negotiation: a frame-specific dictionary hit wins,
// else the file-level fallback; neither → false. Shape: Dictionary<int,
// uint[]> → an ordered map (only TryGetValue semantics, order-insensitive);
// uint[] (null) → optional<vector> (a null-valued frame-dictionary entry
// returns false upstream while an empty C++ vector counts as present —
// unreachable on loader paths, where values are always constructed). See
// the Chinese note above. Consumer: PaletteFromEmbeddedSpritePalette
// (wired with the Phase 5 traits).
#pragma once
import std;

namespace ora::fmt {

class EmbeddedSpritePalette {
 public:
  EmbeddedSpritePalette(std::optional<std::vector<std::uint32_t>> vec_file_palette = std::nullopt,
                        std::optional<std::map<int, std::vector<std::uint32_t>>> map_frame_palettes = std::nullopt)
      : vec_file_palette_(std::move(vec_file_palette)),
        map_frame_palettes_(std::move(map_frame_palettes)) {}

  /// TryGetPaletteForFrame(L27-33):命中帧级则给帧级,否则给文件级;
  /// 返回是否有可用调色板(span 视图指向内部存储)。
  /// TryGetPaletteForFrame (L27-33): a frame-level hit yields the frame
  /// palette, else the file-level one; the return says whether any
  /// palette exists (the span view points into internal storage).
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

}  // namespace ora::fmt
