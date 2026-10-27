#pragma once
import std;

#include "platform/al_loader.hpp"
#include "platform/sound_engine.hpp"

namespace ora::platform {

class OpenAlSoundEngine;

inline constexpr int kAlMaxInstancesPerFrame = 3;
inline constexpr int kAlGroupDistance = 2730;
inline constexpr std::int64_t kAlGroupDistanceSqr =
    static_cast<std::int64_t>(kAlGroupDistance) * kAlGroupDistance;
inline constexpr int kAlPoolSize = 256;

ALenum MakeAlFormat(int int4_channels, int int4_sample_bits);

std::vector<std::string> ParseAlDeviceList(const char* ptr_devices);

struct GateProbeSlot {
  bool b_is_active = false;
  bool b_is_relative = false;
  std::uintptr_t uintp_source_id = 0;
  std::int32_t int4_frame_started = 0;
  WPos pos{};
};

struct PlayGateResult {
  bool b_deny = false;
  float fp4_atten = 1.0f;
};

PlayGateResult EvaluatePlayGate(std::span<const GateProbeSlot> span_slots, bool b_relative,
                                 std::uintptr_t uintp_source_id, WPos pos_position,
                                 std::int32_t int4_curr_frame);

class OpenAlSoundSource final : public ISoundSource {
 public:
  OpenAlSoundSource(std::span<const std::uint8_t> span_data, int int4_channels,
                    int int4_sample_bits, int int4_sample_rate);
  ~OpenAlSoundSource() override;

  OpenAlSoundSource(const OpenAlSoundSource&) = delete;
  OpenAlSoundSource& operator=(const OpenAlSoundSource&) = delete;

  [[nodiscard]] ALuint Buffer() const { return uint4_buffer_; }
  [[nodiscard]] int SampleRate() const { return int4_sample_rate_; }

 private:
  ALuint uint4_buffer_ = 0;
  bool b_disposed_ = false;
  int int4_sample_rate_ = 0;
};

class OpenAlSound : public ISound {
 public:
  friend class OpenAlSoundEngine;

  OpenAlSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping, bool b_relative,
              WPos pos_position, float fp4_volume, int int4_sample_rate, ALuint uint4_buffer);

  ~OpenAlSound() override;

  float GetVolume() const override;
  void SetVolume(float fp4_volume) override;
  [[nodiscard]] float SeekPosition() const override;
  [[nodiscard]] virtual bool Complete() const override;
  virtual void Stop();
  void SetPosition(WPos pos_position) override;
  void SetLooping(bool b_looping);

  [[nodiscard]] ALuint Source() const { return uint4_source_; }
  void UnbindSource();

 protected:

  OpenAlSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping, bool b_relative,
              WPos pos_position, float fp4_volume, int int4_sample_rate);
  void StopSource();

  OpenAlSoundEngine* ptr_engine_;
  ALuint uint4_source_;
  float fp4_sample_rate_;
  bool b_done_ = false;
};

class OpenAlAsyncLoadSound final : public OpenAlSound {
 public:
  OpenAlAsyncLoadSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping,
                       bool b_relative, WPos pos_position, float fp4_volume, int int4_channels,
                       int int4_sample_bits, int int4_sample_rate,
                       std::unique_ptr<IPcmStream>&& up_stream);
  ~OpenAlAsyncLoadSound() override;

  void Stop() override;
  [[nodiscard]] bool Complete() const override;

 private:
  void RunPlayTask();

  std::jthread thread_play_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::atomic<bool> b_cancel_{false};
  std::atomic<bool> b_finished_{false};
};

class OpenAlSoundEngine final : public ISoundEngine {
 public:

  explicit OpenAlSoundEngine(std::optional<std::string> str_device_name);
  ~OpenAlSoundEngine() override;

  OpenAlSoundEngine(const OpenAlSoundEngine&) = delete;
  OpenAlSoundEngine& operator=(const OpenAlSoundEngine&) = delete;

  [[nodiscard]] std::vector<SoundDevice> AvailableDevices() override;
  [[nodiscard]] bool Dummy() const override { return false; }
  [[nodiscard]] float GetVolume() const override { return fp4_volume_; }
  void SetVolume(float fp4_volume) override;

  ISoundSource* AddSoundSourceFromMemory(std::span<const std::uint8_t> span_data,
                                         int int4_channels, int int4_sample_bits,
                                         int int4_sample_rate) override;
  std::unique_ptr<ISound> Play2D(ISoundSource* ptr_source, bool b_loop, bool b_relative,
                                 WPos pos_position, float fp4_volume,
                                 bool b_attenuate_volume) override;
  std::unique_ptr<ISound> Play2DStream(std::unique_ptr<IPcmStream>&& up_stream, int int4_channels,
                                       int int4_sample_bits, int int4_sample_rate, bool b_loop,
                                       bool b_relative, WPos pos_position,
                                       float fp4_volume) override;

  void PauseSound(ISound* ptr_sound, bool b_paused) override;
  void StopSound(ISound* ptr_sound) override;
  void SetAllSoundsPaused(bool b_paused) override;
  void StopAllSounds() override;
  void SetListenerPosition(WPos pos_position) override;
  void SetSoundVolume(float fp4_volume, ISound* ptr_music, ISound* ptr_video) override;
  void SetSoundLooping(bool b_looping, ISound* ptr_sound) override;
  void SetSoundPosition(ISound* ptr_sound, WPos pos_position) override;

  void SetLocalTick(std::int32_t int4_tick) { int4_local_tick_ = int4_tick; }

 private:
  friend class OpenAlSound;
  friend class OpenAlAsyncLoadSound;

  struct PoolSlot {
    bool b_is_active = false;
    std::int32_t int4_frame_started = 0;
    WPos pos{};
    bool b_is_relative = false;
    OpenAlSoundSource* ptr_sound_source = nullptr;
    OpenAlSound* ptr_sound = nullptr;
    ALuint uint4_source = 0;
  };

  bool TryGetSourceFromPool(ALuint& uint4_source);
  static void PauseSource(ALuint uint4_source, bool b_paused);

  void ForgetSound(OpenAlSound* ptr_sound);

  std::vector<PoolSlot> vec_slots_;
  float fp4_volume_ = 1.0f;
  ALCdevice* ptr_device_ = nullptr;
  ALCcontext* ptr_context_ = nullptr;
  std::vector<OpenAlSound*> vec_live_sounds_;
  std::int32_t int4_local_tick_ = 0;
};

}
