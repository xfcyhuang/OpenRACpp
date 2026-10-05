// UPSTREAM: NONE —— Pfim v0.11.3(src/Pfim/targa/{Targa,TargaHeader,
// UncompressedTarga,CompressedTarga}.cs)逐语义移植;上游 OpenRA 经
// OpenRA.Mods.Common/SpriteLoaders/TgaLoader.cs @b6fc03f 间接消费(IsTga
// 的字节判定逐字取自 TgaLoader.cs L24-53)。
// 真彩 Targa 解码:18 字节头(图像类型/色图规格/原点/描述字节的 4-5 位
// 定向)、非压缩与 RLE 两解码器 × 四向原点(BottomLeft/BottomRight/
// TopRight 归一到自下而上填充,TopLeft 自上而下)、4 字节对齐 stride 的
// 行尾零衬垫、色图应用(RGB565/888/8888 查表展开)。Pfim 的 Allocator/
// Rent 形态 → vector;缓冲流与 MemoryStream 快路径在合法输入下输出一致,
// C++ 统一走内存路径(D65)。
// The true-color Targa decoder: the 18-byte header (image type / color-map
// spec / origin / the descriptor bits 4-5 for orientation), the
// uncompressed and RLE decoders × the four orientations (BottomLeft/
// BottomRight/TopRight normalize to bottom-up filling, TopLeft fills
// top-down), the 4-byte-aligned stride with zero row padding, and the
// color-map application (RGB565/888/8888 table expansion). Pfim's
// Allocator/Rent shape → vector; the buffered and MemoryStream fast paths
// produce identical output on valid input — C++ unifies on the memory path
// (D65).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "formats/span_reader.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// TgaLoader.IsTga(TgaLoader.cs L24-53):真彩无色图三重判定(逐字节:
/// 跳过 idLength;colorMapType==0;imageType==2;色图 origin+length 合读
/// u32 == 0;colorMapDepth == 0)。
/// TgaLoader.IsTga (TgaLoader.cs L24-53): the three true-color-no-colormap
/// probes (byte for byte: skip idLength; colorMapType==0; imageType==2;
/// the color-map origin+length combined u32 == 0; colorMapDepth == 0).
bool IsTga(std::span<const std::byte> vec_file);

/// TgaLoader.TryParseSprite(L55-66):判定 + 解析一体。
/// TgaLoader.TryParseSprite (L55-66): probe + parse in one.
bool TryParseTga(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

/// TgaLoader.cs L69-116 的帧适配(TgaFrame):帧数据 = Pfim 解码输出整
/// 缓冲(含 stride 衬垫,与上游 tga.Data 全数组一致);32bpp → Bgra32、
/// 24bpp → Bgr24,其余 Pfim ImageFormat 名逐字抛
/// "Unhandled ImageFormat {Rgb8|R5g5b5}"。裁剪构造(ShpRemastered 用)
/// 按上游公式给 FrameSize/Offset。
/// The TgaLoader.cs L69-116 frame adapter (TgaFrame): frame data is the
/// whole Pfim decode buffer (stride padding included, matching upstream's
/// tga.Data full array); 32bpp → Bgra32, 24bpp → Bgr24, any other Pfim
/// ImageFormat throws "Unhandled ImageFormat {Rgb8|R5g5b5}" with the name
/// verbatim. The cropping constructor (for ShpRemastered) sets
/// FrameSize/Offset per the upstream formula.
class TgaFrame final : public gfx::ISpriteFrame {
 public:
  /// 空白帧(ShpRemastered 缺号占位;Data 空,尺寸 0)。
  /// The blank frame (ShpRemastered's gap filler; empty Data, zero size).
  TgaFrame() = default;

  /// TgaFrame(Stream)(L85-99):整帧解码。
  /// TgaFrame(Stream) (L85-99): the full-frame decode.
  explicit TgaFrame(std::span<const std::byte> vec_file);

  /// TgaFrame(Stream, frameSize, frameWindow)(L101-106):解码 + 画框/偏移
  /// 覆盖(Offset = 0.5 ×(left+right−FrameSize.Width, top+bottom−
  /// FrameSize.Height))。
  /// TgaFrame(Stream, frameSize, frameWindow) (L101-106): decode + the
  /// canvas/offset override (Offset = 0.5 × (left+right−FrameSize.Width,
  /// top+bottom−FrameSize.Height)).
  TgaFrame(std::span<const std::byte> vec_file, int2 int2_frame_size, Rectangle rect_frame_window);

  gfx::SpriteFrameType Type() const override { return type_frame_; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return vector2_offset_; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

 private:
  // C# 自动属性未赋值 = default(SpriteFrameType) = 首枚举 Indexed8
  // (空白帧的 Type 实际取值;仅 ShpRemastered 缺号帧可见)。
  // The C# auto-property stays unassigned = default(SpriteFrameType) =
  // the first member Indexed8 (observable only on ShpRemastered's blank
  // gap frames).
  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Indexed8;
  int2 int2_size_{};
  int2 int2_frame_size_{};
  core::Vector2 vector2_offset_{};
  std::vector<std::byte> vec_data_;
};

/// TgaSprite(L109-115):单帧包装。
/// TgaSprite (L109-115): the single-frame wrapper.
class TgaSprite {
 public:
  explicit TgaSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }

 private:
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}  // namespace ora::fmt
