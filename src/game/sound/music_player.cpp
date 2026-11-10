#include "game/sound/music_player.hpp"

namespace ora::game {

namespace {

const music_info* library_find(const std::vector<std::pair<std::string, music_info>>& vec_library,
                               std::string_view str_key) {
  for (const auto& [str_entry, info] : vec_library)
    if (str_entry == str_key) return &info;
  return nullptr;
}

bool background_music_muted(const music_player& player) {
  return player.b_allow_mute_background_music && player.ptr_settings != nullptr &&
         player.ptr_settings->b_mute_background_music;
}

std::optional<std::string> find_next_key(const std::vector<std::pair<std::string, music_info>>& vec,
                                         const std::optional<std::string>& opt_current,
                                         bool b_reverse) {
  if (vec.empty()) return std::nullopt;
  std::optional<std::size_t> sz_current_index;
  if (opt_current.has_value()) {
    for (std::size_t sz_i{0}; sz_i < vec.size(); ++sz_i)
      if (vec[sz_i].first == *opt_current) {
        sz_current_index = sz_i;
        break;
      }
  }
  std::optional<std::size_t> sz_next_index;
  if (sz_current_index.has_value()) {
    const std::size_t sz_step{b_reverse ? *sz_current_index : vec.size() - 1 - *sz_current_index};
    if (sz_step > 0) sz_next_index = b_reverse ? *sz_current_index - 1 : *sz_current_index + 1;
  }
  if (!sz_next_index.has_value()) sz_next_index = b_reverse ? vec.size() - 1 : 0;
  return vec[*sz_next_index].first;
}

const music_info* current_info(const music_player& player) {
  if (!player.str_current_song.has_value()) return nullptr;
  return library_find(player.vec_library, *player.str_current_song);
}

void play_next_song(music_player& player) {
  if (!player.b_current_song_is_background)
    player.str_current_song = music_player_next_song(player);
  music_player_play(player);
}

}

std::vector<std::pair<std::string, music_info>> shuffle_playlist(
    const std::vector<std::pair<std::string, music_info>>& vec_playlist,
    support::MersenneTwister& mt_cosmetic) {
  std::vector<std::pair<std::string, music_info>> vec_out{vec_playlist};
  for (std::size_t sz_i{0}; sz_i + 1 < vec_out.size(); ++sz_i) {
    const std::int32_t int4_span{static_cast<std::int32_t>(vec_out.size() - sz_i)};
    const std::int32_t int4_j{support::next(mt_cosmetic, int4_span)};
    std::swap(vec_out[sz_i], vec_out[static_cast<std::size_t>(sz_i) +
                                    static_cast<std::size_t>(int4_j)]);
  }
  return vec_out;
}

music_player make_music_player(sound_facade& facade, SoundSettingsModel& settings,
                               const std::vector<std::pair<std::string, music_info>>& vec_library,
                               support::MersenneTwister& mt_cosmetic,
                               const music_playlist_info& info) {
  music_player player{};
  player.ptr_facade = &facade;
  player.ptr_settings = &settings;
  player.vec_library = vec_library;

  bool b_installed{false};
  for (const auto& [str_key, track] : vec_library)
    if (track.b_exists) {
      b_installed = true;
      break;
    }
  if (!b_installed) return player;

  for (const auto& [str_key, track] : vec_library)
    if (track.b_exists && !track.b_hidden) player.vec_playlist.emplace_back(str_key, track);

  player.vec_random = shuffle_playlist(player.vec_playlist, mt_cosmetic);
  player.b_is_music_available = !player.vec_playlist.empty();
  player.b_allow_mute_background_music = info.b_allow_mute_background_music;

  const music_info* ptr_background{library_find(player.vec_library, info.str_background_music)};
  if (ptr_background != nullptr && ptr_background->b_exists) {
    player.str_current_background_song = info.str_background_music;
    player.str_current_song = info.str_background_music;
    player.b_current_song_is_background = true;
  } else if (!player.vec_random.empty()) {
    player.str_current_song = player.vec_random.front().first;
  }

  const music_info* ptr_starting{library_find(player.vec_library, info.str_starting_music)};
  if (ptr_starting != nullptr && ptr_starting->b_exists) {
    player.str_current_song = info.str_starting_music;
    player.b_current_song_is_background = false;
  }
  return player;
}

const music_info* music_player_find(const music_player& player, std::string_view str_key) {
  return library_find(player.vec_library, str_key);
}

std::optional<std::string> music_player_next_song(const music_player& player) {
  if (!player.b_is_music_available) return std::nullopt;
  const bool b_shuffle{player.ptr_settings != nullptr && player.ptr_settings->b_shuffle};
  const std::optional<std::string> str_key{find_next_key(
      b_shuffle ? player.vec_random : player.vec_playlist, player.str_current_song, false)};
  if (!str_key.has_value()) return std::nullopt;
  const music_info* ptr_info{library_find(player.vec_library, *str_key)};
  if (ptr_info == nullptr || !ptr_info->b_exists) return std::nullopt;
  return str_key;
}

std::optional<std::string> music_player_prev_song(const music_player& player) {
  if (!player.b_is_music_available) return std::nullopt;
  const bool b_shuffle{player.ptr_settings != nullptr && player.ptr_settings->b_shuffle};
  const std::optional<std::string> str_key{find_next_key(
      b_shuffle ? player.vec_random : player.vec_playlist, player.str_current_song, true)};
  if (!str_key.has_value()) return std::nullopt;
  const music_info* ptr_info{library_find(player.vec_library, *str_key)};
  if (ptr_info == nullptr || !ptr_info->b_exists) return std::nullopt;
  return str_key;
}

void music_player_play(music_player& player) {
  const music_info* ptr_info{current_info(player)};
  if (ptr_info == nullptr || !ptr_info->b_exists) return;
  if (player.b_current_song_is_background && background_music_muted(player)) return;
  sound_facade_play_music_then(*player.ptr_facade, *ptr_info);
}

void music_player_play_track(music_player& player, std::string_view str_key) {
  const music_info* ptr_info{library_find(player.vec_library, str_key)};
  if (ptr_info == nullptr) return;
  player.str_current_song = std::string{str_key};
  player.b_current_song_is_background = false;
  music_player_play(player);
}

void music_player_stop(music_player& player) {
  player.str_current_song = std::nullopt;
  sound_facade_stop_music(*player.ptr_facade);
  if (player.str_current_background_song.has_value()) {
    player.str_current_song = player.str_current_background_song;
    player.b_current_song_is_background = true;
    if (!background_music_muted(player)) music_player_play(player);
  }
}

void music_player_tick(music_player& player) {
  if (sound_facade_tick(*player.ptr_facade) != sound_tick_event::k_music_complete) return;
  play_next_song(player);
}

}
