#pragma once
import std;

#include "formats/span_reader.hpp"
#include "formats/video.hpp"

namespace ora::fmt {

class WsaLoader : public IVideoLoader {
 public:
  bool TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                     std::unique_ptr<IVideo>& out_video) const override;
};

bool IsWsa(std::span<const std::byte> vec_file);

class WsaVideo : public IVideo {
 public:

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

  std::vector<std::byte> vec_previous_indices_;
  std::vector<std::byte> vec_current_indices_;
  bool b_has_previous_ = false;
};

}
