#pragma once

import std;

#include "platforms/sound_engine.hpp"

namespace ora::game {

struct sound_facade {
  ora::platforms::sound_engine engine;
  bool b_music_playing{false};
  std::optional<ora::platforms::sound> opt_music{};
};

[[nodiscard]] sound_facade make_sound_facade(std::string_view str_device, bool b_mute);

void sound_facade_tick(sound_facade& facade);

}
