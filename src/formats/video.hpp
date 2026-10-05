// UPSTREAM: OpenRA.Game/Graphics/Video.cs @7d57605 L14-36(IVideo)+
// VideoLoader.cs L16-31(IVideoLoader/GetVideo 链)。
// 视频契约:逐帧 BGRA 帧数据 + 帧推进/复位 + 可选音频(22050Hz 16bit
// PCM,由具体格式解码为非交错字节流)。C# 属性面映射为 const 方法面;
// CurrentFrameData/AudioData 的 byte[] 返回改为 span(零拷贝直传,项目
// 集合参数优化主轴);AudioData 的上游 null(WsaVideo)以空 span 表示。
// [UPSTREAM continued] Video.cs L14-36 (IVideo) plus VideoLoader.cs L16-31
// (the IVideoLoader/GetVideo chain). The video contract: per-frame BGRA
// frame data with frame advance/reset plus optional audio (22050Hz 16bit
// PCM decoded by each format into a non-interleaved byte stream). The C#
// property surface maps to a const method surface; CurrentFrameData/
// AudioData return spans instead of byte[] (zero-copy direct passing, the
// project's collection-parameter optimization axis); upstream's null
// AudioData (WsaVideo) becomes an empty span.
#pragma once
import std;

namespace ora::fmt {

/// Video.cs L14-35:IVideo 的 13 个成员。
/// Video.cs L14-35: the 13 IVideo members.
struct IVideo {
  virtual ~IVideo() = default;

  virtual std::uint16_t FrameCount() const = 0;
  virtual std::uint8_t Framerate() const = 0;
  virtual std::uint16_t Width() const = 0;
  virtual std::uint16_t Height() const = 0;

  /// 当前帧的 32 位 BGRA 数据。| The current frame in 32-bit BGRA.
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

/// VideoLoader.cs L16-19:嗅探 + 构造一体(上游 TryParseVideo 的 catch 面
/// 由各实现持有)。
/// VideoLoader.cs L16-19: sniff plus construct in one (each implementation
/// carries upstream TryParseVideo's catch surface).
struct IVideoLoader {
  virtual ~IVideoLoader() = default;
  virtual bool TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                             std::unique_ptr<IVideo>& out_video) const = 0;
};

/// VideoLoader.GetVideo(VideoLoader.cs L22-30):按链序首个命中者;全不
/// 命中返回 false(上游返回 null)。
/// VideoLoader.GetVideo (VideoLoader.cs L22-30): the first hit in chain
/// order; false when none match (upstream returns null).
inline bool GetVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                     std::span<const IVideoLoader* const> vec_loaders, std::unique_ptr<IVideo>& out_video) {
  for (const auto* loader : vec_loaders)
    if (loader->TryParseVideo(vec_file, b_use_frame_padding, out_video))
      return true;

  return false;
}

}  // namespace ora::fmt
