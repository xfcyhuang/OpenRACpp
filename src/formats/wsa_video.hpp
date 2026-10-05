// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/WsaVideo.cs @7d57605 L18-156(全文)
// + OpenRA.Mods.Cnc/VideoLoaders/WsaLoader.cs L18-70(IsWsa 嗅探与
// TryParseVideo 面)。
// Westwood WSA 动画:10 字节头(frames/x/y/width/height/delta/flags)+
// frames+2 个 u32 绝对偏移(flags==1 时先 768 字节调色板且偏移 +768;
// 调色板 <<2 后高位复制到低两位)。每帧 = Format80(LCW)解出中间帧,
// 再 Format40(XOR delta)作用于上一帧索引图,末按 totalFrameWidth 行
// 距展开 BGRA。Framerate 恒 15;无音频。
// 形态适配(D79):Stream → SpanReader;LoadFrame 每帧的 new byte[]
// (intermediate/current)以成员 vector 每帧重置零代替(字节面等价);
// flags≠1 时上游 paletteBytes 为 null → 展开处 NRE 的等价抛;IsWsa 的
// `width <= 0` 对 ushort 恒假(上游怪癖照抄)。
// [UPSTREAM continued] the whole of WsaVideo.cs plus the WsaLoader.cs L18-70
// sniff and TryParseVideo surface. Westwood WSA animation: a 10-byte header
// (frames/x/y/width/height/delta/flags) plus frames+2 absolute u32
// offsets (with flags==1 reading a 768-byte palette first and adding 768
// to the offsets; the palette shifts left by 2 then copies its high bits
// into the low two). Each frame = Format80 (LCW) decoded into an
// intermediate, then Format40 (the XOR delta) applied over the previous
// frame's index map, finally expanded to BGRA on the totalFrameWidth row
// stride. Framerate is a constant 15; no audio. Shape adaptation (D79):
// Stream → SpanReader; LoadFrame's per-frame new byte[]s (intermediate/
// current) become member vectors reset to zero each frame (byte-equal);
// with flags≠1 upstream's paletteBytes is null → the NRE's equivalent
// throw at the expansion; IsWsa's `width <= 0` is always false for a
// ushort (the upstream quirk kept).
#pragma once
import std;

#include "formats/span_reader.hpp"
#include "formats/video.hpp"

namespace ora::fmt {

/// WsaLoader.cs L18-32。
class WsaLoader : public IVideoLoader {
 public:
  bool TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                     std::unique_ptr<IVideo>& out_video) const override;
};

/// WsaLoader.IsWsa(L34-68):frames>1 + 偏移表全读 + flags==1 吞 768 调色
/// 板 + 长度核对(Length == offsets[^1])。
/// WsaLoader.IsWsa (L34-68): frames>1 + the offset table read whole + a
/// flags==1 swallowing the 768-byte palette + the length check (Length ==
/// offsets[^1]).
bool IsWsa(std::span<const std::byte> vec_file);

/// WsaVideo(全文逐语义)。构造 = 解析头 + 首帧解码(Reset)。
/// WsaVideo (verbatim semantics throughout). Construction = header parse +
/// the first frame decoded (Reset).
class WsaVideo : public IVideo {
 public:
  /// L42-98。useFramePadding:帧数据升到 NextPowerOf2(max(W,H)) 方形;
  /// 否则 Width×H。
  /// L42-98. useFramePadding: the frame data grows to a
  /// NextPowerOf2(max(W,H)) square; else W×H.
  WsaVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding);

  std::uint16_t FrameCount() const override { return uint2_frame_count_; }
  std::uint8_t Framerate() const override { return 15; }
  std::uint16_t Width() const override { return uint2_width_; }
  std::uint16_t Height() const override { return uint2_height_; }

  std::span<const std::byte> CurrentFrameData() const override { return vec_current_frame_data_; }
  std::int32_t CurrentFrameIndex() const override { return int4_current_frame_index_; }
  void AdvanceFrame() override;

  bool HasAudio() const override { return false; }
  std::span<const std::byte> AudioData() const override { return {}; }
  std::int32_t AudioChannels() const override { return 0; }
  std::int32_t SampleBits() const override { return 0; }
  std::int32_t SampleRate() const override { return 0; }

  void Reset() override;

 private:
  void LoadFrame();

  std::span<const std::byte> vec_file_;
  std::uint16_t uint2_frame_count_ = 0;
  std::uint16_t uint2_width_ = 0;
  std::uint16_t uint2_height_ = 0;

  std::vector<std::byte> vec_current_frame_data_;
  std::int32_t int4_current_frame_index_ = 0;

  std::vector<std::byte> vec_palette_bytes_;
  std::vector<std::uint32_t> vec_frame_offsets_;
  std::uint16_t uint2_total_frame_width_ = 0;

  // LoadFrame 的每帧重建量(上游 new byte[];previous=null 以
  // b_has_previous_ 表达)。| LoadFrame's per-frame rebuilds (upstream's
  // new byte[]s; previous=null expressed by b_has_previous_).
  std::vector<std::byte> vec_previous_indices_;
  std::vector<std::byte> vec_current_indices_;
  bool b_has_previous_ = false;
};

}  // namespace ora::fmt
