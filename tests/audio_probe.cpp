import std;

#include "game/sound/sound.hpp"
#include "test_util.hpp"
using ora::test::check;

int main() {
  namespace pl = ora::platforms;
  pl::sound_engine engine{pl::make_sound_engine("")};
  if (pl::engine_dummy(engine)) {
    std::println("audio_probe: OpenAL device unavailable - SKIPPED");
    return 77;
  }

  {
    const std::vector<pl::sound_device> vec_devices{pl::engine_available_devices(engine)};
    check(vec_devices.size() >= 1u, "openal lists at least the default device");
    if (!vec_devices.empty()) {
      check(!vec_devices[0].opt_device.has_value() && vec_devices[0].str_label == "Default Output",
            "first device is default output");
    }
  }
  {
    std::vector<std::uint8_t> vec_silence(2205u * 4u, 0u);
    const pl::sound_source src{
        pl::engine_add_sound_source_from_memory(engine, vec_silence, 1, 16, 22050)};
    check(src.b_valid, "openal source created");
    check(src.int4_sample_rate == 22050, "source sample rate kept");

    const std::optional<pl::sound> opt_snd{
        pl::engine_play_2d(engine, src, false, false, pl::wpos{}, 1.0f, true, 7)};
    check(opt_snd.has_value(), "play2d returns sound");
    if (opt_snd.has_value()) {
      check(!pl::sound_complete(engine, *opt_snd), "playing sound not complete immediately");
      const std::optional<float> opt_gain{pl::sound_volume_get(engine, *opt_snd)};
      check(opt_gain.has_value() && *opt_gain > 0.0f && *opt_gain <= 1.0f,
            "playing sound gain set");
      pl::engine_set_listener_position(engine, pl::wpos{1024, 2048, 0});
      pl::engine_pause_sound(engine, &*opt_snd, true);
      pl::engine_pause_sound(engine, &*opt_snd, false);
      pl::engine_set_sound_volume(engine, 0.25f, nullptr, nullptr);
      pl::engine_set_sound_looping(engine, true, &*opt_snd);
      pl::engine_set_sound_position(engine, &*opt_snd, pl::wpos{4096, 0, 0});
      pl::engine_stop_sound(engine, &*opt_snd);
      check(pl::sound_complete(engine, *opt_snd), "stopped sound completes");
    }

    const std::optional<pl::sound> opt_null{
        pl::engine_play_2d(engine, pl::sound_source{}, false, false, pl::wpos{}, 1.0f, false, 8)};
    check(!opt_null.has_value(), "null source play rejected");
  }
  {
    pl::engine_stop_all_sounds(engine);
    pl::engine_set_all_sounds_paused(engine, true);
    pl::engine_set_all_sounds_paused(engine, false);
  }
  pl::engine_dispose(engine);
  return ora::test::test_exit_code("audio_probe");
}
