module ora.game.input;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;

namespace ora::game::input {

namespace {

std::string group_hotkey_name(std::string_view str_prefix, std::size_t idx_group) {
  return std::format("{}{:02}", str_prefix, idx_group + 1);
}

}

ControlGroupsKeyResult control_groups_handle_key(const HotkeyRegistryModel& reg,
                                                 SelectionModel& selection,
                                                 const world::WorldModel& w,
                                                 std::uint32_t u4_viewer_player,
                                                 const KeyInput& ki) {
  for (std::size_t idx{0}; idx < kControlGroupCount; ++idx) {
    if (hotkey_is_activated_by(hotkey_resolve(reg, group_hotkey_name("ControlGroupSelect", idx)),
                               ki)) {
      control_group_select(selection, idx);
      ControlGroupsKeyResult result;
      result.b_handled = true;
      if (ki.int4_tap_count >= 2) result.opt_center_group = idx;
      return result;
    }
    if (hotkey_is_activated_by(hotkey_resolve(reg, group_hotkey_name("ControlGroupCreate", idx)),
                               ki)) {
      control_group_create(selection, w, u4_viewer_player, idx);
      ControlGroupsKeyResult result;
      result.b_handled = true;
      return result;
    }
    if (hotkey_is_activated_by(hotkey_resolve(reg, group_hotkey_name("ControlGroupAddTo", idx)),
                               ki)) {
      control_group_add_to(selection, w, u4_viewer_player, idx);
      ControlGroupsKeyResult result;
      result.b_handled = true;
      return result;
    }
    if (hotkey_is_activated_by(
            hotkey_resolve(reg, group_hotkey_name("ControlGroupCombineWith", idx)), ki)) {
      control_group_combine_with(selection, w, idx);
      ControlGroupsKeyResult result;
      result.b_handled = true;
      return result;
    }
    if (hotkey_is_activated_by(hotkey_resolve(reg, group_hotkey_name("ControlGroupJumpTo", idx)),
                               ki)) {
      ControlGroupsKeyResult result;
      result.b_handled = true;
      result.opt_center_group = idx;
      return result;
    }
  }
  return {};
}

bool hotkey_action_select_all_units(const HotkeyRegistryModel& reg, const world::WorldModel& w,
                                    std::uint32_t u4_viewer_player, SelectionModel& selection,
                                    const KeyInput& ki, const PxRect& rect_viewport) {
  if (hotkey_is_activated_by(hotkey_resolve(reg, "SelectAllUnits"), ki)) {
    const std::vector<std::uint32_t> vec_eligible{
        players_to_include_in_selection(w, u4_viewer_player)};
    std::vector<std::uint32_t> vec_new{subset_with_highest_selection_priority(
        w, u4_viewer_player, select_actors_on_screen(w, vec_eligible, {}, rect_viewport),
        ki.mods)};
    if (vec_new.size() <= selection.vec_actor_ids.size())
      vec_new = subset_with_highest_selection_priority(
          w, u4_viewer_player, select_actors_in_world(w, vec_eligible, {}), ki.mods);
    selection_combine(selection, vec_new, false, false);
    return true;
  }
  return false;
}

bool hotkey_action_select_units_by_type(const HotkeyRegistryModel& reg, const world::WorldModel& w,
                                        std::uint32_t u4_viewer_player, SelectionModel& selection,
                                        const KeyInput& ki, const PxRect& rect_viewport) {
  if (!hotkey_is_activated_by(hotkey_resolve(reg, "SelectUnitsByType"), ki)) return false;
  if (selection.vec_actor_ids.empty()) return false;

  std::vector<std::uint32_t> vec_eligible{players_to_include_in_selection(w, u4_viewer_player)};

  std::vector<std::uint32_t> vec_owned;
  for (const std::uint32_t u4_id : selection.vec_actor_ids) {
    if (u4_id >= w.vec_actors_all.size()) continue;
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.b_disposed || !actor.b_in_world) continue;
    bool b_eligible{false};
    for (const std::uint32_t u4_player : vec_eligible)
      if (u4_player == actor.u4_owner) b_eligible = true;
    if (b_eligible) vec_owned.push_back(u4_id);
  }
  if (vec_owned.empty()) return false;

  std::vector<std::string> vec_classes;
  for (const std::uint32_t u4_id : vec_owned)
    vec_classes.push_back(selectable_class(w, w.vec_actors_all[u4_id]));

  std::vector<std::uint32_t> vec_new{select_actors_on_screen(w, vec_eligible, vec_classes,
                                                             rect_viewport)};
  if (vec_new.size() <= selection.vec_actor_ids.size())
    vec_new = select_actors_in_world(w, vec_eligible, vec_classes);
  selection_combine(selection, vec_new, true, false);
  return true;
}

bool hotkey_action_remove_from_control_group(const HotkeyRegistryModel& reg,
                                             SelectionModel& selection, const KeyInput& ki) {
  if (!hotkey_is_activated_by(hotkey_resolve(reg, "RemoveFromControlGroup"), ki)) return false;
  control_group_remove_from(selection);
  return true;
}

}
