// UPSTREAM: OpenRA.Game/Renderer.cs @1f594e320 L88-122(Renderer ctor 的
//          GraphicSettings 消费:GetResolution + CreateWindow 六参)+
//          OpenRA.Game/Game.cs L827-829(Loop 的帧率帽)+ L901-904(Run 的
//          MaxFramerate<1 修正)+ Sdl2PlatformWindow.cs L173-174/234
//          (videoDisplay 域校验与居中)。
//          GraphicSettings' startup consumption distilled for the embedder:
//          GetResolution + the CreateWindow six parameters, the Loop
//          framerate cap, and Run's MaxFramerate<1 fixup, plus the
//          videoDisplay domain check and centering.
//
// 形态适配:C++ 无 Game.Initialize 单体装配 —— 本文件是调用方(未来 main/
// 测试)从 Settings 到 Sdl2Window::Desc 的装配纯函数族。GLProfile 的
// Automatic/ANGLE 档映射 Modern(C++ 窗口域无 ANGLE 探测链;上游
// supportedProfiles 回退 supported[0] 的简化;登记 COVERAGE)。
// Shape adaptation: C++ has no monolithic Game.Initialize assembly — this
// file is the caller's (the future main/tests) pure-function family mapping
// Settings onto Sdl2Window::Desc. GLProfile's Automatic/ANGLE tiers map to
// Modern (the C++ window domain has no ANGLE probe chain; a simplification
// of upstream's supportedProfiles[0] fallback; registered in COVERAGE).
#pragma once
import std;

#include "core/int2.hpp"
#include "game/settings.hpp"
#include "platform/sdl2_window.hpp"

namespace ora::game {

/// 启动装配值(上游 Renderer ctor L88-98 消费的 GraphicSettings 子集 +
/// Loop L827-829 的帧率帽三字段)。
/// The startup-assembly values (the GraphicSettings subset consumed by the
/// Renderer ctor L88-98 + the Loop L827-829 framerate-cap trio).
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

/// GetResolution(Renderer.cs L116-122):Windowed 取 WindowedSize,否则
/// FullscreenSize(WindowMode 枚举值:Windowed=0)。
/// GetResolution (Renderer.cs L116-122): WindowedSize when Windowed, else
/// FullscreenSize (the WindowMode enum value: Windowed=0).
int2 GetGraphicsResolution(int int4_mode, int2 int2_fullscreen_size,
                           int2 int2_windowed_size);

/// Settings.Graphics 段 → 启动装配值(窗口尺寸由调用方按
/// GetGraphicsResolution 决定后的整装形态)。
/// The Settings.Graphics section → the startup-assembly values (the packed
/// form after the caller resolves the window size via
/// GetGraphicsResolution).
GraphicsBootDesc GraphicsBootFromSettings(const Settings& settings);

/// Game.Run 的修正(Game.cs L901-904):MaxFramerate < 1 → 默认值 +
/// CapFramerate = false。返回是否发生修正。
/// Game.Run's fixup (Game.cs L901-904): MaxFramerate < 1 → the default and
/// CapFramerate = false. Returns whether a fixup happened.
bool SanitizeMaxFramerate(Settings& settings);

/// Loop 每帧的帧率帽三字段(Game.cs L827-829 的读序;槽索引静态缓存 ——
/// RecordDesc 编译期恒定)。
/// The per-frame framerate-cap trio of Loop (Game.cs L827-829's read order;
/// slot indices cached statically — RecordDesc is compile-time constant).
struct GraphicsFramerateFace {
  bool b_cap_framerate = false;
  std::int32_t int4_max_framerate = 60;
  bool b_cap_framerate_to_game_fps = false;
};

GraphicsFramerateFace GraphicsFramerateFromSettings(const Settings& settings);

}  // namespace ora::game
