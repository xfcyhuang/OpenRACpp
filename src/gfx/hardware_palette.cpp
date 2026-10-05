// UPSTREAM: OpenRA.Game/Graphics/HardwarePalette.cs @7d57605 L18-161(实现)
// Implementations of HardwarePalette (the header carries the full UPSTREAM
// anchors, the OPT-A7 dirty-row design, and the OPT-C5 equivalence notes).
#include "gfx/hardware_palette.hpp"

#include "gfx/gfx_util.hpp"  // NextPowerOf2(Exts)| NextPowerOf2 (Exts)

namespace ora::gfx {

HardwarePalette::HardwarePalette(RenderThread* ptr_render) : ptr_render_{ptr_render} {
  if (ptr_render_ != nullptr) {
    opt_texture_.emplace(*ptr_render_);
    opt_color_shifts_.emplace(*ptr_render_);
  }
}

bool HardwarePalette::Contains(std::string_view str_name) const {
  return map_mutable_palettes_.contains(str_name) || map_palettes_.contains(str_name);
}

const IPalette& HardwarePalette::GetPalette(std::string_view str_name) const {
  if (const auto iter_mutable = map_mutable_palettes_.find(str_name); iter_mutable != map_mutable_palettes_.end())
    return iter_mutable->second;  // 上游 mutable.AsReadOnly() → const 引用 | → a const reference
  if (const auto iter_immutable = map_palettes_.find(str_name); iter_immutable != map_palettes_.end())
    return iter_immutable->second;
  throw std::runtime_error("Palette `" + std::string{str_name} + "` does not exist");
}

std::int32_t HardwarePalette::GetPaletteIndex(std::string_view str_name) const {
  if (const auto iter_index = map_indices_.find(str_name); iter_index != map_indices_.end())
    return iter_index->second;
  throw std::runtime_error("Palette `" + std::string{str_name} + "` does not exist");
}

void HardwarePalette::AddPalette(std::string_view str_name, ImmutablePalette palette_p, bool b_allow_modifiers) {
  if (map_palettes_.contains(str_name))
    throw std::runtime_error("Palette " + std::string{str_name} + " has already been defined");

  // PERF: the first row in the palette textures is reserved as a placeholder
  // for non-indexed sprites that do not have a color-shift applied. This
  // provides a quick shortcut to avoid querying the color-shift texture for
  // every pixel only to find that most are not shifted.
  // (HardwarePalette.cs L61-64)
  const auto int4_index = static_cast<std::int32_t>(map_palettes_.size()) + 1;
  map_indices_.emplace(std::string{str_name}, int4_index);
  map_palettes_.emplace(std::string{str_name}, std::move(palette_p));

  if (int4_index >= int4_height_) {
    // 高度增长:纹理尺寸变化必须整体重建 —— 全量脏(OPT-A7)。缓冲扩容
    // **保留旧行**(上游 Array.Resize 语义;误用 assign 清零会丢已写行
    // —— gfx_test 行内容断言实证)。
    // Height growth: the texture size changes, forcing a rebuild — full
    // dirty (OPT-A7). The buffer grows **preserving existing rows**
    // (upstream's Array.Resize semantics; an assign-by-mistake zeroes
    // previously written rows — caught by gfx_test's row-content
    // assertions).
    int4_height_ = NextPowerOf2(int4_index + 1);
    vec_buffer_.resize(static_cast<std::size_t>(int4_height_) * kPaletteSize * 4, std::byte{0});
    vec_color_shift_buffer_.resize(static_cast<std::size_t>(int4_height_) * 8, 0.0f);
    vecb_dirty_rows_.assign(static_cast<std::size_t>(int4_height_), false);
    b_full_dirty_ = true;
    b_shifts_dirty_ = true;
  }

  const auto& palette_registered = map_palettes_.find(str_name)->second;
  if (b_allow_modifiers)
    map_mutable_palettes_.emplace(std::string{str_name}, MutablePalette{palette_registered});
  else
    CopyPaletteToBuffer(int4_index, palette_registered);
}

void HardwarePalette::ReplacePalette(std::string_view str_name, const IPalette& palette_p) {
  if (const auto iter_mutable = map_mutable_palettes_.find(str_name); iter_mutable != map_mutable_palettes_.end()) {
    map_palettes_.insert_or_assign(std::string{str_name}, ImmutablePalette{palette_p});
    auto palette_mutable_new = MutablePalette{palette_p};
    CopyPaletteToBuffer(GetPaletteIndex(str_name), palette_mutable_new);
    map_mutable_palettes_.insert_or_assign(std::string{str_name}, std::move(palette_mutable_new));
  } else if (map_palettes_.contains(str_name)) {
    auto palette_immutable_new = ImmutablePalette{palette_p};
    CopyPaletteToBuffer(GetPaletteIndex(str_name), palette_immutable_new);
    map_palettes_.insert_or_assign(std::string{str_name}, std::move(palette_immutable_new));
  } else {
    throw std::runtime_error("Palette `" + std::string{str_name} + "` does not exist");
  }
  CopyBufferToTexture();
}

void HardwarePalette::SetColorShift(std::string_view str_name, float float_hue_offset, float float_sat_offset,
                                    float float_value_multiplier, float float_min_hue, float float_max_hue) {
  const auto int4_index = GetPaletteIndex(str_name);
  const bool b_had_shift =
      vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 0] != 0 ||
      vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 1] != 0;
  vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 0] = float_min_hue;
  vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 1] = float_max_hue;
  vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 4] = float_hue_offset;
  vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 5] = float_sat_offset;
  vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 6] = float_value_multiplier;
  b_shifts_dirty_ = true;  // OPT-A7:上传延迟(上游不在此上传 | no upload here upstream)

  // OPT-A7:HasColorShift 的可观察结果翻转时递增 epoch —— PaletteReference
  // 的 epoch 缓存据此失效(消除每精灵的字符串字典查找)。
  // OPT-A7: bump the epoch whenever HasColorShift's observable result flips —
  // PaletteReference epoch caches invalidate on it (killing the per-sprite
  // string-dictionary lookup).
  const bool b_has_shift_now = float_min_hue != 0 || float_max_hue != 0;
  if (b_had_shift != b_has_shift_now)
    ++uint4_shift_epoch_;
}

bool HardwarePalette::HasColorShift(std::string_view str_name) const {
  const auto int4_index = GetPaletteIndex(str_name);
  return vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 0] != 0 ||
         vec_color_shift_buffer_[static_cast<std::size_t>(8) * int4_index + 1] != 0;
}

void HardwarePalette::Initialize() {
  CopyModifiablePalettesToBuffer();
  CopyBufferToTexture();
}

void HardwarePalette::ApplyModifiers(std::span<IPaletteModifier* const> vec_palette_mods) {
  for (auto* ptr_mod : vec_palette_mods)
    ptr_mod->AdjustPalette(map_mutable_palettes_);

  // Update our texture with the changes.
  // (HardwarePalette.cs L143-144;OPT-A7:仅脏行增量上传)
  // (HardwarePalette.cs L143-144; OPT-A7: incremental upload of dirty rows only)
  CopyModifiablePalettesToBuffer();
  CopyBufferToTexture();

  // Reset modified palettes back to their original colors, ready for next
  // time. (HardwarePalette.cs L147-153;重置是 CPU 侧行为,不标脏)
  // (HardwarePalette.cs L147-153; the reset is CPU-side — no dirty bits)
  for (auto& [str_name, palette_mutable] : map_mutable_palettes_)
    palette_mutable.SetFromPalette(map_palettes_.at(str_name));
}

void HardwarePalette::CopyPaletteToBuffer(std::int32_t int4_index, const IPalette& palette_p) {
  palette_p.CopyToArray(vec_buffer_, int4_index * kPaletteSize);
  if (!b_full_dirty_ && int4_index < static_cast<std::int32_t>(vecb_dirty_rows_.size()))
    vecb_dirty_rows_[static_cast<std::size_t>(int4_index)] = true;
}

void HardwarePalette::CopyModifiablePalettesToBuffer() {
  for (const auto& [str_name, palette_mutable] : map_mutable_palettes_)
    CopyPaletteToBuffer(map_indices_.at(str_name), palette_mutable);
}

void HardwarePalette::CopyBufferToTexture() {
  // 纯数据模式:路径决策与脏位推进照常,仅 GL 发射跳过(上游无此模式;
  // Utility 路径不构造 HardwarePalette —— 形态适配见文件头)。
  // Data-only mode: path decisions and dirty-bit progression proceed as
  // usual; only the GL emission skips (upstream has no such mode — the
  // Utility path never constructs a HardwarePalette; see the header).
  Texture* ptr_texture = opt_texture_ ? &*opt_texture_ : nullptr;
  Texture* ptr_shifts = opt_color_shifts_ ? &*opt_color_shifts_ : nullptr;

  // 启发式:脏行过半时一次 SetData 便宜过半数 SetSubImage(OPT-A7;
  // OPT-C5 等价性不受影响 —— 字节与落点相同)。
  // Heuristic: past half-dirty, one SetData beats a majority of
  // SetSubImage calls (OPT-A7; OPT-C5 equivalence intact — same bytes at
  // the same destinations).
  std::int32_t int4_dirty_rows = 0;
  for (auto row = 0; row < int4_height_; ++row)
    int4_dirty_rows += vecb_dirty_rows_[static_cast<std::size_t>(row)] ? 1 : 0;

  if (b_full_dirty_ || int4_dirty_rows * 2 >= int4_height_) {
    if (ptr_texture != nullptr)
      ptr_texture->SetData(vec_buffer_, kPaletteSize, int4_height_);
    vecb_dirty_rows_.assign(vecb_dirty_rows_.size(), false);
    b_last_upload_full_ = true;
  } else {
    for (auto row = 0; row < int4_height_; ++row) {
      if (!vecb_dirty_rows_[static_cast<std::size_t>(row)])
        continue;
      if (ptr_texture != nullptr) {
        // SetSubData 契约:源 = 行距整纹理宽的位图,SKIP 定位 —— 传至该行
        // 末的子区间即可(覆盖下界 = 最后取出行末像素)。
        // The SetSubData contract: a bitmap pitched at the full texture
        // width located via SKIP — a sub-span through this row's end
        // suffices (coverage floor = the last fetched row's final pixel).
        const auto vec_row_source =
            std::span<const std::byte>{vec_buffer_}.subspan(0, (static_cast<std::size_t>(row) + 1) * kPaletteSize * 4);
        ptr_texture->SetSubData(vec_row_source, 0, row, kPaletteSize, 1);
      }
      vecb_dirty_rows_[static_cast<std::size_t>(row)] = false;
    }
    b_last_upload_full_ = false;
  }

  if (b_full_dirty_ || b_shifts_dirty_) {
    if (ptr_shifts != nullptr)
      ptr_shifts->SetFloatData(vec_color_shift_buffer_, 2, int4_height_);
    b_shifts_dirty_ = false;
  }

  b_full_dirty_ = false;
}

}  // namespace ora::gfx
