#include "sound_engine.hpp"

namespace ora::platforms {

namespace {

constexpr std::int32_t kAlNoError{0};
constexpr std::int32_t kAlFormatMono8{0x1100};
constexpr std::int32_t kAlFormatMono16{0x1101};
constexpr std::int32_t kAlFormatStereo8{0x1102};
constexpr std::int32_t kAlFormatStereo16{0x1103};
constexpr std::int32_t kAlPosition{0x1004};
constexpr std::int32_t kAlOrientation{0x100F};
constexpr std::int32_t kAlVelocity{0x1006};
constexpr std::int32_t kAlPitch{0x1003};
constexpr std::int32_t kAlGain{0x100A};
constexpr std::int32_t kAlLooping{0x1007};
constexpr std::int32_t kAlBuffer{0x1009};
constexpr std::int32_t kAlSourceState{0x1010};
constexpr std::int32_t kAlInitial{0x1011};
constexpr std::int32_t kAlPlaying{0x1012};
constexpr std::int32_t kAlPaused{0x1013};
constexpr std::int32_t kAlStopped{0x1014};
constexpr std::int32_t kAlSourceRelative{0x202};
constexpr std::int32_t kAlReferenceDistance{0x1020};
constexpr std::int32_t kAlMaxDistance{0x1023};
constexpr std::int32_t kAlSampleOffset{0x1025};
constexpr std::int32_t kAlcDeviceSpecifier{0x1005};
constexpr std::int32_t kAlcAllDevicesSpecifier{0x1013};
constexpr std::int32_t kAlMetersPerUnit{0x20004};

constexpr std::int32_t kMaxInstancesPerFrame{3};
constexpr std::int32_t kGroupDistance{2730};
constexpr std::int64_t kGroupDistanceSqr{static_cast<std::int64_t>(kGroupDistance) * kGroupDistance};
constexpr std::int32_t kPoolSize{256};

constexpr std::uint32_t kUnboundSource{0xffffffffu};

std::int32_t make_al_format(std::int32_t int4_channels, std::int32_t int4_bits) {
  if (int4_channels == 1) return int4_bits == 16 ? kAlFormatMono16 : kAlFormatMono8;
  return int4_bits == 16 ? kAlFormatStereo16 : kAlFormatStereo8;
}

std::int64_t wpos_length_squared(wpos pos) {
  return static_cast<std::int64_t>(pos.int4_x) * pos.int4_x +
         static_cast<std::int64_t>(pos.int4_y) * pos.int4_y +
         static_cast<std::int64_t>(pos.int4_z) * pos.int4_z;
}

wpos wpos_sub(wpos pos_a, wpos pos_b) {
  return wpos{pos_a.int4_x - pos_b.int4_x, pos_a.int4_y - pos_b.int4_y,
              pos_a.int4_z - pos_b.int4_z};
}

const openal_ops& state_ops(const sound_engine& engine) {
  return engine.ptr_state->ops;
}

std::vector<std::string> query_devices(const sound_engine& engine, std::string_view str_label,
                                       std::int32_t int4_type) {
  std::vector<std::string> vec_devices;
  const openal_ops& ops{state_ops(engine)};
  ops.fn_al_get_error();

  const char* chr_list{ops.fn_alc_get_string(nullptr, int4_type)};
  if (chr_list == nullptr || ops.fn_al_get_error() != kAlNoError) {
    std::println("Failed to query OpenAL device list using {}", str_label);
    return vec_devices;
  }

  std::string str_current;
  for (const char* chr_p{chr_list};; ++chr_p) {
    if (*chr_p != '\0') {
      str_current.push_back(*chr_p);
      continue;
    }
    vec_devices.push_back(str_current);
    str_current.clear();
    if (chr_p[1] == '\0') break;
  }
  return vec_devices;
}

std::vector<std::string> physical_devices(const sound_engine& engine) {
  const openal_ops& ops{state_ops(engine)};
  if (ops.fn_alc_is_extension_present(nullptr, "ALC_ENUMERATE_ALL_EXT"))
    return query_devices(engine, "ALC_ENUMERATE_ALL_EXT", kAlcAllDevicesSpecifier);
  if (ops.fn_alc_is_extension_present(nullptr, "ALC_ENUMERATION_EXT"))
    return query_devices(engine, "ALC_ENUMERATION_EXT", kAlcDeviceSpecifier);
  return {};
}

void unbind_sound(sound_state& snd_state) {
  snd_state.b_done = true;
  snd_state.u4_al_source = kUnboundSource;
}

std::optional<std::uint32_t> try_get_source_from_pool(sound_engine& engine) {
  openal_engine_state& state{*engine.ptr_state};
  for (std::size_t sz_i{0}; sz_i < state.vec_pool.size(); ++sz_i) {
    auto& [u4_key, slot] = state.vec_pool[sz_i];
    if (!slot.b_is_active) {
      slot.b_is_active = true;
      return u4_key;
    }
  }

  std::vector<std::size_t> vec_free;
  for (std::size_t sz_i{0}; sz_i < state.vec_pool.size(); ++sz_i) {
    auto& [u4_key, slot] = state.vec_pool[sz_i];
    if (slot.snd.ptr_state == nullptr) continue;
    sound_state& snd_state{*slot.snd.ptr_state};
    bool b_complete{false};
    if (snd_state.b_done) {
      b_complete = true;
    } else {
      std::int32_t int4_state{0};
      state.ops.fn_al_get_source_i(u4_key, kAlSourceState, &int4_state);
      b_complete = int4_state == kAlStopped;
    }
    if (!b_complete) continue;

    vec_free.push_back(sz_i);
    state.ops.fn_al_source_rewind(u4_key);
    state.ops.fn_al_source_i(u4_key, kAlBuffer, 0);
    unbind_sound(snd_state);
    slot.u4_sound_buffer = 0;
    slot.snd = sound{};
    slot.b_is_active = false;
  }

  if (vec_free.empty()) return std::nullopt;
  state.vec_pool[vec_free[0]].second.b_is_active = true;
  return state.vec_pool[vec_free[0]].first;
}

std::shared_ptr<sound_state> make_openal_sound(sound_engine& engine, std::uint32_t u4_source,
                                               bool b_loop, bool b_relative, wpos pos,
                                               float f4_volume, float f4_sample_rate) {
  openal_engine_state& state{*engine.ptr_state};
  auto ptr_state{std::make_shared<sound_state>()};
  ptr_state->u4_al_source = u4_source;
  ptr_state->f4_sample_rate = f4_sample_rate;
  ptr_state->b_done = false;

  state.ops.fn_al_source_f(u4_source, kAlPitch, 1.0f);
  state.ops.fn_al_source_3f(u4_source, kAlPosition, static_cast<float>(pos.int4_x),
                            static_cast<float>(pos.int4_y), static_cast<float>(pos.int4_z));
  state.ops.fn_al_source_3f(u4_source, kAlVelocity, 0.0f, 0.0f, 0.0f);
  state.ops.fn_al_source_i(u4_source, kAlLooping, b_loop ? 1 : 0);
  state.ops.fn_al_source_i(u4_source, kAlSourceRelative, b_relative ? 1 : 0);
  state.ops.fn_al_source_f(u4_source, kAlReferenceDistance, 6826.0f);
  state.ops.fn_al_source_f(u4_source, kAlMaxDistance, 136533.0f);
  state.ops.fn_al_source_f(u4_source, kAlGain, f4_volume);
  return ptr_state;
}

void pause_source(sound_engine& engine, std::uint32_t u4_source, bool b_paused) {
  openal_engine_state& state{*engine.ptr_state};
  std::int32_t int4_state{0};
  state.ops.fn_al_get_source_i(u4_source, kAlSourceState, &int4_state);
  if (b_paused) {
    if (int4_state == kAlPlaying) state.ops.fn_al_source_pause(u4_source);
    else if (int4_state == kAlInitial) {
      state.ops.fn_al_source_play(u4_source);
      state.ops.fn_al_source_stop(u4_source);
    }
  } else if (int4_state != kAlPlaying) {
    state.ops.fn_al_source_play(u4_source);
  }
}

void stop_openal_sound(sound_engine& engine, const sound& snd) {
  if (snd.ptr_state == nullptr) return;
  sound_state& snd_state{*snd.ptr_state};
  if (snd_state.b_done) return;
  openal_engine_state& state{*engine.ptr_state};
  std::int32_t int4_state{0};
  state.ops.fn_al_get_source_i(snd_state.u4_al_source, kAlSourceState, &int4_state);
  if (int4_state == kAlPlaying || int4_state == kAlPaused)
    state.ops.fn_al_source_stop(snd_state.u4_al_source);
}

}

sound_engine make_dummy_engine() {
  sound_engine engine{};
  engine.kind = sound_engine_kind::dummy;
  engine.ptr_state = nullptr;
  engine.ptr_dummy = std::make_unique<dummy_engine_state>();
  return engine;
}

sound_engine make_sound_engine(std::string_view str_device) {
  auto res_ops{load_openal()};
  if (!res_ops.has_value()) {
    std::println("sound: OpenAL load failed: {} — using dummy engine", res_ops.error());
    return make_dummy_engine();
  }

  if (str_device.empty()) std::println("Using default sound device");
  else std::println("Using sound device `{}`", str_device);

  sound_engine engine{};
  engine.kind = sound_engine_kind::openal;
  engine.ptr_state = std::make_unique<openal_engine_state>();
  openal_engine_state& state{*engine.ptr_state};
  state.ops = *res_ops;

  const char* chr_device{str_device.empty() ? nullptr : str_device.data()};
  std::string str_device_buffer;
  if (chr_device != nullptr) {
    str_device_buffer = std::string{str_device};
    chr_device = str_device_buffer.c_str();
  }
  state.ptr_device = state.ops.fn_alc_open_device(chr_device);
  if (state.ptr_device == nullptr) {
    std::println("Failed to open device. Falling back to default");
    state.ptr_device = state.ops.fn_alc_open_device(nullptr);
    if (state.ptr_device == nullptr) {
      std::println("sound: can't create OpenAL device — using dummy engine");
      engine.kind = sound_engine_kind::dummy;
      engine.ptr_state = nullptr;
      engine.ptr_dummy = std::make_unique<dummy_engine_state>();
      return engine;
    }
  }

  state.ptr_context = state.ops.fn_alc_create_context(state.ptr_device, nullptr);
  if (state.ptr_context == nullptr) {
    std::println("sound: can't create OpenAL context — using dummy engine");
    state.ops.fn_alc_close_device(state.ptr_device);
    state.ptr_device = nullptr;
    engine.kind = sound_engine_kind::dummy;
    engine.ptr_state = nullptr;
    engine.ptr_dummy = std::make_unique<dummy_engine_state>();
    return engine;
  }
  state.ops.fn_alc_make_context_current(state.ptr_context);

  for (std::int32_t int4_i{0}; int4_i < kPoolSize; ++int4_i) {
    std::uint32_t u4_source{0};
    state.ops.fn_al_gen_sources(1, &u4_source);
    if (state.ops.fn_al_get_error() != kAlNoError) {
      std::println("Failed generating OpenAL source {}", int4_i);
      return engine;
    }
    state.vec_pool.emplace_back(u4_source, pool_slot{});
  }
  return engine;
}

void engine_dispose(sound_engine& engine) {
  if (engine.kind != sound_engine_kind::openal || engine.ptr_state == nullptr) {
    engine.ptr_state = nullptr;
    return;
  }
  openal_engine_state& state{*engine.ptr_state};
  engine_stop_all_sounds(engine);
  if (!state.vec_pool.empty()) {
    std::vector<std::uint32_t> vec_keys;
    vec_keys.reserve(state.vec_pool.size());
    for (const auto& [u4_key, slot] : state.vec_pool) vec_keys.push_back(u4_key);
    state.ops.fn_al_delete_sources(static_cast<std::int32_t>(vec_keys.size()), vec_keys.data());
  }
  state.vec_pool.clear();
  if (state.ptr_context != nullptr) {
    state.ops.fn_alc_make_context_current(nullptr);
    state.ops.fn_alc_destroy_context(state.ptr_context);
    state.ptr_context = nullptr;
  }
  if (state.ptr_device != nullptr) {
    state.ops.fn_alc_close_device(state.ptr_device);
    state.ptr_device = nullptr;
  }
  engine.ptr_state = nullptr;
}

bool engine_dummy(const sound_engine& engine) {
  return engine.kind == sound_engine_kind::dummy;
}

std::vector<sound_device> engine_available_devices(const sound_engine& engine) {
  if (engine.kind == sound_engine_kind::dummy) {
    std::vector<sound_device> vec_devices;
    vec_devices.push_back(sound_device{std::nullopt, "No Sound Output"});
    return vec_devices;
  }

  std::vector<sound_device> vec_devices;
  vec_devices.push_back(sound_device{std::nullopt, "Default Output"});
  for (const std::string& str_device : physical_devices(engine))
    vec_devices.push_back(sound_device{str_device, str_device});
  return vec_devices;
}

sound_source engine_add_sound_source_from_memory(sound_engine& engine,
                                                 std::span<const std::uint8_t> span_data,
                                                 std::int32_t int4_channels,
                                                 std::int32_t int4_sample_bits,
                                                 std::int32_t int4_sample_rate) {
  if (engine.kind == sound_engine_kind::dummy) return sound_source{};
  openal_engine_state& state{*engine.ptr_state};
  std::uint32_t u4_buffer{0};
  state.ops.fn_al_gen_buffers(1, &u4_buffer);
  state.ops.fn_al_buffer_data(u4_buffer, make_al_format(int4_channels, int4_sample_bits),
                              span_data.data(), static_cast<std::int32_t>(span_data.size()),
                              int4_sample_rate);
  return sound_source{u4_buffer, int4_sample_rate, true};
}

std::optional<sound> engine_play_2d(sound_engine& engine, const sound_source& src, bool b_loop,
                                    bool b_relative, wpos pos, float f4_volume,
                                    bool b_attenuate_volume, std::int32_t int4_current_tick) {
  if (engine.kind == sound_engine_kind::dummy) return sound{};

  openal_engine_state& state{*engine.ptr_state};
  if (!src.b_valid) {
    std::println("Attempt to Play2D a null `ISoundSource`");
    return std::nullopt;
  }

  float f4_atten{1.0f};
  if (b_attenuate_volume) {
    std::int32_t int4_instances{0};
    std::int32_t int4_active_count{0};
    for (const auto& [u4_key, slot] : state.vec_pool) {
      if (!slot.b_is_active) continue;
      if (slot.b_is_relative != b_relative) continue;
      ++int4_active_count;
      if (slot.u4_sound_buffer != src.u4_al_buffer) continue;
      if (int4_current_tick - slot.int4_frame_started >= 5) continue;
      const std::int64_t int4_lensqr{wpos_length_squared(wpos_sub(slot.pos, pos))};
      if (int4_lensqr >= kGroupDistanceSqr) continue;
      ++int4_instances;
      if (int4_instances == kMaxInstancesPerFrame) return std::nullopt;
    }
    f4_atten = 0.66f * ((256.0f - static_cast<float>(int4_active_count) * 0.5f) / 256.0f);
  }

  const std::optional<std::uint32_t> opt_source{try_get_source_from_pool(engine)};
  if (!opt_source.has_value()) return std::nullopt;

  for (auto& [u4_key, slot] : state.vec_pool) {
    if (u4_key != *opt_source) continue;
    slot.pos = pos;
    slot.int4_frame_started = int4_current_tick;
    slot.b_is_relative = b_relative;
    slot.u4_sound_buffer = src.u4_al_buffer;
    sound snd{};
    snd.ptr_state = make_openal_sound(engine, *opt_source, b_loop, b_relative, pos,
                                      f4_volume * f4_atten,
                                      static_cast<float>(src.int4_sample_rate));
    state.ops.fn_al_source_i(*opt_source, kAlBuffer, static_cast<std::int32_t>(src.u4_al_buffer));
    state.ops.fn_al_source_play(*opt_source);
    slot.snd = snd;
    return snd;
  }
  return std::nullopt;
}

std::optional<sound> engine_play_2d_stream(sound_engine& engine, std::string_view str_name,
                                           bool b_loop, float f4_volume) {
  if (engine.kind == sound_engine_kind::dummy) {
    dummy_engine_state& state{*engine.ptr_dummy};
    state.vec_play_events.push_back(
        sound_event{std::string{str_name}, f4_volume, b_loop});
    auto ptr_snd_state{std::make_shared<sound_state>()};
    ptr_snd_state->u4_al_source = static_cast<std::uint32_t>(state.int4_next_stream_id);
    ptr_snd_state->b_done = false;
    ptr_snd_state->f4_gain = f4_volume;
    ++state.int4_next_stream_id;
    sound snd{};
    snd.ptr_state = std::move(ptr_snd_state);
    return snd;
  }
  std::println("music: stream playback not supported on OpenAL engine (M2 face)");
  return std::nullopt;
}

const std::vector<sound_event>& engine_play_events(const sound_engine& engine) {
  static const std::vector<sound_event> vec_empty;
  if (engine.kind != sound_engine_kind::dummy || engine.ptr_dummy == nullptr)
    return vec_empty;
  return engine.ptr_dummy->vec_play_events;
}

void engine_stream_finish([[maybe_unused]] sound_engine& engine, const sound& snd) {
  if (snd.ptr_state == nullptr) return;
  snd.ptr_state->b_done = true;
}

float engine_volume_get(const sound_engine& engine) {
  if (engine.kind == sound_engine_kind::dummy) return 0.0f;
  return engine.ptr_state->f4_volume;
}

void engine_volume_set(sound_engine& engine, float f4_volume) {
  if (engine.kind == sound_engine_kind::dummy) return;
  openal_engine_state& state{*engine.ptr_state};
  state.f4_volume = f4_volume;
  state.ops.fn_al_listener_f(kAlGain, f4_volume);
}

void engine_pause_sound(sound_engine& engine, const sound* ptr_snd, bool b_paused) {
  if (engine.kind == sound_engine_kind::dummy) return;
  if (ptr_snd == nullptr || ptr_snd->ptr_state == nullptr) return;
  if (sound_complete(engine, *ptr_snd)) return;
  pause_source(engine, ptr_snd->ptr_state->u4_al_source, b_paused);
}

void engine_set_all_sounds_paused(sound_engine& engine, bool b_paused) {
  if (engine.kind == sound_engine_kind::dummy) return;
  openal_engine_state& state{*engine.ptr_state};
  for (const auto& [u4_key, slot] : state.vec_pool) pause_source(engine, u4_key, b_paused);
}

void engine_stop_sound(sound_engine& engine, const sound* ptr_snd) {
  if (engine.kind == sound_engine_kind::dummy) return;
  if (ptr_snd == nullptr) return;
  if (ptr_snd->ptr_state == nullptr) return;
  sound_state& snd_state{*ptr_snd->ptr_state};
  if (snd_state.b_done) return;
  openal_engine_state& state{*engine.ptr_state};
  stop_openal_sound(engine, *ptr_snd);
  state.ops.fn_al_source_i(snd_state.u4_al_source, kAlBuffer, 0);
}

void engine_stop_all_sounds(sound_engine& engine) {
  if (engine.kind == sound_engine_kind::dummy) return;
  openal_engine_state& state{*engine.ptr_state};
  for (auto& [u4_key, slot] : state.vec_pool) {
    if (slot.snd.ptr_state == nullptr) continue;
    stop_openal_sound(engine, slot.snd);
    state.ops.fn_al_source_i(slot.snd.ptr_state->u4_al_source, kAlBuffer, 0);
  }
}

void engine_set_listener_position(sound_engine& engine, wpos pos) {
  if (engine.kind == sound_engine_kind::dummy) return;
  openal_engine_state& state{*engine.ptr_state};
  state.ops.fn_al_listener_3f(kAlPosition, static_cast<float>(pos.int4_x),
                              static_cast<float>(pos.int4_y),
                              static_cast<float>(pos.int4_z + 2133));
  const std::array<float, 6> arr_orientation{0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f};
  state.ops.fn_al_listener_fv(kAlOrientation, arr_orientation.data());
  state.ops.fn_al_listener_f(kAlMetersPerUnit, 0.01f);
}

void engine_set_sound_volume(sound_engine& engine, float f4_volume, const sound* ptr_music,
                             const sound* ptr_video) {
  if (engine.kind == sound_engine_kind::dummy) return;
  openal_engine_state& state{*engine.ptr_state};
  for (const auto& [u4_key, slot] : state.vec_pool) {
    std::int32_t int4_state{0};
    state.ops.fn_al_get_source_i(u4_key, kAlSourceState, &int4_state);
    if (int4_state != kAlPlaying && int4_state != kAlPaused) continue;
    if (ptr_music != nullptr && ptr_music->ptr_state != nullptr &&
        ptr_music->ptr_state->u4_al_source == u4_key)
      continue;
    if (ptr_video != nullptr && ptr_video->ptr_state != nullptr &&
        ptr_video->ptr_state->u4_al_source == u4_key)
      continue;
    state.ops.fn_al_source_f(u4_key, kAlGain, f4_volume);
  }
}

void engine_set_sound_looping(sound_engine& engine, bool b_looping, const sound* ptr_snd) {
  if (engine.kind == sound_engine_kind::dummy) return;
  if (ptr_snd == nullptr || ptr_snd->ptr_state == nullptr) return;
  sound_state& snd_state{*ptr_snd->ptr_state};
  if (snd_state.b_done) return;
  openal_engine_state& state{*engine.ptr_state};
  state.ops.fn_al_source_i(snd_state.u4_al_source, kAlLooping, b_looping ? 1 : 0);
}

void engine_set_sound_position(sound_engine& engine, const sound* ptr_snd, wpos pos) {
  if (engine.kind == sound_engine_kind::dummy) return;
  if (ptr_snd == nullptr || ptr_snd->ptr_state == nullptr) return;
  sound_state& snd_state{*ptr_snd->ptr_state};
  if (snd_state.b_done) return;
  openal_engine_state& state{*engine.ptr_state};
  state.ops.fn_al_source_3f(snd_state.u4_al_source, kAlPosition,
                            static_cast<float>(pos.int4_x), static_cast<float>(pos.int4_y),
                            static_cast<float>(pos.int4_z));
}

bool sound_complete(const sound_engine& engine, const sound& snd) {
  if (snd.ptr_state == nullptr) return false;
  const sound_state& snd_state{*snd.ptr_state};
  if (snd_state.b_done) return true;
  if (engine.kind == sound_engine_kind::dummy) return false;
  std::int32_t int4_state{0};
  engine.ptr_state->ops.fn_al_get_source_i(snd_state.u4_al_source, kAlSourceState, &int4_state);
  return int4_state == kAlStopped;
}

std::optional<float> sound_seek_position(const sound_engine& engine, const sound& snd) {
  if (snd.ptr_state == nullptr) return 0.0f;
  const sound_state& snd_state{*snd.ptr_state};
  if (snd_state.b_done) return std::nullopt;
  if (engine.kind == sound_engine_kind::dummy) return std::nullopt;
  std::int32_t int4_sample_offset{0};
  engine.ptr_state->ops.fn_al_get_source_i(snd_state.u4_al_source, kAlSampleOffset,
                                           &int4_sample_offset);
  return static_cast<float>(int4_sample_offset) / snd_state.f4_sample_rate;
}

std::optional<float> sound_volume_get(const sound_engine& engine, const sound& snd) {
  if (snd.ptr_state == nullptr) return 0.0f;
  const sound_state& snd_state{*snd.ptr_state};
  if (snd_state.b_done) return std::nullopt;
  if (engine.kind == sound_engine_kind::dummy) return snd_state.f4_gain;
  float f4_gain{0.0f};
  engine.ptr_state->ops.fn_al_get_source_f(snd_state.u4_al_source, kAlGain, &f4_gain);
  return f4_gain;
}

void sound_volume_set(sound_engine& engine, const sound& snd, float f4_volume) {
  if (snd.ptr_state == nullptr) return;
  sound_state& snd_state{*snd.ptr_state};
  if (snd_state.b_done) return;
  if (engine.kind == sound_engine_kind::dummy) {
    snd_state.f4_gain = f4_volume;
    return;
  }
  engine.ptr_state->ops.fn_al_source_f(snd_state.u4_al_source, kAlGain, f4_volume);
}

void sound_set_position(sound_engine& engine, const sound& snd, wpos pos) {
  if (snd.ptr_state == nullptr) return;
  sound_state& snd_state{*snd.ptr_state};
  if (snd_state.b_done) return;
  if (engine.kind == sound_engine_kind::dummy) return;
  engine.ptr_state->ops.fn_al_source_3f(snd_state.u4_al_source, kAlPosition,
                                        static_cast<float>(pos.int4_x),
                                        static_cast<float>(pos.int4_y),
                                        static_cast<float>(pos.int4_z));
}

}
