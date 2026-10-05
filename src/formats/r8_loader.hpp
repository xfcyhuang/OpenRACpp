// UPSTREAM: OpenRA.Mods.D2k/SpriteLoaders/R8Loader.cs @b6fc03f L24-229(全文逐语义)
// Dune 2000 的 R8/R16 图像:帧头(1 字节 type,前导 0 跳过)+ w/h/x/y 四
// i32 + imageHandle u32 + paletteHandle i32 + bpp u8(8/16)+ frameW/frameH
// 两 u8 + 对齐 u8。bpp16 数据为 RGB555,展开成 BGRA32(0xFF alpha,通道
// 左移对齐);bpp8 为索引数据。type==1 且 paletteHandle!=0 时帧尾随 512
// 字节调色板(RGB555 同式展开,索引 0 重映射为透明);type==2 沿用上一帧
// 调色板。RemappableFrame 将索引数据按需展开为 BGRA:shroud→fog 的位运算
// 半调(~((c>>1)&0x7F7F7F))、索引 1 → 影色(140<<24)、玩家色 240..255 的
// PlayerColorRemap 重建(HSV 域)。
// 形态适配:Stream → SpanReader;byte[] 惰性 Data → 首次调用物化并缓存;
// uint[] Palette → optional<vector<uint32>>;IsR8 的越界读(上游 EOS 异常逃
// 出 TryParse 的 catch 面)= 文件长度前置检查归 false。
// [UPSTREAM continued] the whole of R8Loader.cs L24-229, verbatim
// semantics. The Dune 2000 R8/R16 image: a frame header (a 1-byte type with
// leading zeros skipped) + four i32s (w/h/x/y) + imageHandle u32 +
// paletteHandle i32 + bpp u8 (8/16) + two u8s (frameW/frameH) + one
// alignment u8. bpp16 data is RGB555 expanded to BGRA32 (0xFF alpha with
// channel left-shifts); bpp8 is indexed. When type==1 and paletteHandle!=0 a
// 512-byte palette trails the frame (the same RGB555 expansion, index 0
// remapped to transparent); type==2 reuses the previous frame's palette.
// RemappableFrame expands the indexed data to BGRA on demand: the
// shroud-to-fog bitwise halving (~((c>>1)&0x7F7F7F)), index 1 → the shadow
// color (140<<24), and the PlayerColorRemap rebuild over the player range
// 240..255 (HSV domain). Shape adaptations: Stream → SpanReader; the lazy
// byte[] Data → materialized on first call then cached; uint[] Palette →
// optional<vector<uint32>>; IsR8's out-of-bounds read (upstream's EOS
// exception escaping the TryParse catch surface) = a length pre-check
// returning false.
#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "formats/span_reader.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// R8Loader.Frame(R8Loader.cs L101-181):单帧 + 尾随调色板。
/// R8Loader.Frame (R8Loader.cs L101-181): one frame + its trailing palette.
class R8Frame : public gfx::ISpriteFrame {
 public:
  /// Frame(s, lastPalette):type==2 帧借用 lastPalette(空指针 = 上游 null)。
  /// Frame(s, lastPalette): a type==2 frame borrows lastPalette (a null
  /// pointer = upstream's null).
  R8Frame(SpanReader& reader_source, const std::vector<std::uint32_t>* ptr_last_palette);

  gfx::SpriteFrameType Type() const override { return type_frame_; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return vec2_offset_; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return true; }

  /// 上游 uint[] Palette(null → nullopt)。
  /// Upstream's uint[] Palette (null → nullopt).
  const std::optional<std::vector<std::uint32_t>>& Palette() const { return opt_palette_; }

 private:
  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Indexed8;
  int2 int2_size_{};
  int2 int2_frame_size_{};
  core::Vector2 vec2_offset_{};
  std::vector<std::byte> vec_data_;
  std::optional<std::vector<std::uint32_t>> opt_palette_;
};

/// R8Loader.RemappableFrame(R8Loader.cs L26-99):索引帧的按需 BGRA 展开。
/// R8Loader.RemappableFrame (R8Loader.cs L26-99): the on-demand BGRA
/// expansion of an indexed frame.
class R8RemappableFrame : public gfx::ISpriteFrame {
 public:
  /// 上游默认参:useShadow = true / convertShroudToFog = false / remap =
  /// default(Color)(argb 0)。
  /// The upstream defaults: useShadow = true / convertShroudToFog = false /
  /// remap = default(Color) (argb 0).
  explicit R8RemappableFrame(R8Frame frame_inner, bool b_use_shadow = true,
                             bool b_convert_shroud_to_fog = false,
                             core::Color color_remap = core::Color{})
      : frame_inner_{std::move(frame_inner)},
        b_use_shadow_{b_use_shadow},
        b_convert_shroud_to_fog_{b_convert_shroud_to_fog},
        color_remap_{color_remap} {}

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Bgra32; }
  int2 Size() const override { return frame_inner_.Size(); }
  int2 FrameSize() const override { return frame_inner_.FrameSize(); }
  core::Vector2 Offset() const override { return frame_inner_.Offset(); }
  bool DisableExportPadding() const override { return frame_inner_.DisableExportPadding(); }
  std::span<const std::byte> Data() const override;  // 惰性物化缓存 | lazily materialized + cached

  /// WithSequenceFlags(L95-98)。
  R8RemappableFrame WithSequenceFlags(bool b_use_shadow, bool b_convert_shroud_to_fog,
                                      core::Color color_remap) const {
    return R8RemappableFrame{frame_inner_, b_use_shadow, b_convert_shroud_to_fog, color_remap};
  }

 private:
  R8Frame frame_inner_;
  bool b_use_shadow_;
  bool b_convert_shroud_to_fog_;
  core::Color color_remap_;
  mutable std::vector<std::byte> vec_data_cached_;
  mutable bool b_data_materialized_ = false;
};

/// IsR8(L183-200):首字节非零 + 字节 @25 ∈ {8,16}(文件不足 26 字节 = 上游
/// 越界读异常逃出 catch 面,归 false)。
/// IsR8 (L183-200): a nonzero first byte + byte @25 in {8,16} (a file
/// shorter than 26 bytes maps upstream's out-of-bounds read exception
/// escaping the catch surface to false).
bool IsR8(std::span<const std::byte> vec_file);

/// TryParseSprite(L202-228):探测 + 逐帧构造;带调色板的帧以
/// RemappableFrame(默认参)包装,其余直入。
/// TryParseSprite (L202-228): the probe + per-frame construction; frames
/// with palettes wrap as RemappableFrame (default arguments), the rest enter
/// directly.
bool TryParseR8(std::span<const std::byte> vec_file,
                std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}  // namespace ora::fmt
