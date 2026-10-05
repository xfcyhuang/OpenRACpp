// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VqaVideo.cs @b6fc03f L19-545(全文)
// + OpenRA.Mods.Cnc/VideoLoaders/VqaLoader.cs L18-61(IsWestwoodVqa 嗅探
// 与 TryParseVideo 面)。
// Westwood VQA 视频:FORM/WVQA/VQHD 头 + FINF 帧偏移表(0x40000000 标志
// 位与 <<1);帧体 = VQFR 子块串(CPL0 调色板、CBFZ/CBF0 全量码本、
// CBP0/CBPZ 分块码本(chunkBufferParts 块拼齐后于"下一帧"应用 —— 非
// 压缩 Clone / 压缩 LCW 解入)、VPTZ/VPRZ/VPTR 帧指令(LCW / 反向 LCW /
// 裸拷贝);非 HQ 8 位调色板展开((mod==0x0f) ? px : cbf[(mod*256+px)*8
// +…])与 HQ 16 位码本(RGB555 拆 3 字节)两条 DecodeFrameData 路径;
// 音频 = SND0/SND2 块收集(SND2 走 IMA ADPCM,立体声左右道交错,奇长
// 度 +=2 怪癖照抄)。偶字节对齐的 `Peek() == 0` 消费依赖 StreamExts.Peek
// 的 EOF -1 语义。
// 形态适配(D78):Stream → SpanReader(构造期物化全文件 span);cbf 字
// 段在上游被多次重指(ctor 数组 / cbp.Clone / cbfBuffer / CBF0 新读),
// C++ 以 heap 存储 + span 别名等价复刻(CBF0 照上游 ReadBytes 拷入 heap
// —— 后续 CBFZ 的 Array.Clear(cbf) 会经别名写它,不可零拷贝指源);
// LoadFrame 尾的 IndexOutOfRange / 参数-less NotSupportedException /
// InvalidDataException 以带类型前缀的逐字消息等价抛,登记于 COVERAGE。
// [UPSTREAM continued] the whole of VqaVideo.cs plus the VqaLoader.cs L18-61
// sniff and TryParseVideo surface. Westwood VQA video: the FORM/WVQA/VQHD
// header and the FINF frame-offset table (the 0x40000000 flag bit and the
// <<1); a frame's body is a VQFR subchunk stream (the CPL0 palette, the
// CBFZ/CBF0 full codebooks, the CBP0/CBPZ partial codebooks assembled over
// chunkBufferParts chunks and applied "the frame after" — an uncompressed
// Clone or an LCW decode into cbf), and the VPTZ/VPRZ/VPTR frame
// instructions (LCW / reverse LCW / a bare copy); two DecodeFrameData
// paths, the non-HQ 8-bit palette expansion ((mod==0x0f) ? px : cbf
// [(mod*256+px)*8+…]) and the HQ 16-bit codebook (RGB555 split into 3
// bytes); audio collected from SND0/SND2 chunks (SND2 runs IMA ADPCM,
// stereo interleaves left/right, the odd-length +=2 quirk kept). The
// even-alignment `Peek() == 0` consume relies on StreamExts.Peek's EOF -1.
// Shape adaptation (D78): Stream → SpanReader (the whole file
// materialized as a span at construction); upstream's cbf field is
// re-pointed several times (the ctor array / cbp.Clone / cbfBuffer / a
// fresh CBF0 read), reproduced equivalently as heap storage plus a span
// alias (CBF0 copies into the heap like upstream's ReadBytes — the later
// CBFZ Array.Clear(cbf) writes through the alias, so a zero-copy source
// alias is impossible); the
// IndexOutOfRange/parameterless NotSupportedException/InvalidDataException
// throws around LoadFrame keep verbatim or equivalent messages, registered
// in COVERAGE.
#pragma once
import std;

#include "core/int2.hpp"
#include "formats/span_reader.hpp"
#include "formats/video.hpp"

namespace ora::fmt {

/// VqaLoader.cs L18-32:嗅探 + 构造一体(上游 catch 面无 —— TryParseVideo
/// 不套 try;构造抛点直接向上)。
/// VqaLoader.cs L18-32: sniff plus construct in one (no catch surface —
/// TryParseVideo wraps nothing; construction throws propagate).
class VqaLoader : public IVideoLoader {
 public:
  bool TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                     std::unique_ptr<IVideo>& out_video) const override;
};

/// VqaLoader.IsWestwoodVqa(L34-59):FORM + 非零长度 + WVQA;失败回卷(此
/// 处输入本自 0 起,回卷为形式照抄)。短文件 = 上游 ReadASCII 抛
/// EndOfStream,IsWestwoodVqa 不捕获 —— 由 TryParseVideo 的调用方(上游
/// 无 catch,GetVideo 亦不套)上抛;嗅探面按"抛 = 非命中"由测试约定。
/// VqaLoader.IsWestwoodVqa (L34-59): FORM + a nonzero length + WVQA; a
/// failure rewinds (the input here starts at 0 anyway, kept for shape).
/// A short file makes upstream's ReadASCII throw EndOfStream, which
/// IsWestwoodVqa does not catch — and neither TryParseVideo nor GetVideo
/// wraps it; the sniff surface treats throw = no-hit by test convention.
bool IsWestwoodVqa(std::span<const std::byte> vec_file);

/// VqaVideo(全文逐语义)。构造 = 解析头 + CollectAudioData + 首帧解码
/// (Reset)。
/// VqaVideo (verbatim semantics throughout). Construction = header parse +
/// CollectAudioData + the first frame decoded (Reset).
class VqaVideo : public IVideo {
 public:
  /// L64-162。useFramePadding:帧数据升到 NextPowerOf2(max(W,H)) 方形
  /// (totalFrameWidth 随之);否则 Width×H。
  /// L64-162. useFramePadding: the frame data grows to a
  /// NextPowerOf2(max(W,H)) square (totalFrameWidth follows); else W×H.
  VqaVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding);

  std::uint16_t FrameCount() const override { return uint2_frame_count_; }
  std::uint8_t Framerate() const override { return uint1_framerate_; }
  std::uint16_t Width() const override { return uint2_width_; }
  std::uint16_t Height() const override { return uint2_height_; }

  std::span<const std::byte> CurrentFrameData() const override { return vec_current_frame_data_; }
  std::int32_t CurrentFrameIndex() const override { return int4_current_frame_index_; }
  void AdvanceFrame() override;

  bool HasAudio() const override { return b_has_audio_; }
  std::span<const std::byte> AudioData() const override { return vec_audio_data_; }
  std::int32_t AudioChannels() const override { return int4_audio_channels_; }
  std::int32_t SampleBits() const override { return int4_sample_bits_; }
  std::int32_t SampleRate() const override { return int4_sample_rate_; }

  void Reset() override;

  /// L326-431,public:VQFR 子块解码(VQFL 父块在 CBFZ 后提前返回)。
  /// L326-431, public: the VQFR subchunk decode (a VQFL parent returns
  /// early after CBFZ).
  void DecodeVQFR(SpanReader& reader, std::string_view str_parent_type = "VQFR");

 private:
  void CollectAudioData();
  void LoadFrame();
  void DecodeFrameData();
  void WriteBlock(int int4_block_number, int int4_count, int& int4_x, int& int4_y);
  bool IsHqVqa() const { return (uint4_video_flags_ & 0x10) == 16; }

  std::span<const std::byte> vec_file_;
  std::uint16_t uint2_frame_count_ = 0;
  std::uint8_t uint1_framerate_ = 0;
  std::uint16_t uint2_width_ = 0;
  std::uint16_t uint2_height_ = 0;

  std::vector<std::byte> vec_current_frame_data_;
  std::int32_t int4_current_frame_index_ = 0;

  bool b_has_audio_ = false;
  std::vector<std::byte> vec_audio_data_;
  std::int32_t int4_audio_channels_ = 0;
  std::int32_t int4_sample_bits_ = 0;
  std::int32_t int4_sample_rate_ = 0;

  std::uint16_t uint2_num_colors_ = 0;
  std::uint16_t uint2_block_width_ = 0;
  std::uint16_t uint2_block_height_ = 0;
  std::uint8_t uint1_chunk_buffer_parts_ = 0;
  int2 blocks_{};
  std::vector<std::uint32_t> vec_offsets_;
  std::vector<std::byte> vec_palette_bytes_;
  std::uint32_t uint4_video_flags_ = 0;
  std::uint16_t uint2_total_frame_width_ = 0;

  // cbf 的可重指存储(L47-50 的 cbf/cbp/cbfBuffer 三数组;cbf 依次可能
  // 指 ctor 存储、cbp 克隆、cbfBuffer、CBF0 源切片)。
  // The re-pointable cbf storage (L47-50's cbf/cbp/cbfBuffer; cbf may in
  // turn point at the ctor storage, the cbp clone, cbfBuffer, or a CBF0
  // slice of the source).
  std::vector<std::byte> vec_cbf_heap_;
  std::span<std::byte> vec_cbf_{};
  std::vector<std::byte> vec_cbp_;
  std::vector<std::byte> vec_cbf_buffer_;
  bool b_cbp_is_compressed_ = false;

  std::vector<std::byte> vec_file_buffer_;
  static constexpr std::int32_t kMaxCbfzSize = 256000;
  std::int32_t int4_vtpr_size_ = 0;
  std::int32_t int4_current_chunk_buffer_ = 0;
  std::int32_t int4_chunk_buffer_offset_ = 0;

  std::vector<std::byte> vec_orig_data_;
};

}  // namespace ora::fmt
