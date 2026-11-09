#pragma once

import std;

#include "platforms/sound_engine.hpp"

namespace ora::game {

constexpr std::uint32_t kSfxNoPlayer{0xffffffffu};

enum class sound_type : std::uint8_t { world, ui };

struct sound_facade {
  ora::platforms::sound_engine engine;
  bool b_music_playing{false};
  std::optional<ora::platforms::sound> opt_music{};
  bool b_disable_all_sounds{false};
  bool b_disable_world_sounds{false};
  float f4_sound_volume{0.5f};
  float f4_sound_volume_modifier{1.0f};
  std::uint32_t u4_local_player{kSfxNoPlayer};
  std::int32_t int4_current_tick{0};
  std::map<std::string, ora::platforms::sound_source> map_sources;
  std::map<std::uint32_t, ora::platforms::sound> map_current_sounds;
  std::map<std::string, ora::platforms::sound> map_current_notifications;
  std::vector<std::string> vec_log;
};

[[nodiscard]] sound_facade make_sound_facade(std::string_view str_device, bool b_mute);
[[nodiscard]] sound_facade make_dummy_facade();

void sound_facade_tick(sound_facade& facade);

void facade_set_tick(sound_facade& facade, std::int32_t int4_tick);

[[nodiscard]] std::string sfx_fmt_float_r(float f4_value);

[[nodiscard]] float facade_internal_sound_volume(const sound_facade& facade);

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
