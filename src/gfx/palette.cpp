// UPSTREAM: OpenRA.Game/Graphics/Palette.cs @7d57605 L20-160 +
//           OpenRA.Game/Graphics/PaletteReference.cs @7d57605 L14-31(实现)
// Implementations of the palette family (headers in palette.hpp carry the
// full UPSTREAM anchors and deviation notes).
#include "gfx/palette.hpp"

#include "gfx/hardware_palette.hpp"

namespace ora::gfx {

namespace {
/// uint32 数组 → 字节视图(BlockCopy 的等价物;全局 -fno-strict-aliasing)。
/// A byte view over uint32 storage (the BlockCopy equivalent; global
/// -fno-strict-aliasing).
std::span<const std::byte> AsBytes(const std::array<std::uint32_t, kPaletteSize>& arr_colors) {
  return std::span<const std::byte>{reinterpret_cast<const std::byte*>(arr_colors.data()), arr_colors.size() * 4};
}
}  // namespace

// ———— ImmutablePalette ————

ImmutablePalette::ImmutablePalette(const IPalette& palette_p) {
  for (auto i = 0; i < kPaletteSize; ++i)
    arr_colors_[static_cast<std::size_t>(i)] = palette_p.At(i);
}

ImmutablePalette::ImmutablePalette(const IPalette& palette_p, const IPaletteRemap& remap_r)
    : ImmutablePalette{palette_p} {
  for (auto i = 0; i < kPaletteSize; ++i)
    arr_colors_[static_cast<std::size_t>(i)] =
        remap_r.GetRemappedColor(core::Color::FromArgb(At(i)), i).ToArgb();
}

ImmutablePalette::ImmutablePalette(std::span<const std::uint32_t> vec_source_colors) {
  auto i = 0;
  for (const auto uint4_source_color : vec_source_colors)
    arr_colors_[static_cast<std::size_t>(i++)] = uint4_source_color;
}

ImmutablePalette::ImmutablePalette(std::span<const std::byte> vec_rgb_triplets,
                                   std::span<const std::int32_t> vec_remap_transparent,
                                   std::span<const std::int32_t> vec_remap_shadow) {
  assert(vec_rgb_triplets.size() >= static_cast<std::size_t>(kPaletteSize) * 3);
  for (auto i = 0; i < kPaletteSize; ++i) {
    const auto u8_r = static_cast<std::uint8_t>(std::to_integer<std::uint8_t>(
                          vec_rgb_triplets[static_cast<std::size_t>(i) * 3])
                      << 2);
    const auto u8_g = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(vec_rgb_triplets[static_cast<std::size_t>(i) * 3 + 1]) << 2);
    const auto u8_b = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(vec_rgb_triplets[static_cast<std::size_t>(i) * 3 + 2]) << 2);

    // Replicate high bits into the (currently zero) low bits.
    // (Palette.cs L88-90)
    const auto u8_r_replicated = static_cast<std::uint8_t>(u8_r | (u8_r >> 6));
    const auto u8_g_replicated = static_cast<std::uint8_t>(u8_g | (u8_g >> 6));
    const auto u8_b_replicated = static_cast<std::uint8_t>(u8_b | (u8_b >> 6));

    arr_colors_[static_cast<std::size_t>(i)] =
        (std::uint32_t{255} << 24) | (std::uint32_t{u8_r_replicated} << 16) |
        (std::uint32_t{u8_g_replicated} << 8) | u8_b_replicated;
  }

  for (const auto int4_i : vec_remap_transparent)
    arr_colors_[static_cast<std::size_t>(int4_i)] = 0;

  for (const auto int4_i : vec_remap_shadow)
    arr_colors_[static_cast<std::size_t>(int4_i)] = std::uint32_t{140} << 24;
}

void ImmutablePalette::CopyToArray(std::span<std::byte> vec_destination,
                                   std::int32_t int4_destination_offset) const {
  const auto bytes = AsBytes(arr_colors_);
  std::memcpy(vec_destination.data() + static_cast<std::size_t>(int4_destination_offset) * 4, bytes.data(),
              bytes.size());
}

// ———— MutablePalette ————

MutablePalette::MutablePalette(const IPalette& palette_p) { SetFromPalette(palette_p); }

void MutablePalette::SetFromPalette(const IPalette& palette_p) {
  // 上游 p.CopyToArray(colors, 0)(uint[] 以 BlockCopy 视作字节);经栈缓冲
  // 中转保持 CopyToArray 的字节接口不变。
  // Upstream's p.CopyToArray(colors, 0) (the uint[] viewed as bytes by
  // BlockCopy); routed through a stack buffer to keep CopyToArray's byte
  // interface unchanged.
  std::array<std::byte, kPaletteSize * 4> arr_bytes{};
  palette_p.CopyToArray(arr_bytes, 0);
  std::memcpy(arr_colors_.data(), arr_bytes.data(), arr_bytes.size());
}

void MutablePalette::ApplyRemap(const IPaletteRemap& remap_r) {
  for (auto i = 0; i < kPaletteSize; ++i)
    arr_colors_[static_cast<std::size_t>(i)] =
        remap_r.GetRemappedColor(core::Color::FromArgb(At(i)), i).ToArgb();
}

void MutablePalette::CopyToArray(std::span<std::byte> vec_destination,
                                 std::int32_t int4_destination_offset) const {
  const auto bytes = AsBytes(arr_colors_);
  std::memcpy(vec_destination.data() + static_cast<std::size_t>(int4_destination_offset) * 4, bytes.data(),
              bytes.size());
}

// ———— PaletteReference ————

PaletteReference::PaletteReference(std::string_view str_name, std::int32_t int4_index, const IPalette& palette,
                                   const HardwarePalette& hardware_palette)
    : str_name_{str_name},
      ptr_palette_{&palette},
      int4_texture_index_{int4_index},
      ptr_hardware_palette_{&hardware_palette} {}

bool PaletteReference::HasColorShift() const {
  // OPT-A7 快路径:epoch 未变直接用缓存(零字符串、零字典)。
  // OPT-A7 fast path: an unchanged epoch serves the cache (zero strings,
  // zero dictionaries).
  const std::uint32_t uint4_epoch = ptr_hardware_palette_->ShiftEpoch();
  if (uint4_epoch == uint4_shift_epoch_cached_)
    return b_shift_cached_;
  b_shift_cached_ = ptr_hardware_palette_->HasColorShift(str_name_);
  uint4_shift_epoch_cached_ = uint4_epoch;
  return b_shift_cached_;
}

}  // namespace ora::gfx
