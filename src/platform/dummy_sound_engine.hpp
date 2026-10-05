// UPSTREAM: OpenRA.Platforms.Default/DummySoundEngine.cs @b6fc03f L16-105(全文:
// DummySoundEngine/NullSoundSource/NullSound;DefaultPlatform.CreateSound 的
// OpenAL 初始化失败回落目标)
// 空实现逐语义移植(NullSound.Complete 恒假、Play2DStream 返回 null 照抄)。
// The whole of DummySoundEngine.cs L16-105 (DummySoundEngine/NullSoundSource/
// NullSound; the fallback target of DefaultPlatform.CreateSound when OpenAL
// initialization fails). The no-op semantics ported verbatim (NullSound's
// Complete stays false; Play2DStream returns null).
#pragma once
import std;

#include "platform/sound_engine.hpp"

namespace ora::platform {

class NullSoundSource final : public ISoundSource {};

class NullSound final : public ISound {
 public:
  float GetVolume() const override { return 0.0f; }
  void SetVolume(float) override {}
  [[nodiscard]] float SeekPosition() const override { return 0.0f; }
  [[nodiscard]] bool Complete() const override { return false; }
  void SetPosition(WPos) override {}
};

class DummySoundEngine final : public ISoundEngine {
 public:
  DummySoundEngine() = default;
  ~DummySoundEngine() override = default;

  [[nodiscard]] std::vector<SoundDevice> AvailableDevices() override {
    return {SoundDevice{std::nullopt, "No Sound Output"}};
  }
  [[nodiscard]] bool Dummy() const override { return true; }
  [[nodiscard]] float GetVolume() const override { return 0.0f; }
  void SetVolume(float) override {}

  [[nodiscard]] ISoundSource* AddSoundSourceFromMemory(std::span<const std::uint8_t>,
                                                       int, int, int) override {
    return new NullSoundSource{};
  }
  [[nodiscard]] std::unique_ptr<ISound> Play2D(ISoundSource*, bool, bool, WPos, float,
                                               bool) override {
    return std::make_unique<NullSound>();
  }
  [[nodiscard]] std::unique_ptr<ISound> Play2DStream(std::unique_ptr<IPcmStream>&&, int, int,
                                                     int, bool, bool, WPos,
                                                     float) override {
    return nullptr;
  }

  void PauseSound(ISound*, bool) override {}
  void StopSound(ISound*) override {}
  void SetAllSoundsPaused(bool) override {}
  void StopAllSounds() override {}
  void SetListenerPosition(WPos) override {}
  void SetSoundVolume(float, ISound*, ISound*) override {}
  void SetSoundLooping(bool, ISound*) override {}
  void SetSoundPosition(ISound*, WPos) override {}
};

}  // namespace ora::platform
