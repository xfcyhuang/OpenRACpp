// UPSTREAM: OpenRA.Game/Sound/Sound.cs @b6fc03f L36-481(全文逐语义)
// 声音门面:格式链 + 源缓存 + 调度(Play 族/PlayPredefined 通知池)、音乐
// 与视频声道、音量面。Game.Settings/Game.CosmeticRandom/Selection 以注入
// 面承载(主循环 Phase 5 接线)。
// The sound facade: the format chain + the source cache + scheduling (the
// Play family / PlayPredefined's notification pools), the music and video
// channels, and the volume faces. Game.Settings / Game.CosmeticRandom /
// Selection arrive as injection faces (the main loop wires them in Phase 5).
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

/// SoundType(Sound.cs L36)。
enum class SoundType : std::uint8_t { World, UI };

/// SoundSettings 的值面(Settings.cs L287-306;Sound 活读写)。
/// The value face of SoundSettings (Settings.cs L287-306; read and written
/// live by Sound).
struct SoundSettingsFace {
  float fp4_sound_volume = 0.5f;
  float fp4_music_volume = 0.5f;
  float fp4_video_volume = 0.5f;
  bool b_shuffle = false;
  bool b_repeat = false;
  bool b_mute = false;
};

/// Sound(Sound.cs L38-481)。
class Sound final {
 public:
  /// 上游 ctor 的注入形态:platform.CreateSound → 引擎所有权;settings =
  /// Game.Settings.Sound;cosmetic = Game.CosmeticRandom。
  /// The injected shape of the upstream ctor: platform.CreateSound becomes
  /// engine ownership; settings = Game.Settings.Sound; cosmetic =
  /// Game.CosmeticRandom.
  Sound(std::unique_ptr<platform::ISoundEngine> up_engine, SoundSettingsFace& settings_sound);

  ~Sound();

  Sound(const Sound&) = delete;
  Sound& operator=(const Sound&) = delete;

  bool DummyEngine() const { return b_dummy_engine_; }

  /// Initialize(L85-102):停止一切、释放源缓存、换装文件系统与格式链。
  /// Initialize (L85-102): stop everything, free the source cache, and swap
  /// in the filesystem and format chain.
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

  /// 本地玩家面(上游 player.World.LocalPlayer;随战局设置)。
  /// The local-player face (upstream's player.World.LocalPlayer; set per
  /// game).
  void SetLocalPlayer(const sim::Player* ptr_local) { ptr_local_player_ = ptr_local; }

  /// Selection.Contains 面(上游 voicedActor.World.Selection;默认恒假 =
  /// 空选区)。
  /// The Selection.Contains face (upstream's
  /// voicedActor.World.Selection; false by default = the empty selection).
  void SetSelectionContains(std::function<bool(const sim::Actor&)> fn_contains) {
    fn_selection_contains_ = std::move(fn_contains);
  }

  // ———— Play 族(L160-185)————
  // ———— The Play family (L160-185) ————
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

  /// names.Random(world.LocalRandom) 两形态(L169-177)。
  /// The two names.Random(world.LocalRandom) forms (L169-177).
  platform::ISound* Play(SoundType kind_type, std::span<const std::string> vec_names,
                         sim::World& world_sim, const sim::Player* ptr_player = nullptr,
                         float fp4_volume_modifier = 1.0f);
  platform::ISound* Play(SoundType kind_type, std::span<const std::string> vec_names,
                         sim::World& world_sim, WPos pos_position, const sim::Player* ptr_player = nullptr,
                         float fp4_volume_modifier = 1.0f);

  /// Play(ISoundFormat)(L179-185):外部 PCM 流(UI 音轨等)。
  /// Play (ISoundFormat) (L179-185): an external PCM stream (UI tracks
  /// among others).
  platform::ISound* PlayFromPcm(const SoundFormatInfo& info_format,
                                std::span<const std::byte> vec_pcm, float fp4_volume);

  void PlayVideo(std::span<const std::uint8_t> vec_raw, int int4_channels, int int4_sample_bits,
                 int int4_sample_rate);
  void PlayVideo();
  void PauseVideo();
  void StopVideo();

  /// Tick(L217-225):歌毕检测。
  /// Tick (L217-225): the song-finished check.
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

  float MusicSeekPosition() const;
  float VideoSeekPosition() const;

  /// PlayPredefined(L360-459):通知/语音调度(池/变体/前缀/打断策略)。
  /// PlayPredefined (L360-459): the notification/voice scheduling (the
  /// pools/variants/prefixes/interrupt policies).
  bool PlayPredefined(SoundType kind_type, const game::Ruleset& ruleset, const sim::Player* ptr_player,
                      const sim::Actor* actor_voiced, std::string_view str_type,
                      std::string_view str_definition, std::string_view str_variant, bool b_relative,
                      WPos pos_position, float fp4_volume_modifier, bool b_attenuate_volume);

  /// PlayNotification(L461-469)。
  bool PlayNotification(const game::Ruleset& ruleset, const sim::Player* ptr_player,
                        std::string_view str_type, std::string_view str_notification,
                        std::string_view str_variant);

  /// MusicInfo.Load 的驱动面(MusicInfo.cs L49-72):打开 + 链内首查。
  /// The driving face of MusicInfo::Load (MusicInfo.cs L49-72): open + the
  /// first chain hit.
  void LoadMusicInfo(game::MusicInfo& info_music);

 private:
  /// sounds[name](Cache 首查工厂;文件缺失 → null 源)。
  /// sounds[name] (the Cache's first-query factory; a missing file → the
  /// null source).
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

  /// Cache&lt;string, ISoundSource&gt; 的保序 vector(首查工厂物化)。
  /// The order-preserving vector of Cache<string, ISoundSource>
  /// (materialized by the first-query factory).
  std::vector<std::pair<std::string, platform::ISoundSource*>> vec_sources_;

  platform::ISoundSource* ptr_video_source_ = nullptr;
  std::unique_ptr<platform::ISound> up_music_;
  std::unique_ptr<platform::ISound> up_video_;

  /// Play2D 产物的所有权表(上游 GC 的显式等价;map 存非拥有引用)
  /// The ownership list of Play2D products (the explicit equivalent of
  /// upstream's GC; the maps hold non-owning references).
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

  /// Game.CosmeticRandom 注入面(种子可测)。
  /// The Game.CosmeticRandom injection face (seedable for tests).
  MersenneTwister mt_cosmetic_;
};

}  // namespace ora::sound
