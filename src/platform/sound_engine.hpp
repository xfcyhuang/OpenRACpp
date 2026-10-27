#pragma once
import std;

#include "core/wpos.hpp"

namespace ora::platform {

struct SoundDevice {
  std::optional<std::string> str_device;
  std::string str_label;
};

class IPcmStream {
 public:
  virtual ~IPcmStream() = default;

  virtual std::optional<std::size_t> Length() const = 0;

  virtual std::size_t Read(std::span<std::uint8_t> span_dst) = 0;
};

class ISoundSource {
 public:
  virtual ~ISoundSource() = default;
};

class ISound {
 public:
  virtual ~ISound() = default;
  virtual float GetVolume() const = 0;
  virtual void SetVolume(float fp4_volume) = 0;
  [[nodiscard]] virtual float SeekPosition() const = 0;
  [[nodiscard]] virtual bool Complete() const = 0;
  virtual void SetPosition(WPos pos_position) = 0;
};

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

}
