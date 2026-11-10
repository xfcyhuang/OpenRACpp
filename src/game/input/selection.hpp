#pragma once

import std;

#include "game/input/input_events.hpp"
#include "game/world/world.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::input {

constexpr std::size_t kControlGroupCount{10};

struct SelectionModel {
  std::vector<std::uint32_t> vec_actor_ids;
  std::array<std::vector<std::uint32_t>, kControlGroupCount> arr_groups;
};

void selection_combine(SelectionModel& selection, std::span<const std::uint32_t> vec_new,
                       bool b_combine, bool b_click);

void selection_clear(SelectionModel& selection);

void control_group_create(SelectionModel& selection, const world::WorldModel& w,
                          std::uint32_t u4_viewer_player, std::size_t idx_group);

void control_group_add_to(SelectionModel& selection, const world::WorldModel& w,
                          std::uint32_t u4_viewer_player, std::size_t idx_group);

// CombineSelectionWithControlGroup（ControlGroups.cs:74-77）：组内（in world）
// 成员并入当前选区（Combine true/false）。
void control_group_combine_with(SelectionModel& selection, const world::WorldModel& w,
                                std::size_t idx_group);

// RemoveFromControlGroup（ControlGroups.cs:84-89 + HotkeyLogic :34-44）：选中各 actor
// 从其所属（首个）组移除。
void control_group_remove_from(SelectionModel& selection);

void control_group_select(SelectionModel& selection, std::size_t idx_group);

}
