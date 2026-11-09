#pragma once

import std;

#include "platforms/sound_engine.hpp"

#include "game/music_info.hpp"
#include "game/settings.hpp"

namespace ora::game {

enum class sound_tick_event : std::uint8_t { k_none, k_music_complete };

constexpr std::uint32_t kSfxNoPlayer{0xffffffffu};

enum class sound_type : std::uint8_t { world, ui };

struct sound_facade {
  ora::platforms::sound_engine engine;
  bool b_music_playing{false};
  std::optional<ora::platforms::sound> opt_music{};
  std::optional<music_info> opt_current_music{};
  std::optional<ora::platforms::sound> opt_video{};
  SoundSettingsModel* ptr_settings{};
  float fp_sound_volume_modifier{1.0f};
  bool b_disable_all_sounds{false};
  bool b_disable_world_sounds{false};
  std::uint32_t u4_local_player{kSfxNoPlayer};
  std::int32_t int4_current_tick{0};
  std::map<std::string, ora::platforms::sound_source> map_sources;
  std::map<std::uint32_t, ora::platforms::sound> map_current_sounds;
  std::map<std::string, ora::platforms::sound> map_current_notifications;
  std::vector<std::string> vec_log;
};

[[nodiscard]] sound_facade make_sound_facade(std::string_view str_device, bool b_mute);
[[nodiscard]] sound_facade make_dummy_facade();

[[nodiscard]] sound_facade make_sound_facade_settings(std::string_view str_device, bool b_mute,
                                                      SoundSettingsModel& settings);

[[nodiscard]] float sound_facade_sound_volume(const sound_facade& facade);
[[nodiscard]] float sound_facade_music_volume(const sound_facade& facade);
[[nodiscard]] float sound_facade_video_volume(const sound_facade& facade);
[[nodiscard]] float sound_facade_internal_sound_volume(const sound_facade& facade);

void sound_facade_set_sound_volume(sound_facade& facade, float f4_volume);
void sound_facade_set_music_volume(sound_facade& facade, float f4_volume);
void sound_facade_set_video_volume(sound_facade& facade, float f4_volume);
void sound_facade_set_sound_volume_modifier(sound_facade& facade, float f4_modifier);

[[nodiscard]] bool sound_facade_music_playing(const sound_facade& facade);

void sound_facade_play_music_then(sound_facade& facade, const music_info& info);
void sound_facade_play_music(sound_facade& facade, const music_info& info, bool b_loop);
void sound_facade_play_music_resume(sound_facade& facade);
void sound_facade_stop_music(sound_facade& facade);
void sound_facade_pause_music(sound_facade& facade);

[[nodiscard]] sound_tick_event sound_facade_tick(sound_facade& facade);

void facade_set_tick(sound_facade& facade, std::int32_t int4_tick);

[[nodiscard]] std::string sfx_fmt_float_r(float f4_value);

[[nodiscard]] const ora::platforms::sound_source& facade_source_get(sound_facade& facade,
                                                                   std::string_view str_name);

[[nodiscard]] std::optional<ora::platforms::sound> facade_play2d(
    sound_facade& facade, std::string_view str_name, bool b_loop, bool b_relative,
    ora::platforms::wpos pos, float f4_volume, bool b_attenuate_volume);

void facade_stop_sound(sound_facade& facade, const ora::platforms::sound* ptr_snd);

[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name,
                                                              ora::platforms::wpos pos,
                                                              float f4_volume_modifier,
                                                              bool b_loop);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name,
                                                              ora::platforms::wpos pos,
                                                              float f4_volume_modifier);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name,
                                                              ora::platforms::wpos pos);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name);

[[nodiscard]] std::optional<ora::platforms::sound> sound_play_to_player(
    sound_facade& facade, sound_type kind, std::uint32_t u4_player, std::string_view str_name);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play_to_player(
    sound_facade& facade, sound_type kind, std::uint32_t u4_player, std::string_view str_name,
    ora::platforms::wpos pos);

}
