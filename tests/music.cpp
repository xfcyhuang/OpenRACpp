import std;

#include "game/music_info.hpp"
#include "game/settings.hpp"
#include "game/sound/music_player.hpp"
#include "game/sound/sound.hpp"
#include "game/support/mersenne_twister.hpp"
#include "platforms/sound_engine.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_MUSIC_GOLDEN_DIR
#define ORA_MUSIC_GOLDEN_DIR "tests/golden/music"
#endif

namespace {

ora::game::music_info make_track(std::string str_filename, float f4_modifier, bool b_exists) {
  ora::game::music_info info{};
  info.str_filename = std::move(str_filename);
  info.f4_volume_modifier = f4_modifier;
  info.b_exists = b_exists;
  return info;
}

std::vector<std::pair<std::string, ora::game::music_info>> make_library() {
  std::vector<std::pair<std::string, ora::game::music_info>> vec_library;
  for (std::int32_t int4_i{1}; int4_i <= 12; ++int4_i) {
    std::string str_key{std::format("t{:02d}", int4_i)};
    ora::game::music_info info{make_track(str_key + ".aud", 1.0f, int4_i != 6)};
    if (int4_i == 3 || int4_i == 7) info.b_hidden = true;
    if (int4_i == 2) info.f4_volume_modifier = 0.7f;
    if (int4_i == 10) info.f4_volume_modifier = 0.6f;
    vec_library.emplace_back(str_key, info);
  }
  return vec_library;
}

std::string playlist_keys(const std::vector<std::pair<std::string, ora::game::music_info>>& vec) {
  std::string str_out;
  for (const auto& [str_key, info] : vec) {
    if (!str_out.empty()) str_out += ",";
    str_out += str_key;
  }
  return str_out;
}

ora::game::music_player make_player(
    ora::game::SoundSettingsModel& settings, ora::game::sound_facade& facade,
    ora::game::support::MersenneTwister& mt_cosmetic,
    const ora::game::music_playlist_info& playlist_info) {
  return ora::game::make_music_player(facade, settings, make_library(), mt_cosmetic,
                                      playlist_info);
}

bool current_is(const ora::game::music_player& player, std::string_view str_key) {
  return player.str_current_song.has_value() && *player.str_current_song == str_key;
}

std::optional<std::string> neighbor_key(
    const std::vector<std::pair<std::string, ora::game::music_info>>& vec,
    std::string_view str_key, bool b_reverse) {
  for (std::size_t sz_i{0}; sz_i < vec.size(); ++sz_i) {
    if (vec[sz_i].first != str_key) continue;
    if (b_reverse) return sz_i > 0 ? vec[sz_i - 1].first : vec.back().first;
    return sz_i + 1 < vec.size() ? vec[sz_i + 1].first : vec.front().first;
  }
  return std::nullopt;
}

std::string fmt_float_r(float f4_value) {
  char buf_chars[32]{};
  const auto res_chars{std::to_chars(buf_chars, buf_chars + 32u, f4_value)};
  return std::string{buf_chars, res_chars.ptr};
}

std::vector<std::pair<std::string, ora::game::music_info>> playlist_of_library(
    const std::vector<std::pair<std::string, ora::game::music_info>>& vec_library) {
  std::vector<std::pair<std::string, ora::game::music_info>> vec_out;
  for (const auto& [str_key, track] : vec_library)
    if (track.b_exists && !track.b_hidden) vec_out.emplace_back(str_key, track);
  return vec_out;
}

constexpr char kCharNewline{'\n'};

std::vector<std::string> golden_data_lines(const std::string& str_text) {
  std::vector<std::string> vec_lines;
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    const std::size_t idx_nl{str_text.find(kCharNewline, sz_pos)};
    const std::size_t sz_end{idx_nl == std::string::npos ? str_text.size() : idx_nl};
    if (sz_end > sz_pos && str_text[sz_pos] != '#')
      vec_lines.emplace_back(str_text.substr(sz_pos, sz_end - sz_pos));
    if (idx_nl == std::string::npos) break;
    sz_pos = idx_nl + 1;
  }
  return vec_lines;
}

std::string join_keys(const std::vector<std::pair<std::string, ora::game::music_info>>& vec) {
  std::string str_out;
  for (const auto& [str_key, info] : vec) {
    if (!str_out.empty()) str_out += ",";
    str_out += str_key;
  }
  return str_out;
}

struct music_scenario {
  std::int32_t int4_seed{1};
  bool b_shuffle{false};
  bool b_repeat{false};
  std::string str_background{};
  std::string str_starting{};
  bool b_allow_mute_background{false};
  bool b_mute_background{false};
  std::vector<std::string> vec_ops;
};

std::string run_music_scenario(
    const music_scenario& scenario,
    const std::vector<std::pair<std::string, ora::game::music_info>>& vec_library) {
  ora::game::SoundSettingsModel settings{};
  settings.b_shuffle = scenario.b_shuffle;
  settings.b_repeat = scenario.b_repeat;
  settings.b_mute_background_music = scenario.b_mute_background;
  ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
  ora::game::support::MersenneTwister mt_cosmetic{
      ora::game::support::make_mersenne_twister(scenario.int4_seed)};
  ora::game::music_playlist_info info{};
  info.str_background_music = scenario.str_background;
  info.str_starting_music = scenario.str_starting;
  info.b_allow_mute_background_music = scenario.b_allow_mute_background;
  ora::game::music_player player{
      ora::game::make_music_player(facade, settings, vec_library, mt_cosmetic, info)};

  std::int32_t int4_installed{0};
  for (const auto& [str_key, track] : vec_library)
    if (track.b_exists) ++int4_installed;

  std::string str_out{std::format(
      "# music player scenario seed={} shuffle={} repeat={} music-volume={} background={} "
      "starting={} allow-mute-background={} mute-background={}\n",
      scenario.int4_seed, scenario.b_shuffle ? "True" : "False",
      scenario.b_repeat ? "True" : "False", fmt_float_r(settings.fp_music_volume),
      scenario.str_background.empty() ? "-" : scenario.str_background,
      scenario.str_starting.empty() ? "-" : scenario.str_starting,
      scenario.b_allow_mute_background ? "True" : "False",
      scenario.b_mute_background ? "True" : "False")};
  str_out += std::format("init installed={} available={} playlist={} random={}\n",
                         int4_installed, player.b_is_music_available ? "True" : "False",
                         join_keys(player.vec_playlist), join_keys(player.vec_random));

  std::vector<std::string> vec_played_keys;
  for (const std::string& str_op : scenario.vec_ops) {
    auto step_play_line = [&](const std::string& str_prefix, std::size_t sz_before) {
      const std::vector<ora::platforms::sound_event>& vec{
          ora::platforms::engine_play_events(facade.engine)};
      if (vec.size() == sz_before) return str_prefix + "same";
      const ora::platforms::sound_event& event{vec.back()};
      std::string str_key;
      for (const auto& [str_k, track] : vec_library)
        if (track.str_filename == event.str_name) str_key = str_k;
      return str_prefix + "play " + str_key + " vol=" + fmt_float_r(event.f4_volume) +
             " loop=" + (event.b_loop ? "True" : "False");
    };
    std::string str_line{"op " + str_op + " -> "};
    if (str_op == "play") {
      const std::size_t sz_before{ora::platforms::engine_play_events(facade.engine).size()};
      ora::game::music_player_play(player);
      if (ora::platforms::engine_play_events(facade.engine).size() > sz_before)
        vec_played_keys.push_back(*player.str_current_song);
      str_line += step_play_line("", sz_before);
    } else if (str_op == "complete") {
      const std::size_t sz_before{ora::platforms::engine_play_events(facade.engine).size()};
      if (facade.opt_music.has_value())
        ora::platforms::engine_stream_finish(facade.engine, *facade.opt_music);
      ora::game::music_player_tick(player);
      if (ora::platforms::engine_play_events(facade.engine).size() > sz_before)
        vec_played_keys.push_back(*player.str_current_song);
      str_line += step_play_line("", sz_before);
    } else if (str_op == "next") {
      const std::optional<std::string> str_key{ora::game::music_player_next_song(player)};
      str_line += str_key.has_value() ? *str_key : "-";
    } else if (str_op == "prev") {
      const std::optional<std::string> str_key{ora::game::music_player_prev_song(player)};
      str_line += str_key.has_value() ? *str_key : "-";
    } else if (str_op.starts_with("play-key ")) {
      const std::string str_key{str_op.substr(9)};
      const std::size_t sz_before{ora::platforms::engine_play_events(facade.engine).size()};
      ora::game::music_player_play_track(player, str_key);
      if (ora::platforms::engine_play_events(facade.engine).size() > sz_before)
        vec_played_keys.push_back(str_key);
      str_line += step_play_line("", sz_before);
    } else if (str_op == "stop") {
      const std::size_t sz_before{ora::platforms::engine_play_events(facade.engine).size()};
      ora::game::music_player_stop(player);
      str_line += "stopped";
      if (ora::platforms::engine_play_events(facade.engine).size() > sz_before) {
        vec_played_keys.push_back(*player.str_current_song);
        str_line += step_play_line("; ", sz_before);
      }
    } else if (str_op.starts_with("volume ")) {
      float f4_volume{0.0f};
      const auto res_parse{
          std::from_chars(str_op.data() + 7, str_op.data() + str_op.size(), f4_volume)};
      if (res_parse.ec == std::errc{}) {
        ora::game::sound_facade_set_music_volume(facade, f4_volume);
        const std::optional<float> f4_live{
            facade.opt_music.has_value()
                ? ora::platforms::sound_volume_get(facade.engine, *facade.opt_music)
                : std::nullopt};
        str_line += f4_live.has_value() ? "live=" + fmt_float_r(*f4_live) : "live=-";
      } else {
        str_line += "bad-volume";
      }
    } else if (str_op.starts_with("mute-background ")) {
      settings.b_mute_background_music = str_op.substr(16) == "True";
      str_line += std::format("muted={}", settings.b_mute_background_music ? "True" : "False");
    } else {
      return std::string{"unknown op: "} + str_op + "\n";
    }
    str_out += str_line + "\n";
  }

  str_out += "events";
  for (const std::string& str_key : vec_played_keys) str_out += " " + str_key;
  str_out += "\n";
  return str_out;
}

}

int main() {
  ora::platforms::openal_probe_override("ora-music-test-no-such-dir/soft_oal.dll");
  {
    ora::platforms::sound_engine engine{ora::platforms::make_dummy_engine()};
    check(ora::platforms::engine_play_events(engine).empty(), "no events before play");
    const std::optional<ora::platforms::sound> opt_snd{
        ora::platforms::engine_play_2d_stream(engine, "t01.aud", false, 0.35f)};
    check(opt_snd.has_value(), "dummy stream play returns sound");
    const std::vector<ora::platforms::sound_event>& vec_events{
        ora::platforms::engine_play_events(engine)};
    check(vec_events.size() == 1, "one play event recorded");
    if (vec_events.size() == 1) {
      check(vec_events[0].str_name == "t01.aud", "event records stream name");
      check(vec_events[0].f4_volume == 0.35f, "event records stream volume");
      check(!vec_events[0].b_loop, "event records loop flag");
    }
    check(opt_snd.has_value() && ora::platforms::sound_complete(engine, *opt_snd) == false,
          "stream sound not complete before finish");
    ora::platforms::engine_stream_finish(engine, *opt_snd);
    check(opt_snd.has_value() && ora::platforms::sound_complete(engine, *opt_snd),
          "stream sound complete after finish");
    const std::optional<ora::platforms::sound> opt_loop{
        ora::platforms::engine_play_2d_stream(engine, "t02.aud", true, 0.7f)};
    check(opt_loop.has_value(), "second stream play returns sound");
    check(ora::platforms::engine_play_events(engine).size() == 2, "second event recorded");
    if (ora::platforms::engine_play_events(engine).size() == 2)
      check(ora::platforms::engine_play_events(engine)[1].b_loop, "loop flag recorded");
  }
  {
    ora::game::SoundSettingsModel settings{};
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    check(facade.ptr_settings == &settings, "facade holds settings reference");
    check(!ora::game::sound_facade_music_playing(facade), "not playing at start");

    const ora::game::music_info track{make_track("t01.aud", 0.7f, true)};
    ora::game::sound_facade_play_music_then(facade, track);
    check(ora::game::sound_facade_music_playing(facade), "playing after play_music_then");
    check(facade.opt_current_music.has_value() &&
              facade.opt_current_music->str_filename == "t01.aud",
          "current music set");
    const std::vector<ora::platforms::sound_event>& vec_events{
        ora::platforms::engine_play_events(facade.engine)};
    check(vec_events.size() == 1 && vec_events[0].f4_volume == 0.35f,
          "play volume is music_volume times modifier");

    ora::game::sound_facade_play_music_then(facade, track);
    check(vec_events.size() == 1, "same track replay does not re-stream");
    check(ora::game::sound_facade_music_playing(facade), "same track replay keeps playing");

    const ora::game::music_info missing{make_track("no.aud", 1.0f, false)};
    ora::game::sound_facade_play_music_then(facade, missing);
    check(vec_events.size() == 1, "missing track does not stream");
    check(ora::game::sound_facade_music_playing(facade), "missing track keeps current playing");

    check(ora::game::sound_facade_tick(facade) == ora::game::sound_tick_event::k_none,
          "no complete event before finish");
    ora::platforms::engine_stream_finish(facade.engine, *facade.opt_music);
    check(ora::game::sound_facade_tick(facade) == ora::game::sound_tick_event::k_music_complete,
          "complete event on finished stream");
    check(!ora::game::sound_facade_music_playing(facade), "stopped after complete");
    check(!facade.opt_current_music.has_value(), "current music cleared on stop");
    check(ora::game::sound_facade_tick(facade) == ora::game::sound_tick_event::k_none,
          "no event when idle");
  }
  {
    ora::game::SoundSettingsModel settings{};
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    const ora::game::music_info track{make_track("t01.aud", 1.0f, true)};
    check(ora::game::sound_facade_music_volume(facade) == 0.5f, "music volume default 0.5");

    ora::game::sound_facade_play_music(facade, track, false);
    check(ora::platforms::engine_play_events(facade.engine).size() == 1, "play_music streams");

    ora::game::sound_facade_set_music_volume(facade, 0.25f);
    check(settings.fp_music_volume == 0.25f, "music volume setter writes settings");
    check(ora::game::sound_facade_music_volume(facade) == 0.25f, "music volume getter reads settings");
    check(ora::platforms::sound_volume_get(facade.engine, *facade.opt_music).has_value() &&
              *ora::platforms::sound_volume_get(facade.engine, *facade.opt_music) == 0.25f,
          "live music gain updated in place");

    ora::game::sound_facade_set_sound_volume(facade, 0.75f);
    check(settings.fp_sound_volume == 0.75f, "sound volume setter writes settings");
    check(ora::game::sound_facade_internal_sound_volume(facade) == 0.75f,
          "internal sound volume follows settings");
    ora::game::sound_facade_set_sound_volume_modifier(facade, 0.5f);
    check(ora::game::sound_facade_internal_sound_volume(facade) == 0.375f,
          "internal sound volume multiplies modifier");

    ora::game::sound_facade_set_video_volume(facade, 0.4f);
    check(settings.fp_video_volume == 0.4f, "video volume setter writes settings");

    ora::game::sound_facade_pause_music(facade);
    check(!ora::game::sound_facade_music_playing(facade), "pause clears playing flag");
    ora::game::sound_facade_play_music_resume(facade);
    check(ora::game::sound_facade_music_playing(facade), "resume restores playing flag");

    ora::game::sound_facade_stop_music(facade);
    check(!facade.opt_music.has_value() && !facade.opt_current_music.has_value(),
          "stop music clears music and current");
  }
  {
    ora::game::SoundSettingsModel settings{};
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    ora::game::support::MersenneTwister mt_cosmetic{ora::game::support::make_mersenne_twister(1)};
    ora::game::music_player player{make_player(settings, facade, mt_cosmetic, {})};
    check(player.b_is_music_available, "playlist available for installed music");
    check(player.vec_playlist.size() == 9, "hidden and missing tracks filtered out");
    check(playlist_keys(player.vec_playlist) == "t01,t02,t04,t05,t08,t09,t10,t11,t12",
          "playlist keeps library order");
    check(mt_cosmetic.int4_total_count == 8,
          "shuffle consumed exactly playlist size minus one rng draws");
    ora::game::support::MersenneTwister mt_replay{ora::game::support::make_mersenne_twister(1)};
    check(playlist_keys(ora::game::shuffle_playlist(player.vec_playlist, mt_replay)) ==
              playlist_keys(player.vec_random),
          "same seed reproduces shuffle order");
    const std::string str_first{player.vec_random.front().first};
    check(current_is(player, str_first), "initial song is first shuffled track");

    ora::game::music_player_play(player);
    check(current_is(player, str_first), "play starts current song");
    const std::vector<ora::platforms::sound_event>& vec_events{
        ora::platforms::engine_play_events(facade.engine)};
    const ora::game::music_info* ptr_first{ora::game::music_player_find(player, str_first)};
    check(ptr_first != nullptr && vec_events.size() == 1 &&
              vec_events[0].str_name == ptr_first->str_filename &&
              vec_events[0].f4_volume == 0.5f * ptr_first->f4_volume_modifier &&
              !vec_events[0].b_loop,
          "first play event records name volume loop");

    const std::string str_second{*ora::game::music_player_next_song(player)};
    check(str_second == *neighbor_key(player.vec_playlist, str_first, false),
          "next song follows playlist order");
    ora::platforms::engine_stream_finish(facade.engine, *facade.opt_music);
    ora::game::music_player_tick(player);
    check(current_is(player, str_second), "tick advances to next playlist track");
    const ora::game::music_info* ptr_second{ora::game::music_player_find(player, str_second)};
    check(ptr_second != nullptr && vec_events.size() == 2 &&
              vec_events[1].f4_volume == 0.5f * ptr_second->f4_volume_modifier,
          "second play volume multiplies track modifier");

    ora::game::music_player_play_track(player, "t02");
    check(ora::game::music_player_next_song(player).has_value() &&
              *ora::game::music_player_next_song(player) == "t04",
          "next song skips hidden tracks");
    check(ora::game::music_player_prev_song(player).has_value() &&
              *ora::game::music_player_prev_song(player) == "t01",
          "prev song returns to previous playlist track");
    ora::game::music_player_play_track(player, "t05");
    check(ora::game::music_player_next_song(player).has_value() &&
              *ora::game::music_player_next_song(player) == "t08",
          "next song skips hidden and missing tracks");

    ora::game::music_player_play_track(player, "t12");
    check(current_is(player, "t12"), "play track selects by key");
    ora::platforms::engine_stream_finish(facade.engine, *facade.opt_music);
    ora::game::music_player_tick(player);
    check(current_is(player, "t01"), "playlist wraps around at the end");

    ora::game::music_player_play_track(player, "t12");
    check(ora::game::music_player_prev_song(player).has_value() &&
              *ora::game::music_player_prev_song(player) == "t11",
          "prev song from last track steps back");
    ora::game::music_player_play_track(player, "t01");
    check(ora::game::music_player_prev_song(player).has_value() &&
              *ora::game::music_player_prev_song(player) == "t12",
          "prev song wraps around from first track");

    ora::game::music_player_stop(player);
    check(!player.str_current_song.has_value(), "stop clears current song");
    check(!ora::game::sound_facade_music_playing(facade), "stop stops playback");
    check(ora::game::music_player_next_song(player).has_value() &&
              *ora::game::music_player_next_song(player) == "t01",
          "next song after stop returns first track");

    const std::size_t sz_events_before{vec_events.size()};
    ora::game::music_player_stop(player);
    ora::game::music_player_play(player);
    check(!ora::game::sound_facade_music_playing(facade) &&
              vec_events.size() == sz_events_before,
          "play without current song after stop is idle");
  }
  {
    ora::game::SoundSettingsModel settings{};
    settings.b_repeat = true;
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    ora::game::support::MersenneTwister mt_cosmetic{ora::game::support::make_mersenne_twister(1)};
    ora::game::music_player player{make_player(settings, facade, mt_cosmetic, {})};
    const std::string str_first{player.vec_random.front().first};
    ora::game::music_player_play(player);
    const std::vector<ora::platforms::sound_event>& vec_events{
        ora::platforms::engine_play_events(facade.engine)};
    check(vec_events.size() == 1 && vec_events[0].b_loop, "repeat setting loops the stream");
    ora::game::music_player_tick(player);
    check(current_is(player, str_first),
          "looped stream never completes so tick does not advance");
    ora::platforms::engine_stream_finish(facade.engine, *facade.opt_music);
    ora::game::music_player_tick(player);
    check(!current_is(player, str_first), "tick after forced finish advances");
  }
  {
    ora::game::SoundSettingsModel settings{};
    settings.b_shuffle = true;
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    ora::game::support::MersenneTwister mt_cosmetic{ora::game::support::make_mersenne_twister(7)};
    ora::game::music_player player{make_player(settings, facade, mt_cosmetic, {})};
    ora::game::music_player_play_track(player, "t01");
    const std::optional<std::string> str_expected{
        neighbor_key(player.vec_random, "t01", false)};
    const std::optional<std::string> str_next{ora::game::music_player_next_song(player)};
    check(str_next.has_value() && str_expected.has_value() && *str_next == *str_expected,
          "shuffle mode next song follows shuffled order");
    check(str_next != std::optional<std::string>{"t02"} || *str_expected == "t02",
          "shuffle next song is not plain playlist order");
    bool b_in_playlist{false};
    for (const auto& [str_key, info] : player.vec_random)
      if (str_key == *str_next) b_in_playlist = true;
    check(b_in_playlist, "shuffle next song is a playlist member");
  }
  {
    ora::game::SoundSettingsModel settings{};
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    ora::game::support::MersenneTwister mt_cosmetic{ora::game::support::make_mersenne_twister(1)};
    ora::game::music_playlist_info info{};
    info.str_background_music = "t04";
    info.b_allow_mute_background_music = true;
    ora::game::music_player player{make_player(settings, facade, mt_cosmetic, info)};
    check(current_is(player, "t04") && player.b_current_song_is_background,
          "background music becomes initial current song");
    check(ora::game::music_player_next_song(player).has_value() &&
              *ora::game::music_player_next_song(player) == "t05",
          "next song from background track steps past it in playlist order");

    ora::game::music_player_play(player);
    check(ora::game::sound_facade_music_playing(facade), "background music plays");
    ora::platforms::engine_stream_finish(facade.engine, *facade.opt_music);
    ora::game::music_player_tick(player);
    check(current_is(player, "t04") && player.b_current_song_is_background,
          "background song replays without advancing");

    settings.b_mute_background_music = true;
    ora::game::music_player_stop(player);
    check(!ora::game::sound_facade_music_playing(facade),
          "stop with muted background music stays silent");
    check(current_is(player, "t04"), "stop keeps background song as current");
    settings.b_mute_background_music = false;
    ora::game::music_player_stop(player);
    check(ora::game::sound_facade_music_playing(facade),
          "stop restores background music when unmuted");

    ora::game::music_player_play_track(player, "t08");
    check(current_is(player, "t08") && !player.b_current_song_is_background,
          "explicit track selection leaves background mode");
    ora::game::music_player_stop(player);
    check(current_is(player, "t04") && player.b_current_song_is_background,
          "stop returns to background music");
  }
  {
    ora::game::SoundSettingsModel settings{};
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    ora::game::support::MersenneTwister mt_cosmetic{ora::game::support::make_mersenne_twister(1)};
    ora::game::music_playlist_info info{};
    info.str_starting_music = "t10";
    ora::game::music_player player{make_player(settings, facade, mt_cosmetic, info)};
    check(current_is(player, "t10") && !player.b_current_song_is_background,
          "starting music overrides random first track");
  }
  {
    ora::game::SoundSettingsModel settings{};
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    ora::game::support::MersenneTwister mt_cosmetic{ora::game::support::make_mersenne_twister(1)};
    std::vector<std::pair<std::string, ora::game::music_info>> vec_empty;
    ora::game::music_player player{
        ora::game::make_music_player(facade, settings, vec_empty, mt_cosmetic, {})};
    check(!player.b_is_music_available, "empty library is not available");
    check(!player.str_current_song.has_value(), "empty library has no current song");
    ora::game::music_player_play(player);
    check(ora::platforms::engine_play_events(facade.engine).empty(),
          "empty library play records nothing");
    check(!ora::game::music_player_next_song(player).has_value(),
          "empty library next song is null");
  }
  {
    ora::game::SoundSettingsModel settings{};
    ora::game::sound_facade facade{ora::game::make_sound_facade_settings("", false, settings)};
    std::vector<std::pair<std::string, ora::game::music_info>> vec_library{make_library()};
    ora::game::support::MersenneTwister mt_cosmetic{ora::game::support::make_mersenne_twister(3)};
    ora::game::support::MersenneTwister mt_shared{ora::game::support::make_mersenne_twister(3)};
    ora::game::music_playlist_info info{};
    ora::game::music_player player{
        ora::game::make_music_player(facade, settings, vec_library, mt_cosmetic, info)};
    const std::string str_random_order{playlist_keys(player.vec_random)};
    for (std::int32_t int4_i{0}; int4_i < 16; ++int4_i) ora::game::support::next_uint(mt_shared);
    check(playlist_keys(player.vec_random) == str_random_order,
          "playlist order untouched by unrelated rng draws");
    check(mt_shared.int4_total_count == 16,
          "cosmetic shuffle does not draw from shared rng");
    ora::game::support::MersenneTwister mt_other{ora::game::support::make_mersenne_twister(3)};
    ora::game::music_player player_b{
        ora::game::make_music_player(facade, settings, vec_library, mt_other, info)};
    check(playlist_keys(player_b.vec_random) == str_random_order,
          "same seed reproduces shuffle order");
  }
  {
    const std::string str_golden_dir{ORA_MUSIC_GOLDEN_DIR};
    const auto opt_yaml{ora::test::read_text_file(str_golden_dir + "/music.yaml")};
    check(opt_yaml.has_value(), "golden music yaml readable");
    auto res_library{ora::game::load_music_infos(str_golden_dir + "/music.yaml")};
    check(res_library.has_value(), "golden music library loads");
    if (res_library.has_value() && opt_yaml.has_value()) {
      namespace fs = std::filesystem;
      const fs::path fs_dir{fs::temp_directory_path() / "ora-music-golden-test"};
      std::error_code ec_fs{};
      fs::remove_all(fs_dir, ec_fs);
      check(fs::create_directories(fs_dir, ec_fs), "golden scenario temp dir created");
      for (const auto& [str_key, track] : *res_library) {
        if (str_key == "t06") continue;
        std::ofstream fs_file{fs_dir / track.str_filename, std::ios::binary};
      }
      ora::game::music_infos_probe_exists(*res_library, fs_dir.string());
      std::int32_t int4_installed{0};
      for (const auto& [str_key, track] : *res_library)
        if (track.b_exists) ++int4_installed;
      check(int4_installed == 11, "probe marks all pseudo files except t06");

      std::string str_playlist_dump{"playlist " + join_keys(playlist_of_library(*res_library)) +
                                    "\n"};
      for (const std::int32_t int4_seed : {1, 2, 3}) {
        ora::game::support::MersenneTwister mt_seed{
            ora::game::support::make_mersenne_twister(int4_seed)};
        const auto vec_shuffled{
            ora::game::shuffle_playlist(playlist_of_library(*res_library), mt_seed)};
        str_playlist_dump += std::format("seed {} draws {} random {}\n", int4_seed,
                                         mt_seed.int4_total_count, join_keys(vec_shuffled));
      }
      const auto opt_playlist_golden{
          ora::test::read_text_file(str_golden_dir + "/playlist-order.txt")};
      check(opt_playlist_golden.has_value(), "playlist order golden readable");
      if (opt_playlist_golden.has_value()) {
        const auto vec_golden{golden_data_lines(*opt_playlist_golden)};
        const auto vec_mine{golden_data_lines(str_playlist_dump)};
        check(vec_mine.size() == vec_golden.size(), "playlist order line count matches");
        for (std::size_t sz_i{0};
             sz_i < vec_mine.size() && sz_i < vec_golden.size(); ++sz_i)
          check(vec_mine[sz_i] == vec_golden[sz_i],
              std::format("playlist order line {} matches", sz_i));
      }

      music_scenario scenario_order{};
      scenario_order.vec_ops = {"play",       "complete", "next",     "prev",
                                "play-key t12", "complete", "play-key t01", "prev",
                                "stop",       "next",     "play",     "volume 0.25",
                                "play-key t02", "volume 0.3", "complete", "stop"};
      music_scenario scenario_shuffle{};
      scenario_shuffle.int4_seed = 2;
      scenario_shuffle.b_shuffle = true;
      scenario_shuffle.vec_ops = {"play",  "complete", "complete",     "complete", "next",
                                  "prev",  "stop",     "next",         "play-key t02",
                                  "complete"};
      music_scenario scenario_repeat{};
      scenario_repeat.int4_seed = 3;
      scenario_repeat.b_repeat = true;
      scenario_repeat.vec_ops = {"play", "complete", "play", "stop", "play", "complete"};
      music_scenario scenario_background{};
      scenario_background.str_background = "t04";
      scenario_background.b_allow_mute_background = true;
      scenario_background.vec_ops = {"play",
                                     "complete",
                                     "stop",
                                     "mute-background True",
                                     "play",
                                     "stop",
                                     "mute-background False",
                                     "stop",
                                     "play-key t08",
                                     "stop",
                                     "play"};

      const std::vector<std::pair<std::string, std::string>> vec_scenarios{
          {"scenario-order", run_music_scenario(scenario_order, *res_library)},
          {"scenario-shuffle", run_music_scenario(scenario_shuffle, *res_library)},
          {"scenario-repeat", run_music_scenario(scenario_repeat, *res_library)},
          {"scenario-background", run_music_scenario(scenario_background, *res_library)}};
      for (const auto& [str_name, str_dump] : vec_scenarios) {
        const auto opt_golden{ora::test::read_text_file(str_golden_dir + "/" + str_name + ".txt")};
        check(opt_golden.has_value(), std::format("{} golden readable", str_name));
        if (opt_golden.has_value()) {
          check(str_dump == *opt_golden, std::format("{} dump matches golden", str_name));
          if (str_dump != *opt_golden) {
            std::vector<std::string> vec_mine;
            std::size_t sz_pos{0};
            while (sz_pos < str_dump.size()) {
              const std::size_t idx_nl{str_dump.find('\n', sz_pos)};
              vec_mine.emplace_back(str_dump.substr(
                  sz_pos, (idx_nl == std::string::npos ? str_dump.size() : idx_nl) - sz_pos));
              if (idx_nl == std::string::npos) break;
              sz_pos = idx_nl + 1;
            }
            std::size_t sz_gold_pos{0};
            for (std::size_t sz_line{0};
                 sz_line < vec_mine.size() && sz_gold_pos <= opt_golden->size(); ++sz_line) {
              const std::size_t idx_gold_nl{opt_golden->find('\n', sz_gold_pos)};
              const std::string str_gold_line{opt_golden->substr(
                  sz_gold_pos,
                  (idx_gold_nl == std::string::npos ? opt_golden->size() : idx_gold_nl) -
                      sz_gold_pos)};
              if (vec_mine[sz_line] != str_gold_line) {
                std::println("diff {} line {}: mine=`{}` golden=`{}`", str_name, sz_line,
                             vec_mine[sz_line], str_gold_line);
                break;
              }
              if (idx_gold_nl == std::string::npos) break;
              sz_gold_pos = idx_gold_nl + 1;
            }
          }
        }
      }
      fs::remove_all(fs_dir, ec_fs);
    }
  }
  ora::platforms::openal_probe_override("");
  return ora::test::test_exit_code("music");
}

