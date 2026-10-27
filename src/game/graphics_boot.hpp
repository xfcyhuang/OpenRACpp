#pragma once
import std;

#include "core/int2.hpp"
#include "game/settings.hpp"
#include "platform/sdl2_window.hpp"

namespace ora::game {

struct GraphicsBootDesc {
  int2 int2_resolution;
  platform::WindowMode mode_window = platform::WindowMode::Windowed;
  platform::GLProfileKind kind_profile = platform::GLProfileKind::Modern;
  int int4_video_display = 0;
  float fp4_ui_scale = 1.0f;
  bool b_vsync = true;
  bool b_cap_framerate = false;
  std::int32_t int4_max_framerate = 60;
  bool b_cap_framerate_to_game_fps = false;
};

int2 GetGraphicsResolution(int int4_mode, int2 int2_fullscreen_size,
                           int2 int2_windowed_size);

GraphicsBootDesc GraphicsBootFromSettings(const Settings& settings);

bool SanitizeMaxFramerate(Settings& settings);

struct GraphicsFramerateFace {
  bool b_cap_framerate = false;
  std::int32_t int4_max_framerate = 60;
  bool b_cap_framerate_to_game_fps = false;
};

GraphicsFramerateFace GraphicsFramerateFromSettings(const Settings& settings);

}
