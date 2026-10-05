// UPSTREAM: OpenRA.Game/Graphics/Palette.cs @7d57605 L20-160 +
//           OpenRA.Game/Graphics/PaletteReference.cs @7d57605 L14-31
// 调色板值族:IPalette(uint32 ARGB × 256)、ImmutablePalette(不可变;
// 流构造的字节形态 = r<<2 | r>>6 高位复制 + remapTransparent→0 +
// remapShadow→140<<24)、MutablePalette(可变 + ApplyRemap)、IPaletteRemap。
// PaletteReference:名字 + 纹理行索引 + HasColorShift 查询(上游每精灵
// 查 HardwarePalette 字典;SpriteRenderer 批次的 OPT-A7 热点,届时改造)。
// 形态适配:
//   - 上游 IEnumerable<uint> 构造 → span;
//   - 流构造改为字节区间(调用方负责读流,.pal 768B RGB;formats 批接线);
//   - GetColor/AsReadOnly 扩展方法 → core::Color::FromArgb / const 引用直返
//     (C++ const& 天然只读,ReadOnlyPalette 包装类不需要 —— COVERAGE 登记)。
// The palette value family: IPalette (uint32 ARGB × 256), ImmutablePalette
// (immutable; the stream constructor's byte semantics = r<<2 | r>>6 high-bit
// replication + remapTransparent→0 + remapShadow→140<<24), MutablePalette
// (mutable + ApplyRemap), and IPaletteRemap. PaletteReference: name +
// texture-row index + the HasColorShift query (upstream queries the
// HardwarePalette dictionary per sprite — the OPT-A7 hot spot to be
// reworked in the SpriteRenderer batch). Shape adaptations:
//   - the IEnumerable<uint> constructor → a span;
//   - the stream constructor takes a byte span instead (the caller reads
//     the stream; the .pal 768-byte RGB layout wires up with formats);
//   - the GetColor/AsReadOnly extension methods → core::Color::FromArgb /
//     direct const& returns (a C++ const reference is read-only already —
//     the ReadOnlyPalette wrapper is unneeded; registered in COVERAGE).
#pragma once
import std;

#include "core/color.hpp"

namespace ora::gfx {

class HardwarePalette;  // PaletteReference::HasColorShift 查询 | the PaletteReference::HasColorShift query

/// Palette.Size(Palette.cs L30)。
/// Palette.Size (Palette.cs L30).
inline constexpr std::int32_t kPaletteSize = 256;

/// IPalette(Palette.cs L20-24):256 项 ARGB 只读视图。
/// IPalette (Palette.cs L20-24): a read-only view of 256 ARGB entries.
struct IPalette {
  /// this[int](越界未定义;上游数组直访 | out-of-range undefined; upstream
  /// indexes the array directly)。
  virtual std::uint32_t At(std::int32_t int4_index) const = 0;

  /// CopyToArray(Palette.cs L63-66):BlockCopy 语义 —— 自 offset*4 字节处
  /// 写 256×4 字节(小端 uint32 = BGRA 字节序)。
  /// CopyToArray (Palette.cs L63-66): BlockCopy semantics — writes
  /// 256×4 bytes at byte offset*4 (little-endian uint32s = BGRA bytes).
  virtual void CopyToArray(std::span<std::byte> vec_destination, std::int32_t int4_destination_offset) const = 0;

  virtual ~IPalette() = default;
};

/// IPaletteRemap(Palette.cs L26):逐索引重映射(玩家色等)。
/// IPaletteRemap (Palette.cs L26): per-index remapping (player colors etc.).
struct IPaletteRemap {
  virtual core::Color GetRemappedColor(core::Color color_original, std::int32_t int4_index) const = 0;
  virtual ~IPaletteRemap() = default;
};

/// 不可变调色板(Palette.cs L57-122)。
/// An immutable palette (Palette.cs L57-122).
class ImmutablePalette : public IPalette {
 public:
  /// 拷贝构造自另一调色板(L110-114)。
  /// Copy-constructs from another palette (L110-114).
  ImmutablePalette(const IPalette& palette_p);

  /// 拷贝 + 逐索引重映射(L103-108)。
  /// Copies then remaps per index (L103-108).
  ImmutablePalette(const IPalette& palette_p, const IPaletteRemap& remap_r);

  /// 逐项构造(L116-121;上游 IEnumerable<uint>,项数 ≤ 256 余零)。
  /// Constructs entry by entry (L116-121; upstream IEnumerable<uint>;
  /// entries beyond the span stay zero like upstream's remaining array).
  explicit ImmutablePalette(std::span<const std::uint32_t> vec_source_colors);

  /// 流构造(L74-101):768 字节 RGB 三元组(v = byte<<2 | byte>>6 复制高位);
  /// remapTransparent 索引置 0,remapShadow 索引置 140<<24。
  /// The stream constructor (L74-101): 768 RGB triplets
  /// (v = byte<<2 | byte>>6 replicating the high bits); remapTransparent
  /// indices zeroed, remapShadow indices set to 140<<24.
  ImmutablePalette(std::span<const std::byte> vec_rgb_triplets, std::span<const std::int32_t> vec_remap_transparent,
                   std::span<const std::int32_t> vec_remap_shadow);

  std::uint32_t At(std::int32_t int4_index) const override { return arr_colors_[static_cast<std::size_t>(int4_index)]; }
  void CopyToArray(std::span<std::byte> vec_destination, std::int32_t int4_destination_offset) const override;

 private:
  std::array<std::uint32_t, kPaletteSize> arr_colors_{};
};

/// 可变调色板(Palette.cs L124-159;受 HardwarePalette 委托持有,modifier
/// 经 AdjustPalette 改写)。
/// A mutable palette (Palette.cs L124-159; held by HardwarePalette and
/// rewritten by modifiers via AdjustPalette).
class MutablePalette : public IPalette {
 public:
  MutablePalette(const IPalette& palette_p);

  std::uint32_t At(std::int32_t int4_index) const override { return arr_colors_[static_cast<std::size_t>(int4_index)]; }

  /// this[int] setter 形态(上游索引器 set;内部直写)。
  /// The this[int] setter form (upstream's indexer set; a direct write).
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

/// 调色板引用(PaletteReference.cs L14-31;渲染侧持名 + 行索引)。
/// A palette reference (PaletteReference.cs L14-31; the render side holds
/// the name + row index).
class PaletteReference {
 public:
  PaletteReference(std::string_view str_name, std::int32_t int4_index, const IPalette& palette,
                   const HardwarePalette& hardware_palette);

  const std::string& Name() const { return str_name_; }
  const IPalette& Palette() const { return *ptr_palette_; }

  /// WorldRenderer/player 更新引用的上游 internal set 形态。
  /// The upstream internal-set form, driven by WorldRenderer/player code.
  void SetPalette(const IPalette& palette) { ptr_palette_ = &palette; }

  std::int32_t TextureIndex() const { return int4_texture_index_; }

  /// OPT-A7:HasColorShift 走 epoch 缓存 —— 上游每精灵查 HardwarePalette 的
  /// 字典(SpriteRenderer.ResolveTextureIndex 热点);HardwarePalette 仅在
  /// HasColorShift 可观察结果翻转时递增 epoch,故 (epoch, value) 缓存与逐次
  /// 字典查找语义等价(单线程渲染路径,无竞争)。
  /// OPT-A7: HasColorShift via the epoch cache — upstream queries the
  /// HardwarePalette dictionary per sprite (the
  /// SpriteRenderer.ResolveTextureIndex hot spot); the HardwarePalette bumps
  /// its epoch only when HasColorShift's observable result flips, so the
  /// (epoch, value) cache is semantically equal to the per-call dictionary
  /// lookup (single-threaded render path, no races).
  bool HasColorShift() const;  // 定义在 palette.cpp | defined in palette.cpp

 private:
  std::string str_name_;
  const IPalette* ptr_palette_ = nullptr;
  std::int32_t int4_texture_index_ = 0;
  const HardwarePalette* ptr_hardware_palette_ = nullptr;

  // OPT-A7:HasColorShift 的 (epoch, value) 缓存(UINT32_MAX = 未缓存)。
  // OPT-A7: the (epoch, value) cache for HasColorShift (UINT32_MAX = uncached).
  mutable std::uint32_t uint4_shift_epoch_cached_ = ~std::uint32_t{0};
  mutable bool b_shift_cached_ = false;
};

}  // namespace ora::gfx
