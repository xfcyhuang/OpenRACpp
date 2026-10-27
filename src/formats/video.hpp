#pragma once
import std;

namespace ora::fmt {

struct IVideo {
  virtual ~IVideo() = default;

  virtual std::uint16_t FrameCount() const = 0;
  virtual std::uint8_t Framerate() const = 0;
  virtual std::uint16_t Width() const = 0;
  virtual std::uint16_t Height() const = 0;

  virtual std::span<const std::byte> CurrentFrameData() const = 0;
  virtual std::int32_t CurrentFrameIndex() const = 0;
  virtual void AdvanceFrame() = 0;

  virtual bool HasAudio() const = 0;
  virtual std::span<const std::byte> AudioData() const = 0;
  virtual std::int32_t AudioChannels() const = 0;
  virtual std::int32_t SampleBits() const = 0;
  virtual std::int32_t SampleRate() const = 0;

  virtual void Reset() = 0;
};

struct IVideoLoader {
  virtual ~IVideoLoader() = default;
  virtual bool TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                             std::unique_ptr<IVideo>& out_video) const = 0;
};

inline bool GetVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                     std::span<const IVideoLoader* const> vec_loaders, std::unique_ptr<IVideo>& out_video) {
  for (const auto* loader : vec_loaders)
    if (loader->TryParseVideo(vec_file, b_use_frame_padding, out_video))
      return true;

  return false;
}

}
