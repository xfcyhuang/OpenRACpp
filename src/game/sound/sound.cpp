#include "game/sound/sound.hpp"

namespace ora::game {

sound_facade make_sound_facade(std::string_view str_device, bool b_mute) {
  sound_facade facade{};
  facade.engine = ora::platforms::make_sound_engine(str_device);
  if (b_mute) ora::platforms::engine_volume_set(facade.engine, 0.0f);
  return facade;
}

void sound_facade_tick(sound_facade& facade) {
  if (!facade.b_music_playing || !facade.opt_music.has_value()) return;
  if (ora::platforms::sound_complete(facade.engine, *facade.opt_music)) {
    facade.b_music_playing = false;
    facade.opt_music.reset();
  }
}

}
