// UPSTREAM: OpenRA.Game/Input/IInputHandler.cs L26(MouseInput record
//          struct)+ L38(Modifiers)+ Input/Keycode.cs(MouseButton)+
//          Settings.cs L29(MouseActionType)+ Settings.cs 的 GameSettings
//          按钮解析面(最小承载)
//          The minimal carrier of IInputHandler.cs L26 (the MouseInput
//          record struct) + L38 (Modifiers) + Input/Keycode.cs
//          (MouseButton) + Settings.cs L29 (MouseActionType) + the
//          button-resolution faces of Settings.cs's GameSettings.
//
// 机制对照 / Mechanism mapping:
//  - Modifiers:[Flags] None/Ctrl/Alt/Shift/Meta + HasModifier 位测
//    (IInputHandler.cs L38-46 的枚举面;Meta = L45 的 Modifier.Meta 承载)
//    Modifiers: the [Flags] enum face; Meta carries Modifier.Meta.
//  - MouseInput(Event/Button/Location/Modifiers)为 record struct 的字段子集
//    (Delta/MultiTapCount 随输入装配批)| a field subset of the record
//    struct (Delta/MultiTapCount land with the input-assembly batch).
//  - MouseActionType(Settings.cs L29 的五值枚举;UnitOrderGenerator 只消费
//    Contextual/None 两态)与 MouseControlStyle(Settings.cs GameSettings 面)
//    MouseActionType (the five-value enum of Settings.cs L29;
//    UnitOrderGenerator consumes only Contextual/None) and
//    MouseControlStyle (the GameSettings face of Settings.cs).
//  - GameSettings 的 ResolveActionButton/ResolveCancelButton(Settings.cs
//    L364-378:Classic 样式主键 = Contextual 左/其余右)→ 注入值面
//    (D101 Settings 批换实体)| the injected value faces.
#pragma once
import std;

#include "core/int2.hpp"

namespace ora {

/// Modifiers(Modifiers.cs) | Modifiers (Modifiers.cs).
enum class Modifiers : std::int32_t {
  None = 0,
  Ctrl = 1,
  Alt = 2,
  Shift = 4,
  Meta = 8,
};

constexpr Modifiers operator|(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(static_cast<std::int32_t>(a) |
                                static_cast<std::int32_t>(b));
}
constexpr bool HasModifier(Modifiers m, Modifiers flag) {
  return (static_cast<std::int32_t>(m) & static_cast<std::int32_t>(flag)) ==
         static_cast<std::int32_t>(flag);
}

/// MouseButton(MouseButton.cs:None/Left/Right/Middle) | MouseButton
/// (MouseButton.cs).
enum class MouseButton : std::uint8_t { None = 0, Left, Right, Middle };

/// MouseInputEvent(MouseInput.cs L~:Down/Up/Move) | MouseInputEvent.
enum class MouseInputEvent : std::uint8_t { Down, Up, Move };

/// MouseInput(MouseInput.cs) | MouseInput (MouseInput.cs).
struct MouseInput {
  MouseInputEvent Event = MouseInputEvent::Down;
  MouseButton Button = MouseButton::None;
  int2 Location{};
  Modifiers Modifiers = Modifiers::None;
};

/// MouseActionType(OrderGenerator.cs L~) | MouseActionType.
enum class MouseActionType : std::uint8_t {
  None = 0,
  Contextual,
  AttackMove,
  Attack,
  PanicMove,
  Guard,
};

/// MouseControlStyle(Settings.cs 的 GameSettings 面) | MouseControlStyle.
enum class MouseControlStyle : std::uint8_t { Classic, Modern };

/// GameSettings 的注入值面(OrderGenerator/UnitOrderGenerator 的按钮解析;
/// D101 Settings 批换实体)
/// The injected value faces of GameSettings (the button resolution of
/// OrderGenerator/UnitOrderGenerator; replaced by the real Settings in the
/// D101 batch).
struct GameSettingsFace {
  MouseControlStyle mouse_control_style = MouseControlStyle::Modern;
  MouseActionType primary_action = MouseActionType::AttackMove;   // 经典样式
                                                                  // 左键面.
  MouseActionType cancel_action = MouseActionType::None;

  /// ResolveActionButton(GameSettings.cs L~):按 Action 类型的按钮映射
  /// ResolveActionButton (the per-action-type button mapping).
  MouseButton ResolveActionButton(MouseActionType action) const {
    if (mouse_control_style == MouseControlStyle::Classic)
      return action == MouseActionType::None || action == MouseActionType::Contextual
                 ? MouseButton::Left
                 : MouseButton::Right;
    return MouseButton::Left;
  }
  MouseButton ResolveCancelButton(MouseActionType action) const {
    if (mouse_control_style == MouseControlStyle::Classic)
      return action == MouseActionType::None || action == MouseActionType::Contextual
                 ? MouseButton::Right
                 : MouseButton::Left;
    return MouseButton::Right;
  }
};

}  // namespace ora
