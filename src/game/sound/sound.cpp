#include "game/sound/sound.hpp"

namespace ora::game {

namespace {

std::string fmt_float_r(float f4_value) {
  char buf_chars[32]{};
  const auto res_chars{std::to_chars(buf_chars, buf_chars + 32u, f4_value)};
  return std::string{buf_chars, res_chars.ptr};
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

void sound_facade_tick(sound_facade& facade) {
  if (!facade.b_music_playing || !facade.opt_music.has_value()) return;
  if (ora::platforms::sound_complete(facade.engine, *facade.opt_music)) {
    facade.b_music_playing = false;
    facade.opt_music.reset();
  }
}

void facade_set_tick(sound_facade& facade, std::int32_t int4_tick) {
  facade.int4_current_tick = int4_tick;
}

float facade_internal_sound_volume(const sound_facade& facade) {
  return facade.f4_sound_volume * facade.f4_sound_volume_modifier;
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
  const float f4_volume{facade_internal_sound_volume(facade) * f4_volume_modifier};
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
  const float f4_volume{facade_internal_sound_volume(facade)};
  return facade_play2d(facade, str_name, false, true, ora::platforms::wpos{}, f4_volume, true);
}

std::optional<ora::platforms::sound> sound_play_to_player(sound_facade& facade, sound_type kind,
                                                          std::uint32_t u4_player,
                                                          std::string_view str_name) {
  if (str_name.empty() || facade.b_disable_all_sounds ||
      (facade.b_disable_world_sounds && kind == sound_type::world))
    return std::nullopt;
  if (u4_player != facade.u4_local_player) return std::nullopt;
  const float f4_volume{facade_internal_sound_volume(facade)};
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
  const float f4_volume{facade_internal_sound_volume(facade)};
  return facade_play2d(facade, str_name, false, false, pos, f4_volume, true);
}

}
