#pragma once

import std;

#include "game/support/mersenne_twister.hpp"

#include "game/sound/sound.hpp"

namespace ora::game {

struct music_playlist_info {
  std::string str_starting_music{};
  std::string str_background_music{};
  bool b_allow_mute_background_music{false};
};

struct music_player {
  sound_facade* ptr_facade{};
  SoundSettingsModel* ptr_settings{};
  std::vector<std::pair<std::string, music_info>> vec_library;
  std::vector<std::pair<std::string, music_info>> vec_playlist;
  std::vector<std::pair<std::string, music_info>> vec_random;
  std::optional<std::string> str_current_song{};
  std::optional<std::string> str_current_background_song{};
  bool b_current_song_is_background{false};
  bool b_is_music_available{false};
  bool b_allow_mute_background_music{false};
};

[[nodiscard]] std::vector<std::pair<std::string, music_info>> shuffle_playlist(
    const std::vector<std::pair<std::string, music_info>>& vec_playlist,
    support::MersenneTwister& mt_cosmetic);

[[nodiscard]] music_player make_music_player(
    sound_facade& facade, SoundSettingsModel& settings,
    const std::vector<std::pair<std::string, music_info>>& vec_library,
    support::MersenneTwister& mt_cosmetic, const music_playlist_info& info);

[[nodiscard]] const music_info* music_player_find(const music_player& player,
                                                  std::string_view str_key);

[[nodiscard]] std::optional<std::string> music_player_next_song(const music_player& player);
[[nodiscard]] std::optional<std::string> music_player_prev_song(const music_player& player);

void music_player_play(music_player& player);
void music_player_play_track(music_player& player, std::string_view str_key);
void music_player_stop(music_player& player);
void music_player_tick(music_player& player);

}
