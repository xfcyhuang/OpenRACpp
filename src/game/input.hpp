#pragma once
import std;

#include "platform/sdl2_input.hpp"

namespace ora {

enum class MouseActionType : std::uint8_t {
  Contextual,
  ConfirmOrder,
  GlobalCommand,
  SupportPower,
  PlaceBuilding,
};

enum class MouseControlStyle : std::uint8_t { Classic, Modern, OtherRTS };

enum class MouseScrollType : std::uint8_t { Disabled, Standard, Inverted, Joystick };

struct GameSettingsFace {
  MouseControlStyle mouse_control_style = MouseControlStyle::Modern;

  MouseButton ResolveActionButton(MouseActionType action) const {
    switch (action) {
      case MouseActionType::ConfirmOrder:
        return mouse_control_style == MouseControlStyle::Modern ? MouseButton::Right
                                                                : MouseButton::Left;
      case MouseActionType::Contextual:
        return mouse_control_style == MouseControlStyle::Classic ? MouseButton::Left
                                                                 : MouseButton::Right;
      default:
        return MouseButton::Left;
    }
  }

  MouseButton ResolveCancelButton(MouseActionType action) const {
    return ResolveActionButton(action) == MouseButton::Left ? MouseButton::Right
                                                            : MouseButton::Left;
  }
};

struct GameSettingsInputFace {
  bool b_viewport_edge_scroll = true;
  std::int32_t int4_viewport_edge_scroll_margin = 5;
  MouseControlStyle kind_mouse_control_style = MouseControlStyle::Modern;
  MouseScrollType kind_mouse_scroll = MouseScrollType::Joystick;
  float fp4_viewport_edge_scroll_step = 30.0f;
  float fp4_zoom_speed = 0.04f;
  std::int32_t int4_selection_deadzone = 24;
  std::int32_t int4_mouse_scroll_deadzone = 8;
  bool b_use_alternate_scroll_button = false;
  Modifiers kind_zoom_modifier = Modifiers::None;
};

}
