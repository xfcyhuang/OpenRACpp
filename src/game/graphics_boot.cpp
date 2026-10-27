import std;

#include "game/graphics_boot.hpp"

namespace ora::game {
namespace {

std::optional<std::size_t> CachedSlot(const SettingsModule* ptr_module,
                                      std::string_view sv_field,
                                      std::optional<std::size_t>& opt_cache) {
  if (!opt_cache.has_value())
    opt_cache = ptr_module != nullptr ? ptr_module->SlotIndexOf(sv_field)
                                      : std::optional<std::size_t>{};
  return opt_cache;
}

}

int2 GetGraphicsResolution(int int4_mode, int2 int2_fullscreen_size,
                           int2 int2_windowed_size) {
  return int4_mode == 0 ? int2_windowed_size : int2_fullscreen_size;
}

GraphicsBootDesc GraphicsBootFromSettings(const Settings& settings) {
  GraphicsBootDesc desc_result{};
  const SettingsModule* ptr_module = settings.Module("Graphics");
  if (ptr_module == nullptr)
    return desc_result;

  std::optional<std::size_t> opt_slot_mode{};
  std::optional<std::size_t> opt_slot_vsync{};
  std::optional<std::size_t> opt_slot_full{};
  std::optional<std::size_t> opt_slot_windowed{};
  std::optional<std::size_t> opt_slot_scale{};
  std::optional<std::size_t> opt_slot_cap{};
  std::optional<std::size_t> opt_slot_max{};
  std::optional<std::size_t> opt_slot_cap_game{};
  std::optional<std::size_t> opt_slot_display{};
  std::optional<std::size_t> opt_slot_profile{};

  const std::int64_t int8_mode =
      CachedSlot(ptr_module, "Mode", opt_slot_mode).has_value()
          ? ptr_module->GetInt(*opt_slot_mode)
          : 2;
  const int2 int2_full =
      CachedSlot(ptr_module, "FullscreenSize", opt_slot_full).has_value()
          ? ptr_module->GetInt2(*opt_slot_full)
          : int2{0, 0};
  const int2 int2_windowed =
      CachedSlot(ptr_module, "WindowedSize", opt_slot_windowed).has_value()
          ? ptr_module->GetInt2(*opt_slot_windowed)
          : int2{1024, 768};
  desc_result.int2_resolution = GetGraphicsResolution(
      static_cast<int>(int8_mode), int2_full, int2_windowed);

  switch (int8_mode) {
    case 0:
      desc_result.mode_window = platform::WindowMode::Windowed;
      break;
    case 1:
      desc_result.mode_window = platform::WindowMode::Fullscreen;
      break;
    default:
      desc_result.mode_window = platform::WindowMode::PseudoFullscreen;
      break;
  }

  if (const auto opt_slot = CachedSlot(ptr_module, "VSync", opt_slot_vsync))
    desc_result.b_vsync = ptr_module->GetBool(*opt_slot);
  if (const auto opt_slot = CachedSlot(ptr_module, "UIScale", opt_slot_scale))
    desc_result.fp4_ui_scale = ptr_module->GetFloat(*opt_slot);
  if (const auto opt_slot =
          CachedSlot(ptr_module, "CapFramerate", opt_slot_cap))
    desc_result.b_cap_framerate = ptr_module->GetBool(*opt_slot);
  if (const auto opt_slot =
          CachedSlot(ptr_module, "MaxFramerate", opt_slot_max))
    desc_result.int4_max_framerate =
        static_cast<std::int32_t>(ptr_module->GetInt(*opt_slot));
  if (const auto opt_slot =
          CachedSlot(ptr_module, "CapFramerateToGameFps", opt_slot_cap_game))
    desc_result.b_cap_framerate_to_game_fps = ptr_module->GetBool(*opt_slot);
  if (const auto opt_slot =
          CachedSlot(ptr_module, "VideoDisplay", opt_slot_display))
    desc_result.int4_video_display =
        static_cast<int>(ptr_module->GetInt(*opt_slot));

  const std::int64_t int8_profile =
      CachedSlot(ptr_module, "GLProfile", opt_slot_profile).has_value()
          ? ptr_module->GetInt(*opt_slot_profile)
          : 0;
  desc_result.kind_profile = int8_profile == 3
                                 ? platform::GLProfileKind::Embedded
                                 : platform::GLProfileKind::Modern;

  return desc_result;
}

bool SanitizeMaxFramerate(Settings& settings) {
  SettingsModule* ptr_module = settings.Module("Graphics");
  if (ptr_module == nullptr)
    return false;
  const std::optional<std::size_t> opt_slot_max =
      ptr_module->SlotIndexOf("MaxFramerate");
  if (!opt_slot_max.has_value())
    return false;
  if (ptr_module->GetInt(*opt_slot_max) >= 1)
    return false;

  ptr_module->SetInt(*opt_slot_max,
                     ptr_module->DefaultInt(*opt_slot_max));
  if (const std::optional<std::size_t> opt_slot_cap =
          ptr_module->SlotIndexOf("CapFramerate");
      opt_slot_cap.has_value())
    ptr_module->SetBool(*opt_slot_cap, false);
  return true;
}

GraphicsFramerateFace GraphicsFramerateFromSettings(const Settings& settings) {
  static std::optional<std::size_t> opt_slot_cap{};
  static std::optional<std::size_t> opt_slot_max{};
  static std::optional<std::size_t> opt_slot_cap_game{};

  GraphicsFramerateFace face_result{};
  const SettingsModule* ptr_module = settings.Module("Graphics");
  if (ptr_module == nullptr)
    return face_result;

  if (const auto opt_slot = CachedSlot(ptr_module, "CapFramerate", opt_slot_cap))
    face_result.b_cap_framerate = ptr_module->GetBool(*opt_slot);
  if (const auto opt_slot = CachedSlot(ptr_module, "MaxFramerate", opt_slot_max))
    face_result.int4_max_framerate =
        static_cast<std::int32_t>(ptr_module->GetInt(*opt_slot));
  if (const auto opt_slot =
          CachedSlot(ptr_module, "CapFramerateToGameFps", opt_slot_cap_game))
    face_result.b_cap_framerate_to_game_fps = ptr_module->GetBool(*opt_slot);
  return face_result;
}

}
