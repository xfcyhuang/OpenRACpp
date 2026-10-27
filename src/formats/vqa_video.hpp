#pragma once
import std;

#include "core/int2.hpp"
#include "formats/span_reader.hpp"
#include "formats/video.hpp"

namespace ora::fmt {

class VqaLoader : public IVideoLoader {
 public:
  bool TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                     std::unique_ptr<IVideo>& out_video) const override;
};

bool IsWestwoodVqa(std::span<const std::byte> vec_file);

class VqaVideo : public IVideo {
 public:

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

}
