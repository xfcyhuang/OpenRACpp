#pragma once
import std;

#include "game/game_records.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}
namespace ora::sound {
class Sound;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct MusicPlaylistInfoData {
  std::string str_starting_music;
  std::string str_victory_music;
  std::string str_defeat_music;
  std::string str_background_music;
  bool b_allow_mute_background_music = false;
  bool b_disable_world_sounds = false;

  static MusicPlaylistInfoData Parse(const meta::RecordObject& rec_info);
};

class MusicPlaylist final : public TraitBase,
                            public sim::INotifyActorDisposing,
                            public sim::IGameOver,
                            public sim::IPostWorldLoaded,
                            public sim::INotifyGameLoaded {
 public:
  MusicPlaylist(sim::World& world, MusicPlaylistInfoData&& info,
                sound::Sound* ptr_sound);

  ORA_TRAIT_INTERFACES(
      MusicPlaylist, OpenRA_Mods_Common_Traits_MusicPlaylist,
      sim::INotifyActorDisposing, sim::IGameOver, sim::IPostWorldLoaded,
      sim::INotifyGameLoaded)

  void Disposing(Actor& self) override;
  void GameOver(sim::World& world) override;
  void PostWorldLoaded(sim::World& world,
                       gfx::WorldRenderer* ptr_world_renderer) override;
  void GameLoaded(sim::World& world) override;

  bool IsMusicInstalled() const { return b_music_installed_; }
  bool IsMusicAvailable() const { return b_music_available_; }
  bool AllowMuteBackgroundMusic() const {
    return b_allow_mute_background_music_;
  }
  bool CurrentSongIsBackground() const { return b_current_song_is_background_; }
  bool IsBackgroundMusicMuted() const;

  const game::MusicInfo* CurrentSong() const { return ptr_current_song_; }
  const std::vector<game::MusicInfo*>& AvailablePlaylist() const {
    return vec_playlist_;
  }

  void Play(const game::MusicInfo* ptr_music);
  void Play(const game::MusicInfo* ptr_music,
            std::function<void()> fn_on_complete);
  void SetBackgroundMusic(const game::MusicInfo* ptr_music);
  game::MusicInfo* GetNextSong();
  game::MusicInfo* GetPrevSong();
  void Stop();

  static game::MusicInfo* FindSong(const sim::World& world,
                                         const std::string& str_key);

 private:
  void PlayInternal();
  void PlayNextSong();
  game::MusicInfo* GetSong(bool b_reverse);
  static bool SongExists(const game::MusicInfo* ptr_song);

  sim::World* ptr_world_ = nullptr;
  MusicPlaylistInfoData info_{};
  sound::Sound* ptr_sound_ = nullptr;
  std::vector<game::MusicInfo*> vec_random_;
  std::vector<game::MusicInfo*> vec_playlist_;
  bool b_music_installed_ = false;
  bool b_music_available_ = false;
  bool b_allow_mute_background_music_ = false;
  bool b_current_song_is_background_ = false;
  const game::MusicInfo* ptr_current_song_ = nullptr;
  const game::MusicInfo* ptr_current_background_song_ = nullptr;
};

}
