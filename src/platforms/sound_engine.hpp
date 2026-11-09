#pragma once

import std;

#include "platforms/openal_loader.hpp"

namespace ora::platforms {

struct wpos {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_z{};
};

struct sound_device {
  std::optional<std::string> opt_device;
  std::string str_label;
};

struct sound_state {
  std::uint32_t u4_al_source{};
  float f4_sample_rate{};
  bool b_done{};
  float f4_gain{1.0f};
};

struct sound {
  std::shared_ptr<sound_state> ptr_state{};
};

struct sound_source {
  std::uint32_t u4_al_buffer{};
  std::int32_t int4_sample_rate{};
  bool b_valid{};
  std::string str_name{};
};

enum class sound_engine_kind : std::uint8_t { dummy, openal };

struct pool_slot {
  bool b_is_active{};
  std::int32_t int4_frame_started{};
  wpos pos{};
  bool b_is_relative{};
  std::uint32_t u4_sound_buffer{};
  sound snd{};
};

struct openal_engine_state {
  openal_ops ops{};
  void* ptr_device{};
  void* ptr_context{};
  float f4_volume{1.0f};
  std::vector<std::pair<std::uint32_t, pool_slot>> vec_pool;
};

struct sound_event {
  std::string str_name{};
  float f4_volume{0.0f};
  bool b_loop{false};
};

struct dummy_engine_state {
  std::vector<sound_event> vec_play_events;
  std::int32_t int4_next_stream_id{1};
};

struct sound_engine {
  sound_engine_kind kind{};
  std::unique_ptr<openal_engine_state> ptr_state{};
  std::unique_ptr<dummy_engine_state> ptr_dummy{};
};

[[nodiscard]] sound_engine make_sound_engine(std::string_view str_device);
[[nodiscard]] sound_engine make_dummy_engine();
void engine_dispose(sound_engine& engine);

[[nodiscard]] bool engine_dummy(const sound_engine& engine);
[[nodiscard]] std::vector<sound_device> engine_available_devices(const sound_engine& engine);

[[nodiscard]] sound_source engine_add_sound_source_from_memory(
    sound_engine& engine, std::span<const std::uint8_t> span_data, std::int32_t int4_channels,
    std::int32_t int4_sample_bits, std::int32_t int4_sample_rate);

[[nodiscard]] std::optional<sound> engine_play_2d(sound_engine& engine,
                                                  const sound_source& src, bool b_loop,
                                                  bool b_relative, wpos pos, float f4_volume,
                                                  bool b_attenuate_volume,
                                                  std::int32_t int4_current_tick);

[[nodiscard]] std::optional<sound> engine_play_2d_stream(sound_engine& engine,
                                                         std::string_view str_name,
                                                         bool b_loop, float f4_volume);

[[nodiscard]] const std::vector<sound_event>& engine_play_events(const sound_engine& engine);

void engine_stream_finish(sound_engine& engine, const sound& snd);

[[nodiscard]] float engine_volume_get(const sound_engine& engine);
void engine_volume_set(sound_engine& engine, float f4_volume);

void engine_pause_sound(sound_engine& engine, const sound* ptr_snd, bool b_paused);
void engine_set_all_sounds_paused(sound_engine& engine, bool b_paused);
void engine_stop_sound(sound_engine& engine, const sound* ptr_snd);
void engine_stop_all_sounds(sound_engine& engine);
void engine_set_listener_position(sound_engine& engine, wpos pos);
void engine_set_sound_volume(sound_engine& engine, float f4_volume, const sound* ptr_music,
                             const sound* ptr_video);
void engine_set_sound_looping(sound_engine& engine, bool b_looping, const sound* ptr_snd);
void engine_set_sound_position(sound_engine& engine, const sound* ptr_snd, wpos pos);

[[nodiscard]] bool sound_complete(const sound_engine& engine, const sound& snd);
[[nodiscard]] std::optional<float> sound_seek_position(const sound_engine& engine,
                                                       const sound& snd);
[[nodiscard]] std::optional<float> sound_volume_get(const sound_engine& engine, const sound& snd);
void sound_volume_set(sound_engine& engine, const sound& snd, float f4_volume);
void sound_set_position(sound_engine& engine, const sound& snd, wpos pos);

}
