#include "game/sound/sound.hpp"

namespace ora::game {

namespace {

std::string fmt_float_r(float f4_value) {
  char buf_chars[32]{};
  const auto res_chars{std::to_chars(buf_chars, buf_chars + 32u, f4_value)};
  return std::string{buf_chars, res_chars.ptr};
}

float settings_music_volume(const sound_facade& facade) {
  return facade.ptr_settings == nullptr ? 0.5f : facade.ptr_settings->fp_music_volume;
}

bool same_music(const std::optional<music_info>& opt_current, const music_info& info) {
  return opt_current.has_value() && opt_current->str_filename == info.str_filename;
}

}

std::string sfx_fmt_float_r(float f4_value) { return fmt_float_r(f4_value); }

sound_facade make_sound_facade(std::string_view str_device, bool b_mute) {
  sound_facade facade{};
  facade.engine = ora::platforms::make_sound_engine(str_device);
  if (b_mute) ora::platforms::engine_volume_set(facade.engine, 0.0f);
  return facade;
}

sound_facade make_dummy_facade() {
  sound_facade facade{};
  facade.engine = ora::platforms::make_dummy_engine();
  return facade;
}

sound_facade make_sound_facade_settings(std::string_view str_device, bool b_mute,
                                        SoundSettingsModel& settings) {
  sound_facade facade{make_sound_facade(str_device, b_mute)};
  facade.ptr_settings = &settings;
  return facade;
}

float sound_facade_sound_volume(const sound_facade& facade) {
  return facade.ptr_settings == nullptr ? 0.5f : facade.ptr_settings->fp_sound_volume;
}

float sound_facade_music_volume(const sound_facade& facade) {
  return settings_music_volume(facade);
}

float sound_facade_video_volume(const sound_facade& facade) {
  return facade.ptr_settings == nullptr ? 0.5f : facade.ptr_settings->fp_video_volume;
}

float sound_facade_internal_sound_volume(const sound_facade& facade) {
  return sound_facade_sound_volume(facade) * facade.fp_sound_volume_modifier;
}

void sound_facade_set_sound_volume(sound_facade& facade, float f4_volume) {
  if (facade.ptr_settings != nullptr) facade.ptr_settings->fp_sound_volume = f4_volume;
  ora::platforms::engine_set_sound_volume(
      facade.engine, sound_facade_internal_sound_volume(facade),
      facade.opt_music.has_value() ? &*facade.opt_music : nullptr,
      facade.opt_video.has_value() ? &*facade.opt_video : nullptr);
}

void sound_facade_set_music_volume(sound_facade& facade, float f4_volume) {
  if (facade.ptr_settings != nullptr) facade.ptr_settings->fp_music_volume = f4_volume;
  if (facade.opt_music.has_value())
    ora::platforms::sound_volume_set(facade.engine, *facade.opt_music, f4_volume);
}

void sound_facade_set_video_volume(sound_facade& facade, float f4_volume) {
  if (facade.ptr_settings != nullptr) facade.ptr_settings->fp_video_volume = f4_volume;
  if (facade.opt_video.has_value())
    ora::platforms::sound_volume_set(facade.engine, *facade.opt_video, f4_volume);
}

void sound_facade_set_sound_volume_modifier(sound_facade& facade, float f4_modifier) {
  facade.fp_sound_volume_modifier = f4_modifier;
  ora::platforms::engine_set_sound_volume(
      facade.engine, sound_facade_internal_sound_volume(facade),
      facade.opt_music.has_value() ? &*facade.opt_music : nullptr,
      facade.opt_video.has_value() ? &*facade.opt_video : nullptr);
}

bool sound_facade_music_playing(const sound_facade& facade) {
  return facade.b_music_playing;
}

void sound_facade_play_music_then(sound_facade& facade, const music_info& info) {
  if (!info.b_exists) return;
  if (same_music(facade.opt_current_music, info) && facade.opt_music.has_value()) {
    ora::platforms::engine_pause_sound(facade.engine, &*facade.opt_music, false);
    facade.b_music_playing = true;
    return;
  }
  sound_facade_play_music(facade, info, facade.ptr_settings != nullptr &&
                                            facade.ptr_settings->b_repeat);
}

void sound_facade_play_music(sound_facade& facade, const music_info& info, bool b_loop) {
  if (!info.b_exists) return;
  sound_facade_stop_music(facade);
  const float f4_volume{settings_music_volume(facade) * info.f4_volume_modifier};
  std::optional<ora::platforms::sound> opt_snd{ora::platforms::engine_play_2d_stream(
      facade.engine, info.str_filename, b_loop, f4_volume)};
  if (!opt_snd.has_value()) return;
  facade.opt_music = std::move(opt_snd);
  facade.opt_current_music = info;
  facade.b_music_playing = true;
}

void sound_facade_play_music_resume(sound_facade& facade) {
  if (!facade.opt_music.has_value()) return;
  facade.b_music_playing = true;
  ora::platforms::engine_pause_sound(facade.engine, &*facade.opt_music, false);
}

void sound_facade_stop_music(sound_facade& facade) {
  if (facade.opt_music.has_value())
    ora::platforms::engine_stop_sound(facade.engine, &*facade.opt_music);
  facade.opt_music.reset();
  facade.opt_current_music.reset();
  facade.b_music_playing = false;
}

void sound_facade_pause_music(sound_facade& facade) {
  if (!facade.opt_music.has_value()) return;
  facade.b_music_playing = false;
  ora::platforms::engine_pause_sound(facade.engine, &*facade.opt_music, true);
}

sound_tick_event sound_facade_tick(sound_facade& facade) {
  if (!facade.b_music_playing || !facade.opt_music.has_value()) return sound_tick_event::k_none;
  if (!ora::platforms::sound_complete(facade.engine, *facade.opt_music))
    return sound_tick_event::k_none;
  sound_facade_stop_music(facade);
  return sound_tick_event::k_music_complete;
}

void facade_set_tick(sound_facade& facade, std::int32_t int4_tick) {
  facade.int4_current_tick = int4_tick;
}

const ora::platforms::sound_source& facade_source_get(sound_facade& facade,
                                                      std::string_view str_name) {
  const auto it{facade.map_sources.find(std::string{str_name})};
  if (it != facade.map_sources.end()) return it->second;
  const ora::platforms::sound_source src{
      .u4_al_buffer = 0, .int4_sample_rate = 0, .b_valid = false, .str_name = std::string{str_name}};
  return facade.map_sources.emplace(std::string{str_name}, src).first->second;
}

std::optional<ora::platforms::sound> facade_play2d(sound_facade& facade,
                                                   std::string_view str_name, bool b_loop,
                                                   bool b_relative, ora::platforms::wpos pos,
                                                   float f4_volume, bool b_attenuate_volume) {
  facade.vec_log.push_back(std::format("  play2d name={} pos={},{},{} loop={} rel={} vol={} atten={}",
                                       str_name, pos.int4_x, pos.int4_y, pos.int4_z,
                                       b_loop ? 1 : 0, b_relative ? 1 : 0, fmt_float_r(f4_volume),
                                       b_attenuate_volume ? 1 : 0));
  const ora::platforms::sound_source& src{facade_source_get(facade, str_name)};
  return ora::platforms::engine_play_2d(facade.engine, src, b_loop, b_relative, pos, f4_volume,
                                        b_attenuate_volume, facade.int4_current_tick);
}

void facade_stop_sound(sound_facade& facade, const ora::platforms::sound* ptr_snd) {
  facade.vec_log.push_back("  stopsound");
  ora::platforms::engine_stop_sound(facade.engine, ptr_snd);
}

std::optional<ora::platforms::sound> sound_play(sound_facade& facade, sound_type kind,
                                                std::string_view str_name,
                                                ora::platforms::wpos pos,
                                                float f4_volume_modifier, bool b_loop) {
  if (str_name.empty() || facade.b_disable_all_sounds ||
      (facade.b_disable_world_sounds && kind == sound_type::world))
    return std::nullopt;
  const float f4_volume{sound_facade_internal_sound_volume(facade) * f4_volume_modifier};
  return facade_play2d(facade, str_name, b_loop, false, pos, f4_volume, true);
}

std::optional<ora::platforms::sound> sound_play(sound_facade& facade, sound_type kind,
                                                std::string_view str_name,
                                                ora::platforms::wpos pos,
                                                float f4_volume_modifier) {
  return sound_play(facade, kind, str_name, pos, f4_volume_modifier, false);
}

std::optional<ora::platforms::sound> sound_play(sound_facade& facade, sound_type kind,
                                                std::string_view str_name,
                                                ora::platforms::wpos pos) {
  return sound_play(facade, kind, str_name, pos, 1.0f, false);
}

std::optional<ora::platforms::sound> sound_play(sound_facade& facade, sound_type kind,
                                                std::string_view str_name) {
  if (str_name.empty() || facade.b_disable_all_sounds ||
      (facade.b_disable_world_sounds && kind == sound_type::world))
    return std::nullopt;
  const float f4_volume{sound_facade_internal_sound_volume(facade)};
  return facade_play2d(facade, str_name, false, true, ora::platforms::wpos{}, f4_volume, true);
}

std::optional<ora::platforms::sound> sound_play_to_player(sound_facade& facade, sound_type kind,
                                                          std::uint32_t u4_player,
                                                          std::string_view str_name) {
  if (str_name.empty() || facade.b_disable_all_sounds ||
      (facade.b_disable_world_sounds && kind == sound_type::world))
    return std::nullopt;
  if (u4_player != facade.u4_local_player) return std::nullopt;
  const float f4_volume{sound_facade_internal_sound_volume(facade)};
  return facade_play2d(facade, str_name, false, true, ora::platforms::wpos{}, f4_volume, true);
}

std::optional<ora::platforms::sound> sound_play_to_player(sound_facade& facade, sound_type kind,
                                                          std::uint32_t u4_player,
                                                          std::string_view str_name,
                                                          ora::platforms::wpos pos) {
  if (str_name.empty() || facade.b_disable_all_sounds ||
      (facade.b_disable_world_sounds && kind == sound_type::world))
    return std::nullopt;
  if (u4_player != facade.u4_local_player) return std::nullopt;
  const float f4_volume{sound_facade_internal_sound_volume(facade)};
  return facade_play2d(facade, str_name, false, false, pos, f4_volume, true);
}

}
