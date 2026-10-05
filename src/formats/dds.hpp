// UPSTREAM: NONE —— Pfim v0.11.3(src/Pfim/dds/{Dds,DdsHeader,
// UncompressedDds,CompressedDds,Dxt1Dds,Dxt3Dds,Dxt5Dds,Bc5Dds}.cs)逐语义
// 移植;上游 OpenRA 经 OpenRA.Mods.Common/SpriteLoaders/DdsLoader.cs
// @7d57605 间接消费(IsDds 的魔数判定逐字取自 DdsLoader.cs L24-31)。
// DDS 子集:124 字节头 + 32 字节像素格式(ThreeCC 分派)、非压缩 RGB
// (8/16/24/32bpp + 位掩码 R/B 交换 + Rgba16 半字节交换 + mip 链)、
// DXT1/3/5 块解码(RGB565 浮点插值逐字 + 3 位 alpha 梯度)。Data = 全
// mip 链拼接缓冲(上游 DdsFrame.Data 同为全数组)。
// 偏离(D66):FourCC BC4/BC4s/BC5/BC5s/ATI1/ATI2/DX10(BC6h/BC7)未
// 移植 —— 显式抛 "FourCC: X not supported."(上游 BC5 系可解 Rgb24;
// mods 零 dds 资产,oracle 向量覆盖已移植路径)。缓冲流与 MemoryStream
// 快路径统一走内存路径(D65)。
// The DDS subset: the 124-byte header + the 32-byte pixel format (the
// FourCC dispatch), the uncompressed RGB paths (8/16/24/32bpp + the
// bitmask R/B swap + the Rgba16 nibble swap + the mip chain), and the
// DXT1/3/5 block decoders (the RGB565 float interpolation verbatim + the
// 3-bit alpha gradient). Data is the whole mip-chain buffer (upstream's
// DdsFrame.Data is the same full array). Deviation (D66): the BC4/BC4s/
// BC5/BC5s/ATI1/ATI2/DX10 (BC6h/BC7) FourCCs are not ported — an explicit
// "FourCC: X not supported." throw (upstream's BC5 family would decode to
// Rgb24; the mods carry zero dds assets; the oracle vectors cover the
// ported paths). Buffered/fast paths unified on memory (D65).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// DdsLoader.IsDds(DdsLoader.cs L24-31):首 u32 小端 == 0x20534444。
/// DdsLoader.IsDds (DdsLoader.cs L24-31): the leading little-endian u32
/// == 0x20534444.
bool IsDds(std::span<const std::byte> vec_file);

/// DdsLoader.TryParseSprite(L33-44):判定 + 解析一体。
/// DdsLoader.TryParseSprite (L33-44): probe + parse in one.
bool TryParseDds(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

/// DdsLoader.cs L47-81 的帧适配(DdsFrame):Size = mip0 尺寸,FrameSize
/// 直通,Offset 恒 0,Data = 全 mip 链缓冲;Pfim Rgba32/Rgb24 →
/// SpriteFrameType.Bgra32/Bgr24(通道序按位反排的注释语义),其余
/// ImageFormat 名逐字抛 "Unhandled ImageFormat {…}"。
/// The DdsLoader.cs L47-81 frame adapter (DdsFrame): Size is the mip0
/// dimension, FrameSize passes through, Offset stays zero, Data is the
/// whole mip-chain buffer; Pfim Rgba32/Rgb24 → SpriteFrameType.Bgra32/
/// Bgr24 (the reversed-channel-order comment semantics), any other
/// ImageFormat throws "Unhandled ImageFormat {…}" with the name verbatim.
class DdsFrame final : public gfx::ISpriteFrame {
 public:
  explicit DdsFrame(std::span<const std::byte> vec_file);

  gfx::SpriteFrameType Type() const override { return type_frame_; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_size_; }
  core::Vector2 Offset() const override { return core::Vector2{0.0f, 0.0f}; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

 private:
  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Bgra32;
  int2 int2_size_{};
  std::vector<std::byte> vec_data_;
};

/// DdsSprite(L75-80):单帧包装。| DdsSprite (L75-80): the single-frame
/// wrapper.
class DdsSprite {
 public:
  explicit DdsSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }

 private:
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}  // namespace ora::fmt
