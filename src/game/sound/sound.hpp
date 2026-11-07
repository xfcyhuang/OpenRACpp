#pragma once

import std;

#include "platforms/sound_engine.hpp"

#include "game/music_info.hpp"
#include "game/settings.hpp"

namespace ora::game {

enum class sound_tick_event : std::uint8_t { k_none, k_music_complete };

struct sound_facade {
  ora::platforms::sound_engine engine;
  bool b_music_playing{false};
  std::optional<ora::platforms::sound> opt_music{};
  std::optional<music_info> opt_current_music{};
  std::optional<ora::platforms::sound> opt_video{};
  SoundSettingsModel* ptr_settings{};
  float fp_sound_volume_modifier{1.0f};
};

[[nodiscard]] sound_facade make_sound_facade(std::string_view str_device, bool b_mute);

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

}
