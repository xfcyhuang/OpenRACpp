// UPSTREAM: OpenRA.Game/Sound/SoundDevice.cs @b6fc03f L20-46(ISoundEngine/
// ISoundSource/ISound 接口族与 SoundDevice 记录;文件另含 ISoundLoader/
// ISoundFormat/SoundType,归 Sound 门面批次 —— Phase 6)
// 接口族的 C++ 形态映射:byte[] 参数 → std::span 直传(README 有意优化表);
// Stream(Play2DStream)→ IPcmStream 最小顺序读面;float.NaN 的 Volume 读值
// 语义保留(解绑源后的哨兵值,消费端 Sound.cs 不读该值)。
// The C++ shape of the interface family: byte[] parameters pass as
// std::span (the README deliberate-optimization table); the Stream of
// Play2DStream becomes the minimal sequential-read IPcmStream; the
// float.NaN Volume read of a sound whose source was unbound keeps verbatim
// (a sentinel the Sound.cs consumer never reads).
#pragma once
import std;

#include "core/wpos.hpp"

namespace ora::platform {

/// 音频输出设备(SoundDevice.cs L44 的 record:Device = 传给 alcOpenDevice 的
/// 名字,null = 默认设备;Label = UI 展示名)。
/// An audio output device (the record of SoundDevice.cs L44: Device is the
/// name passed to alcOpenDevice, null meaning the default device; Label is
/// the UI-facing name).
struct SoundDevice {
  std::optional<std::string> str_device;
  std::string str_label;
};

/// 顺序字节流的最小面(Play2DStream 的 Stream 参数;调用即转移所有权,读完由
/// 引擎释放 —— 上游 `using (stream)` 的等价物):Length 返回 std::nullopt 等价
/// 上游 NotSupportedException 的 MemoryStream 回退分支
/// (OpenAlSoundEngine.cs L566-574)。
/// The minimal sequential-byte-stream face (the Stream parameter of
/// Play2DStream; ownership transfers on the call and the engine releases it
/// when done — the equivalent of upstream's `using (stream)`): a Length of
/// std::nullopt is the C++ equivalent of upstream's NotSupportedException
/// fallback branch (OpenAlSoundEngine.cs L566-574).
class IPcmStream {
 public:
  virtual ~IPcmStream() = default;

  /// 已知总长(未知返回 nullopt → 后台收集走增长缓冲)。
  /// Total length when known (nullopt → the background collector grows its
  /// buffer).
  virtual std::optional<std::size_t> Length() const = 0;

  /// 顺序读;返回写入 dst 的字节数(0 = 流尽)。
  /// Sequential read; returns the byte count written into dst (0 = end).
  virtual std::size_t Read(std::span<std::uint8_t> span_dst) = 0;
};

/// 已上传缓冲(OpenAlSoundSource 的接口面;Buffer/SampleRate 为引擎内部细节,
/// 归具体实现)。
/// An uploaded buffer (the interface face of OpenAlSoundSource; Buffer/
/// SampleRate are engine-internal details of the concrete implementation).
class ISoundSource {
 public:
  virtual ~ISoundSource() = default;
};

/// 一次发声(OpenAlSound 的接口面)。Volume 读值在源解绑后为 NaN(上游语义)。
/// One playing sound (the interface face of OpenAlSound). Volume reads NaN
/// once the source has been unbound (upstream semantics).
class ISound {
 public:
  virtual ~ISound() = default;
  virtual float GetVolume() const = 0;
  virtual void SetVolume(float fp4_volume) = 0;
  [[nodiscard]] virtual float SeekPosition() const = 0;
  [[nodiscard]] virtual bool Complete() const = 0;
  virtual void SetPosition(WPos pos_position) = 0;
};

/// 声音引擎(ISoundEngine,SoundDevice.cs L26-40)。生命周期:实现类的析构
/// 即上游 Dispose(StopAllSounds + 源删除 + 上下文/设备销毁)。发声对象
/// (Play2D/Play2DStream 返回)为调用方所有,引擎析构须先于其使用结束
/// (上游 GC 生命期的 C++ 对应约定)。
/// The sound engine (ISoundEngine, SoundDevice.cs L26-40). Lifetime: the
/// implementor's destructor is upstream's Dispose (StopAllSounds + source
/// deletion + context/device teardown). Playing sounds (returned by Play2D/
/// Play2DStream) are caller-owned, and the engine must outlive their use (the
/// C++ counterpart of upstream's GC lifetimes).
class ISoundEngine {
 public:
  virtual ~ISoundEngine() = default;

  [[nodiscard]] virtual std::vector<SoundDevice> AvailableDevices() = 0;
  [[nodiscard]] virtual bool Dummy() const = 0;
  [[nodiscard]] virtual float GetVolume() const = 0;
  virtual void SetVolume(float fp4_volume) = 0;

  [[nodiscard]] virtual ISoundSource* AddSoundSourceFromMemory(
      std::span<const std::uint8_t> span_data, int int4_channels, int int4_sample_bits,
      int int4_sample_rate) = 0;
  [[nodiscard]] virtual std::unique_ptr<ISound> Play2D(ISoundSource* ptr_source, bool b_loop,
                                                       bool b_relative, WPos pos_position,
                                                       float fp4_volume,
                                                       bool b_attenuate_volume) = 0;
  [[nodiscard]] virtual std::unique_ptr<ISound> Play2DStream(
      std::unique_ptr<IPcmStream>&& up_stream, int int4_channels, int int4_sample_bits,
      int int4_sample_rate, bool b_loop, bool b_relative, WPos pos_position,
      float fp4_volume) = 0;

  virtual void PauseSound(ISound* ptr_sound, bool b_paused) = 0;
  virtual void StopSound(ISound* ptr_sound) = 0;
  virtual void SetAllSoundsPaused(bool b_paused) = 0;
  virtual void StopAllSounds() = 0;
  virtual void SetListenerPosition(WPos pos_position) = 0;
  virtual void SetSoundVolume(float fp4_volume, ISound* ptr_music, ISound* ptr_video) = 0;
  virtual void SetSoundLooping(bool b_looping, ISound* ptr_sound) = 0;
  virtual void SetSoundPosition(ISound* ptr_sound, WPos pos_position) = 0;
};

}  // namespace ora::platform
