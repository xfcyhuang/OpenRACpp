#include "game/input/selection_logic.hpp"

#include <algorithm>
#include <cctype>

#include "game/input/screen_map.hpp"
#include "game/world/trait.hpp"

namespace ora::game::input {

namespace {

const world::TraitModel* actor_trait_find(const world::WorldModel& w,
                                          const world::ActorModel& actor,
                                          std::string_view str_kind) {
  for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
    const world::TraitModel& trait{w.vec_traits[u4_handle]};
    if (trait.str_kind == str_kind) return &trait;
  }
  return nullptr;
}

bool string_list_contains(std::span<const std::string> vec_list, std::string_view str_value) {
  for (const std::string& str_entry : vec_list)
    if (str_entry == str_value) return true;
  return false;
}

}

std::int32_t pixel_distance(std::int32_t int4_dx, std::int32_t int4_dy) {
  return static_cast<std::int32_t>(std::sqrt(static_cast<double>(static_cast<std::int64_t>(int4_dx) * int4_dx +
                                                                static_cast<std::int64_t>(int4_dy) * int4_dy)));
}

SelectionRelationship selection_relationship(const world::WorldModel& w,
                                             std::uint32_t u4_viewer_player,
                                             std::uint32_t u4_owner_player) {
  if (u4_owner_player == u4_viewer_player) return SelectionRelationship::kSelf;
  if (u4_owner_player < w.vec_players.size() &&
      w.vec_players[u4_owner_player].b_non_combatant)
    return SelectionRelationship::kNeutral;
  return SelectionRelationship::kEnemy;
}

SelectionPriorityModifiers selectable_priority_modifiers(const world::WorldModel& w,
                                                         const world::ActorModel& actor) {
  const world::TraitModel* ptr_selectable{actor_trait_find(w, actor, "Selectable")};
  if (ptr_selectable == nullptr) return SelectionPriorityModifiers::kNone;

  SelectionPriorityModifiers mods{SelectionPriorityModifiers::kNone};
  std::size_t idx_begin{0};
  const std::string& str_value{ptr_selectable->str_param3};
  while (idx_begin <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', idx_begin)};
    std::string_view str_token{std::string_view{str_value}.substr(
        idx_begin, idx_comma == std::string::npos ? std::string_view::npos
                                                  : idx_comma - idx_begin)};
    while (!str_token.empty() && (str_token.front() == ' ' || str_token.front() == '\t'))
      str_token.remove_prefix(1);
    while (!str_token.empty() && (str_token.back() == ' ' || str_token.back() == '\t'))
      str_token.remove_suffix(1);
    if (str_token == "ctrl") mods = mods | SelectionPriorityModifiers::kCtrl;
    if (str_token == "alt") mods = mods | SelectionPriorityModifiers::kAlt;
    if (idx_comma == std::string::npos) break;
    idx_begin = idx_comma + 1;
  }
  return mods;
}

std::string selectable_class(const world::WorldModel& w, const world::ActorModel& actor) {
  const world::TraitModel* ptr_selectable{actor_trait_find(w, actor, "Selectable")};
  if (ptr_selectable != nullptr && !ptr_selectable->str_param2.empty())
    return ptr_selectable->str_param2;
  std::string str_class{actor.str_type};
  std::transform(str_class.begin(), str_class.end(), str_class.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return str_class;
}

// 上游 BaseSelectionPriority（SelectableExts.cs:50-61）：Ctrl/Alt 越权互斥
// （Ctrl&&!Alt 或 Alt&&!Ctrl 且 actor 的 PriorityModifiers 含对应 flag）→ int.MaxValue；
// Ctrl+Alt 同按两个条件都不成立 → 普通优先级。
std::int32_t selection_priority(const world::WorldModel& w, const world::ActorModel& actor,
                                std::uint32_t u4_viewer_player, ModifierKeys mods) {
  const world::TraitModel* ptr_selectable{actor_trait_find(w, actor, "Selectable")};
  if (ptr_selectable == nullptr) return std::numeric_limits<std::int32_t>::min();

  std::int32_t int4_base{ptr_selectable->int4_param};
  if (int4_base == 0) int4_base = 10;
  const SelectionPriorityModifiers priority_mods{selectable_priority_modifiers(w, actor)};
  const bool b_ctrl{has_modifier(mods, ModifierKeys::kCtrl)};
  const bool b_alt{has_modifier(mods, ModifierKeys::kAlt)};
  if (b_ctrl && !b_alt &&
      (priority_mods & SelectionPriorityModifiers::kCtrl) != SelectionPriorityModifiers::kNone)
    int4_base = std::numeric_limits<std::int32_t>::max();
  if (b_alt && !b_ctrl &&
      (priority_mods & SelectionPriorityModifiers::kAlt) != SelectionPriorityModifiers::kNone)
    int4_base = std::numeric_limits<std::int32_t>::max();

  switch (selection_relationship(w, u4_viewer_player, actor.u4_owner)) {
    case SelectionRelationship::kSelf:
      return int4_base;
    case SelectionRelationship::kNeutral:
      return int4_base - 2 * 30;
    case SelectionRelationship::kEnemy:
      return int4_base - 3 * 30;
  }
  return int4_base;
}

std::optional<std::uint32_t> select_highest_priority_at_point(const world::WorldModel& w,
                                                              std::uint32_t u4_viewer_player,
                                                              std::int32_t int4_x,
                                                              std::int32_t int4_y,
                                                              ModifierKeys mods) {
  const std::vector<ActorBoundsPair> vec_hits{actors_at_mouse(w, int4_x, int4_y)};

  std::optional<std::uint32_t> opt_best;
  std::int64_t int8_best{std::numeric_limits<std::int64_t>::min()};
  for (const ActorBoundsPair& hit : vec_hits) {
    const world::ActorModel& actor{w.vec_actors_all[hit.u4_actor_id]};
    const std::int32_t int4_priority{selection_priority(w, actor, u4_viewer_player, mods)};
    const std::int32_t int4_cx{hit.rect_bounds.int4_x + hit.rect_bounds.int4_w / 2};
    const std::int32_t int4_cy{hit.rect_bounds.int4_y + hit.rect_bounds.int4_h / 2};
    const std::int32_t int4_dist{pixel_distance(int4_cx - int4_x, int4_cy - int4_y)};
    const std::int64_t int8_score{
        static_cast<std::int64_t>(int4_priority) -
        (static_cast<std::int64_t>(int4_dist) << 16)};
    if (int8_score > int8_best) {
      int8_best = int8_score;
      opt_best = hit.u4_actor_id;
    }
  }
  return opt_best;
}

std::vector<std::uint32_t> select_actors_in_box(const world::WorldModel& w,
                                                std::uint32_t u4_viewer_player,
                                                std::int32_t int4_ax, std::int32_t int4_ay,
                                                std::int32_t int4_bx, std::int32_t int4_by,
                                                ModifierKeys mods) {
  if (pixel_distance(int4_ax - int4_bx, int4_ay - int4_by) <= kSelectionDeadzone) {
    const auto opt_picked{select_highest_priority_at_point(w, u4_viewer_player, int4_ax, int4_ay,
                                                            mods)};
    if (opt_picked.has_value()) return std::vector<std::uint32_t>{*opt_picked};
    return {};
  }

  const PxRect rect{std::min(int4_ax, int4_bx), std::min(int4_ay, int4_by),
                    std::abs(int4_ax - int4_bx), std::abs(int4_ay - int4_by)};
  const std::vector<ActorBoundsPair> vec_hits{actors_in_box(w, rect)};

  std::int32_t int4_best{std::numeric_limits<std::int32_t>::min()};
  for (const ActorBoundsPair& hit : vec_hits) {
    const world::ActorModel& actor{w.vec_actors_all[hit.u4_actor_id]};
    const std::int32_t int4_priority{selection_priority(w, actor, u4_viewer_player, mods)};
    int4_best = std::max(int4_best, int4_priority);
  }

  std::vector<std::uint32_t> vec_out;
  for (const ActorBoundsPair& hit : vec_hits) {
    const world::ActorModel& actor{w.vec_actors_all[hit.u4_actor_id]};
    if (selection_priority(w, actor, u4_viewer_player, mods) == int4_best)
      vec_out.push_back(hit.u4_actor_id);
  }
  return vec_out;
}

std::vector<std::uint32_t> select_actors_on_screen(
    const world::WorldModel& w, std::span<const std::uint32_t> vec_eligible_players,
    std::span<const std::string> vec_selection_classes, const PxRect& rect_viewport) {
  std::vector<std::uint32_t> vec_out;
  for (const ActorBoundsPair& hit : actors_in_box(w, rect_viewport)) {
    const world::ActorModel& actor{w.vec_actors_all[hit.u4_actor_id]};
    if (actor_trait_find(w, actor, "Selectable") == nullptr) continue;

    bool b_owner_hit{false};
    for (const std::uint32_t u4_player : vec_eligible_players)
      if (u4_player == actor.u4_owner) b_owner_hit = true;
    if (!b_owner_hit) continue;

    if (!vec_selection_classes.empty() &&
        !string_list_contains(vec_selection_classes, selectable_class(w, actor)))
      continue;
    vec_out.push_back(hit.u4_actor_id);
  }
  return vec_out;
}

std::vector<std::uint32_t> select_actors_in_world(
    const world::WorldModel& w, std::span<const std::uint32_t> vec_eligible_players,
    std::span<const std::string> vec_selection_classes) {
  std::vector<std::uint32_t> vec_out;
  for (const std::uint32_t u4_id : w.vec_in_world) {
    if (u4_id >= w.vec_actors_all.size()) continue;
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor_trait_find(w, actor, "Selectable") == nullptr) continue;

    bool b_owner_hit{false};
    for (const std::uint32_t u4_player : vec_eligible_players)
      if (u4_player == actor.u4_owner) b_owner_hit = true;
    if (!b_owner_hit) continue;

    if (!vec_selection_classes.empty() &&
        !string_list_contains(vec_selection_classes, selectable_class(w, actor)))
      continue;
    vec_out.push_back(u4_id);
  }
  return vec_out;
}

std::vector<std::uint32_t> subset_with_highest_selection_priority(
    const world::WorldModel& w, std::uint32_t u4_viewer_player,
    std::span<const std::uint32_t> vec_ids, ModifierKeys mods) {
  std::int32_t int4_best{std::numeric_limits<std::int32_t>::min()};
  for (const std::uint32_t u4_id : vec_ids)
    int4_best = std::max(int4_best,
                         selection_priority(w, w.vec_actors_all[u4_id], u4_viewer_player, mods));

  std::vector<std::uint32_t> vec_out;
  for (const std::uint32_t u4_id : vec_ids)
    if (selection_priority(w, w.vec_actors_all[u4_id], u4_viewer_player, mods) == int4_best)
      vec_out.push_back(u4_id);
  return vec_out;
}

std::vector<std::uint32_t> players_to_include_in_selection(const world::WorldModel& w,
                                                           std::uint32_t u4_viewer_player) {
  if (u4_viewer_player < w.vec_players.size()) {
    const world::PlayerModel& player{w.vec_players[u4_viewer_player]};
    if (!player.b_non_combatant || !player.b_spectating)
      return std::vector<std::uint32_t>{u4_viewer_player};
  }
  std::vector<std::uint32_t> vec_all;
  for (std::uint32_t u4_idx{0}; u4_idx < w.vec_players.size(); ++u4_idx) vec_all.push_back(u4_idx);
  return vec_all;
}

std::vector<std::uint32_t> select_by_type_double_click(const world::WorldModel& w,
                                                       std::uint32_t u4_viewer_player,
                                                       std::int32_t int4_x, std::int32_t int4_y,
                                                       ModifierKeys mods,
                                                       const PxRect& rect_viewport) {
  const auto opt_unit{select_highest_priority_at_point(w, u4_viewer_player, int4_x, int4_y, mods)};
  if (!opt_unit.has_value()) return {};

  const world::ActorModel& actor{w.vec_actors_all[*opt_unit]};
  const std::vector<std::uint32_t> vec_eligible{players_to_include_in_selection(w, u4_viewer_player)};
  bool b_owner_eligible{false};
  for (const std::uint32_t u4_player : vec_eligible)
    if (u4_player == actor.u4_owner) b_owner_eligible = true;
  if (!b_owner_eligible) return {};

  const std::string str_class{selectable_class(w, actor)};
  return select_actors_on_screen(w, vec_eligible, std::span<const std::string>{&str_class, 1},
                                 rect_viewport);
}

std::int32_t drag_length(const DragState& drag) {
  return pixel_distance(drag.int4_start_x - drag.int4_mouse_x,
                        drag.int4_start_y - drag.int4_mouse_y);
}

bool drag_is_valid_box(const DragState& drag) {
  return drag.b_dragging && drag_length(drag) > kSelectionDeadzone;
}

// 上游 WorldInteractionControllerWidget.HandleMouseInput（WIC :85-183）左键面：
// Up 时 multiClick（tap>=2）分支优先于框选/点选分支，两者互斥。
void selection_handle_mouse_in_view(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                                    SelectionModel& selection, DragState& drag,
                                    const MouseInput& mi, const PxRect& rect_viewport) {
  drag.int4_mouse_x = view_to_world_px(mi.int4_x);
  drag.int4_mouse_y = view_to_world_px(mi.int4_y);

  if (mi.event == MouseInputEvent::kDown && mi.button == MouseButton::kLeft) {
    drag.b_dragging = true;
    drag.int4_start_x = drag.int4_mouse_x;
    drag.int4_start_y = drag.int4_mouse_y;
    return;
  }

  if (mi.event == MouseInputEvent::kUp && mi.button == MouseButton::kLeft && drag.b_dragging) {
    if (mi.int4_tap_count >= 2) {
      const std::vector<std::uint32_t> vec_new{select_by_type_double_click(
          w, u4_viewer_player, drag.int4_mouse_x, drag.int4_mouse_y, mi.mods, rect_viewport)};
      if (!vec_new.empty())
        selection_combine(selection, vec_new, true, false);
    } else {
      const std::vector<std::uint32_t> vec_new{select_actors_in_box(
          w, u4_viewer_player, drag.int4_start_x, drag.int4_start_y, drag.int4_mouse_x,
          drag.int4_mouse_y, mi.mods)};
      const bool b_click{drag.int4_start_x == drag.int4_mouse_x &&
                         drag.int4_start_y == drag.int4_mouse_y};
      selection_combine(selection, vec_new, has_modifier(mi.mods, ModifierKeys::kShift), b_click);
    }
    drag.b_dragging = false;
  }
}

void selection_handle_mouse(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                            SelectionModel& selection, DragState& drag, const MouseInput& mi) {
  selection_handle_mouse_in_view(w, u4_viewer_player, selection, drag, mi,
                                 PxRect{std::numeric_limits<std::int32_t>::min() / 2,
                                        std::numeric_limits<std::int32_t>::min() / 2,
                                        std::numeric_limits<std::int32_t>::max(),
                                        std::numeric_limits<std::int32_t>::max()});
}

}
