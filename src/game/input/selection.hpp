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

void control_group_select(SelectionModel& selection, std::size_t idx_group);

}  // namespace ora::game::input
