#include "mods/music_playlist.hpp"

#include "game/ruleset.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "sound/sound.hpp"

namespace ora::mods {

MusicPlaylistInfoData MusicPlaylistInfoData::Parse(
    const meta::RecordObject& rec_info) {
  MusicPlaylistInfoData data{};
  if (const auto v = sim::RecordFieldString(rec_info, "StartingMusic"))
    data.str_starting_music = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "VictoryMusic"))
    data.str_victory_music = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "DefeatMusic"))
    data.str_defeat_music = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "BackgroundMusic"))
    data.str_background_music = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info,
                                         "AllowMuteBackgroundMusic"))
    data.b_allow_mute_background_music = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "DisableWorldSounds"))
    data.b_disable_world_sounds = *v != 0;
  return data;
}

MusicPlaylist::MusicPlaylist(sim::World& world,
                             MusicPlaylistInfoData&& info,
                             sound::Sound* ptr_sound)
    : ptr_world_{&world}, info_{std::move(info)}, ptr_sound_{ptr_sound} {
  for (auto& [str_key, up_music] : world.Map().Rules().Music())
    if (up_music->Exists()) {
      b_music_installed_ = true;
      break;
    }
  if (!b_music_installed_)
    return;

  for (auto& [str_key, up_music] : world.Map().Rules().Music())
    if (up_music->Exists() && !up_music->Hidden())
      vec_playlist_.push_back(up_music.get());

  vec_random_ = vec_playlist_;
  for (std::size_t i = vec_random_.size(); i > 1; i--) {
    const std::size_t j = static_cast<std::size_t>(
        world.LocalRandom().Next(static_cast<std::int32_t>(i)));
    std::swap(vec_random_[j], vec_random_[i - 1]);
  }

  b_music_available_ = !vec_playlist_.empty();
  b_allow_mute_background_music_ = info_.b_allow_mute_background_music;

  if (game::MusicInfo* ptr_background =
          FindSong(world, info_.str_background_music);
      ptr_background != nullptr && ptr_background->Exists()) {
    ptr_current_song_ = ptr_background;
    ptr_current_background_song_ = ptr_background;
    b_current_song_is_background_ = true;
  } else {
    ptr_current_song_ = vec_random_.empty() ? nullptr : vec_random_.front();
  }

  if (game::MusicInfo* ptr_starting =
          FindSong(world, info_.str_starting_music);
      ptr_starting != nullptr && ptr_starting->Exists()) {
    ptr_current_song_ = ptr_starting;
    b_current_song_is_background_ = false;
  }
}

game::MusicInfo* MusicPlaylist::FindSong(const sim::World& world,
                                         const std::string& str_key) {
  if (str_key.empty())
    return nullptr;
  for (auto& [str_name, up_music] : world.Map().Rules().Music())
    if (str_name == str_key)
      return up_music.get();
  return nullptr;
}

bool MusicPlaylist::SongExists(const game::MusicInfo* ptr_song) {
  return ptr_song != nullptr && ptr_song->Exists();
}

bool MusicPlaylist::IsBackgroundMusicMuted() const {
  if (!b_allow_mute_background_music_ || ptr_sound_ == nullptr)
    return false;
  return ptr_sound_->SettingsSound().b_mute_background_music;
}

void MusicPlaylist::PostWorldLoaded(sim::World& world,
                                    gfx::WorldRenderer* ) {
  if (ptr_sound_ != nullptr)
    ptr_sound_->SetDisableWorldSounds(info_.b_disable_world_sounds);

  if (!world.IsLoadingGameSave())
    PlayInternal();
}

void MusicPlaylist::GameLoaded(sim::World& ) { PlayInternal(); }

void MusicPlaylist::GameOver(sim::World& world) {
  const sim::Player* ptr_local = world.LocalPlayer();
  const bool b_won = ptr_local != nullptr &&
                     ptr_local->PlayerWinState() == sim::WinState::Won;
  const std::string& str_key =
      b_won ? info_.str_victory_music : info_.str_defeat_music;
  game::MusicInfo* ptr_song = FindSong(world, str_key);
  if (ptr_song == nullptr || !ptr_song->Exists())
    return;

  ptr_current_background_song_ = ptr_song;
  Stop();
}

void MusicPlaylist::PlayInternal() {
  if (!SongExists(ptr_current_song_) ||
      (b_current_song_is_background_ && IsBackgroundMusicMuted()))
    return;

  if (ptr_sound_ != nullptr)
    ptr_sound_->PlayMusicThen(*const_cast<game::MusicInfo*>(ptr_current_song_),
                              [this] { PlayNextSong(); });
}

void MusicPlaylist::PlayNextSong() {
  if (!b_current_song_is_background_)
    ptr_current_song_ = GetNextSong();

  PlayInternal();
}

void MusicPlaylist::Play(const game::MusicInfo* ptr_music) {
  if (ptr_music == nullptr)
    return;

  ptr_current_song_ = ptr_music;
  b_current_song_is_background_ = false;

  PlayInternal();
}

void MusicPlaylist::Play(const game::MusicInfo* ptr_music,
                         std::function<void()> fn_on_complete) {
  if (ptr_music == nullptr)
    return;

  ptr_current_song_ = ptr_music;
  b_current_song_is_background_ = false;
  if (ptr_sound_ != nullptr)
    ptr_sound_->PlayMusicThen(*const_cast<game::MusicInfo*>(ptr_music),
                              std::move(fn_on_complete));
}

void MusicPlaylist::SetBackgroundMusic(const game::MusicInfo* ptr_music) {
  ptr_current_background_song_ = ptr_music;

  if (b_current_song_is_background_)
    Stop();
}

game::MusicInfo* MusicPlaylist::GetNextSong() { return GetSong(false); }

game::MusicInfo* MusicPlaylist::GetPrevSong() { return GetSong(true); }

game::MusicInfo* MusicPlaylist::GetSong(bool b_reverse) {
  if (!b_music_available_)
    return nullptr;

  const std::vector<game::MusicInfo*>& vec_songs =
      ptr_sound_ != nullptr && ptr_sound_->SettingsSound().b_shuffle
          ? vec_random_
          : vec_playlist_;

  game::MusicInfo* ptr_next = nullptr;
  if (b_reverse) {
    for (std::size_t i = vec_songs.size(); i > 0; i--)
      if (vec_songs[i - 1] == ptr_current_song_) {
        ptr_next = i >= 2 ? vec_songs[i - 2] : nullptr;
        break;
      }
    if (ptr_next == nullptr && !vec_songs.empty())
      ptr_next = vec_songs.back();
  } else {
    for (std::size_t i = 0; i < vec_songs.size(); i++)
      if (vec_songs[i] == ptr_current_song_) {
        ptr_next = i + 1 < vec_songs.size() ? vec_songs[i + 1] : nullptr;
        break;
      }
    if (ptr_next == nullptr && !vec_songs.empty())
      ptr_next = vec_songs.front();
  }

  if (SongExists(ptr_next))
    return ptr_next;

  return nullptr;
}

void MusicPlaylist::Stop() {
  ptr_current_song_ = nullptr;
  if (ptr_sound_ != nullptr)
    ptr_sound_->StopMusic();

  if (ptr_current_background_song_ != nullptr) {
    ptr_current_song_ = ptr_current_background_song_;
    b_current_song_is_background_ = true;

    if (!IsBackgroundMusicMuted())
      PlayInternal();
  }
}

void MusicPlaylist::Disposing(Actor& ) {
  if (ptr_current_song_ != nullptr && ptr_sound_ != nullptr)
    ptr_sound_->StopMusic();

  if (ptr_sound_ != nullptr)
    ptr_sound_->SetDisableWorldSounds(false);
}

}
