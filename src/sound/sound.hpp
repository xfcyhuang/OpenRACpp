#pragma once
import std;

#include "core/mersenne_twister.hpp"
#include "core/wpos.hpp"
#include "fs/file_system.hpp"
#include "game/game_records.hpp"
#include "game/ruleset.hpp"
#include "platform/sound_engine.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "sound/sound_loader.hpp"

namespace ora::sound {

enum class SoundType : std::uint8_t { World, UI };

struct SoundSettingsFace {
  float fp4_sound_volume = 0.5f;
  float fp4_music_volume = 0.5f;
  float fp4_video_volume = 0.5f;
  bool b_shuffle = false;
  bool b_repeat = false;
  bool b_mute = false;
  bool b_mute_background_music = false;
};

class Sound final {
 public:

  Sound(std::unique_ptr<platform::ISoundEngine> up_engine, SoundSettingsFace& settings_sound);

  ~Sound();

  Sound(const Sound&) = delete;
  Sound& operator=(const Sound&) = delete;

  bool DummyEngine() const { return b_dummy_engine_; }

  void Initialize(std::span<const SoundLoaderFn> vec_loaders, fs::FileSystem& file_system);

  std::vector<platform::SoundDevice> AvailableDevices();
  void SetListenerPosition(WPos pos_position);

  void StopAudio();
  void SetLooped(platform::ISound* ptr_sound, bool b_looped);
  void SetPosition(platform::ISound* ptr_sound, WPos pos_position);
  void MuteAudio();
  void UnmuteAudio();
  void SetMusicLooped(bool b_loop);

  bool DisableAllSounds() const { return b_disable_all_sounds_; }
  void SetDisableAllSounds(bool b_value) { b_disable_all_sounds_ = b_value; }
  bool DisableWorldSounds() const { return b_disable_world_sounds_; }
  void SetDisableWorldSounds(bool b_value) { b_disable_world_sounds_ = b_value; }

  void SetLocalPlayer(const sim::Player* ptr_local) { ptr_local_player_ = ptr_local; }

  void SetSelectionContains(std::function<bool(const sim::Actor&)> fn_contains) {
    fn_selection_contains_ = std::move(fn_contains);
  }

  platform::ISound* Play(SoundType kind_type, const std::string& str_name);
  platform::ISound* Play(SoundType kind_type, const std::string& str_name, WPos pos_position);
  platform::ISound* Play(SoundType kind_type, const std::string& str_name, float fp4_volume_modifier);
  platform::ISound* Play(SoundType kind_type, const std::string& str_name, WPos pos_position,
                         float fp4_volume_modifier);
  platform::ISound* PlayToPlayer(SoundType kind_type, const sim::Player* ptr_player,
                                 const std::string& str_name);
  platform::ISound* PlayToPlayer(SoundType kind_type, const sim::Player* ptr_player,
                                 const std::string& str_name, WPos pos_position);
  platform::ISound* PlayLooped(SoundType kind_type, const std::string& str_name);
  platform::ISound* PlayLooped(SoundType kind_type, const std::string& str_name, WPos pos_position);

  platform::ISound* Play(SoundType kind_type, std::span<const std::string> vec_names,
                         sim::World& world_sim, const sim::Player* ptr_player = nullptr,
                         float fp4_volume_modifier = 1.0f);
  platform::ISound* Play(SoundType kind_type, std::span<const std::string> vec_names,
                         sim::World& world_sim, WPos pos_position, const sim::Player* ptr_player = nullptr,
                         float fp4_volume_modifier = 1.0f);

  platform::ISound* PlayFromPcm(const SoundFormatInfo& info_format,
                                std::span<const std::byte> vec_pcm, float fp4_volume);

  void PlayVideo(std::span<const std::uint8_t> vec_raw, int int4_channels, int int4_sample_bits,
                 int int4_sample_rate);
  void PlayVideo();
  void PauseVideo();
  void StopVideo();

  void Tick();

  bool MusicPlaying() const { return b_music_playing_; }
  const game::MusicInfo* CurrentMusic() const { return ptr_current_music_; }

  void PlayMusicThen(game::MusicInfo& info_music, std::function<void()> fn_then);
  void PlayMusic(game::MusicInfo& info_music, bool b_looped = false);
  void PlayMusic();
  void StopSound(platform::ISound* ptr_sound);
  void StopMusic();
  void PauseMusic();

  float SoundVolumeModifier() const { return fp4_sound_volume_modifier_; }
  void SetSoundVolumeModifier(float fp4_value);
  float SoundVolume() const { return settings_.fp4_sound_volume; }
  void SetSoundVolume(float fp4_value);
  float MusicVolume() const { return settings_.fp4_music_volume; }
  void SetMusicVolume(float fp4_value);
  float VideoVolume() const { return settings_.fp4_video_volume; }
  void SetVideoVolume(float fp4_value);
  const SoundSettingsFace& SettingsSound() const { return settings_; }
  SoundSettingsFace& MutableSettingsSound() { return settings_; }

  float MusicSeekPosition() const;
  float VideoSeekPosition() const;

  bool PlayPredefined(SoundType kind_type, const game::Ruleset& ruleset, const sim::Player* ptr_player,
                      const sim::Actor* actor_voiced, std::string_view str_type,
                      std::string_view str_definition, std::string_view str_variant, bool b_relative,
                      WPos pos_position, float fp4_volume_modifier, bool b_attenuate_volume);

  bool PlayNotification(const game::Ruleset& ruleset, const sim::Player* ptr_player,
                        std::string_view str_type, std::string_view str_notification,
                        std::string_view str_variant);

  void LoadMusicInfo(game::MusicInfo& info_music);

 private:

  platform::ISoundSource* GetOrLoadSource(const std::string& str_filename);

  platform::ISound* PlayInternal(SoundType kind_type, const sim::Player* ptr_player,
                                 const std::string& str_name, bool b_head_relative, WPos pos_position,
                                 float fp4_volume_modifier = 1.0f, bool b_loop = false);

  float InternalSoundVolume() const {
    return settings_.fp4_sound_volume * fp4_sound_volume_modifier_;
  }

  std::unique_ptr<platform::ISoundEngine> up_engine_;
  SoundSettingsFace& settings_;
  bool b_dummy_engine_ = false;

  std::vector<SoundLoaderFn> vec_loaders_;
  fs::FileSystem* ptr_file_system_ = nullptr;

  std::vector<std::pair<std::string, platform::ISoundSource*>> vec_sources_;

  platform::ISoundSource* ptr_video_source_ = nullptr;
  std::unique_ptr<platform::ISound> up_music_;
  std::unique_ptr<platform::ISound> up_video_;

  std::vector<std::unique_ptr<platform::ISound>> vec_playing_;

  std::vector<std::pair<std::uint32_t, platform::ISound*>> vec_current_sounds_;
  std::vector<std::pair<std::string, platform::ISound*>> vec_current_notifications_;

  bool b_disable_all_sounds_ = false;
  bool b_disable_world_sounds_ = false;
  const sim::Player* ptr_local_player_ = nullptr;
  std::function<bool(const sim::Actor&)> fn_selection_contains_;

  std::function<void()> fn_on_music_complete_;
  bool b_music_playing_ = false;
  game::MusicInfo* ptr_current_music_ = nullptr;

  float fp4_sound_volume_modifier_ = 1.0f;

  MersenneTwister mt_cosmetic_;
};

}
