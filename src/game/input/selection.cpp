#include "game/input/selection.hpp"

namespace ora::game::input {

namespace {

void sorted_unique(std::vector<std::uint32_t>& vec_ids) {
  std::sort(vec_ids.begin(), vec_ids.end());
  vec_ids.erase(std::unique(vec_ids.begin(), vec_ids.end()), vec_ids.end());
}

void groups_remove_selection(std::array<std::vector<std::uint32_t>, kControlGroupCount>& arr_groups,
                             const std::vector<std::uint32_t>& vec_ids) {
  for (std::vector<std::uint32_t>& vec_group : arr_groups)
    for (const std::uint32_t u4_id : vec_ids) {
      const auto it{std::find(vec_group.begin(), vec_group.end(), u4_id)};
      if (it != vec_group.end()) vec_group.erase(it);
    }
}

}

void selection_combine(SelectionModel& selection, std::span<const std::uint32_t> vec_new,
                       bool b_combine, bool b_click) {
  std::vector<std::uint32_t> vec_in{vec_new.begin(), vec_new.end()};
  if (b_click && vec_in.size() > 1) vec_in.resize(1);

  std::vector<std::uint32_t> vec_out;
  if (b_combine && b_click) {
    for (const std::uint32_t u4_id : selection.vec_actor_ids)
      if (std::find(vec_in.begin(), vec_in.end(), u4_id) == vec_in.end())
        vec_out.push_back(u4_id);
    for (const std::uint32_t u4_id : vec_in)
      if (std::find(selection.vec_actor_ids.begin(), selection.vec_actor_ids.end(), u4_id) ==
          selection.vec_actor_ids.end())
        vec_out.push_back(u4_id);
  } else if (b_combine) {
    vec_out = selection.vec_actor_ids;
    vec_out.insert(vec_out.end(), vec_in.begin(), vec_in.end());
  } else {
    vec_out = vec_in;
  }

  sorted_unique(vec_out);
  selection.vec_actor_ids = std::move(vec_out);
}

void selection_clear(SelectionModel& selection) { selection.vec_actor_ids.clear(); }

// 上游 CreateControlGroup（ControlGroups.cs:49-58）：空选区 no-op（不清组）。
void control_group_create(SelectionModel& selection, const world::WorldModel& w,
                          std::uint32_t u4_viewer_player, std::size_t idx_group) {
  if (idx_group >= kControlGroupCount) return;
  if (selection.vec_actor_ids.empty()) return;

  std::vector<std::uint32_t> vec_owned;
  for (const std::uint32_t u4_id : selection.vec_actor_ids) {
    if (u4_id >= w.vec_actors_all.size()) continue;
    if (w.vec_actors_all[u4_id].u4_owner == u4_viewer_player) vec_owned.push_back(u4_id);
  }

  selection.arr_groups[idx_group].clear();
  groups_remove_selection(selection.arr_groups, selection.vec_actor_ids);
  selection.arr_groups[idx_group] = std::move(vec_owned);
}

// 上游 AddSelectionToControlGroup（ControlGroups.cs:60-68）：空选区 no-op；
// 先把选中从所有组移除，再并入目标组（只收己方）。
void control_group_add_to(SelectionModel& selection, const world::WorldModel& w,
                          std::uint32_t u4_viewer_player, std::size_t idx_group) {
  if (idx_group >= kControlGroupCount) return;
  if (selection.vec_actor_ids.empty()) return;

  std::vector<std::uint32_t> vec_owned;
  for (const std::uint32_t u4_id : selection.vec_actor_ids) {
    if (u4_id >= w.vec_actors_all.size()) continue;
    if (w.vec_actors_all[u4_id].u4_owner == u4_viewer_player) vec_owned.push_back(u4_id);
  }

  groups_remove_selection(selection.arr_groups, selection.vec_actor_ids);

  std::vector<std::uint32_t> vec_group{selection.arr_groups[idx_group]};
  vec_group.insert(vec_group.end(), vec_owned.begin(), vec_owned.end());
  sorted_unique(vec_group);
  selection.arr_groups[idx_group] = std::move(vec_group);
}

void control_group_combine_with(SelectionModel& selection, const world::WorldModel& w,
                                std::size_t idx_group) {
  if (idx_group >= kControlGroupCount) return;
  std::vector<std::uint32_t> vec_members;
  for (const std::uint32_t u4_id : selection.arr_groups[idx_group]) {
    if (u4_id >= w.vec_actors_all.size()) continue;
    if (!w.vec_actors_all[u4_id].b_in_world) continue;
    vec_members.push_back(u4_id);
  }
  selection_combine(selection, vec_members, true, false);
}

void control_group_remove_from(SelectionModel& selection) {
  for (const std::uint32_t u4_id : selection.vec_actor_ids)
    for (std::vector<std::uint32_t>& vec_group : selection.arr_groups) {
      const auto it{std::find(vec_group.begin(), vec_group.end(), u4_id)};
      if (it != vec_group.end()) {
        vec_group.erase(it);
        break;
      }
    }
}

void control_group_select(SelectionModel& selection, std::size_t idx_group) {
  if (idx_group >= kControlGroupCount) return;
  selection_combine(selection, selection.arr_groups[idx_group], false, false);
}

}
