#pragma once

import std;

#include "game/input/hotkey.hpp"
#include "game/input/hotkey_registry.hpp"
#include "game/input/selection.hpp"
#include "game/input/selection_logic.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::input {

// ControlGroupsWidget.HandleKeyPress（ControlGroupsWidget.cs:128-171）键面：
// 逐组按 Select/Create/AddTo/CombineWith/JumpTo 顺序做注册表活查询（全匹配激活）。
// Select 命中且 KeyInput.MultiTapCount>=2 时记录视口跳转 hook（opt_center_group，
// 表现层据此 Center 组质心；JumpTo 同样只跳转不改选区）。
struct ControlGroupsKeyResult {
  bool b_handled{};
  std::optional<std::size_t> opt_center_group{};
};

[[nodiscard]] ControlGroupsKeyResult control_groups_handle_key(
    const HotkeyRegistryModel& reg, SelectionModel& selection, const world::WorldModel& w,
    std::uint32_t u4_viewer_player, const KeyInput& ki);

// SelectAllUnitsHotkeyLogic.OnHotkeyActivated：屏幕内（eligible 玩家、全 Class）
// 取最高优先组 → 未增则全图；替换 Combine。Down-only（SingleHotkeyBaseLogic）。
[[nodiscard]] bool hotkey_action_select_all_units(const HotkeyRegistryModel& reg,
                                                  const world::WorldModel& w,
                                                  std::uint32_t u4_viewer_player,
                                                  SelectionModel& selection, const KeyInput& ki,
                                                  const PxRect& rect_viewport);

// SelectUnitsByTypeHotkeyLogic.OnHotkeyActivated：以选中 actor 的 Class 集选屏幕
// 同类 → 未增则全图；并选 Combine。空选区/无 eligible 选中 → false。
[[nodiscard]] bool hotkey_action_select_units_by_type(const HotkeyRegistryModel& reg,
                                                      const world::WorldModel& w,
                                                      std::uint32_t u4_viewer_player,
                                                      SelectionModel& selection,
                                                      const KeyInput& ki,
                                                      const PxRect& rect_viewport);

// 移除选中（HotkeyManager 改键后的热键名消费示例面）：RemoveFromControlGroup。
[[nodiscard]] bool hotkey_action_remove_from_control_group(const HotkeyRegistryModel& reg,
                                                           SelectionModel& selection,
                                                           const KeyInput& ki);

}
