#include "game/input/selection_logic.hpp"

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

std::int32_t selection_priority(const world::WorldModel& w, const world::ActorModel& actor,
                                std::uint32_t u4_viewer_player, ModifierKeys mods) {
  static_cast<void>(mods);
  const world::TraitModel* ptr_selectable{actor_trait_find(w, actor, "Selectable")};
  if (ptr_selectable == nullptr) return std::numeric_limits<std::int32_t>::min();

  const std::int32_t int4_base{ptr_selectable->int4_param};
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

std::int32_t drag_length(const DragState& drag) {
  return pixel_distance(drag.int4_start_x - drag.int4_mouse_x,
                        drag.int4_start_y - drag.int4_mouse_y);
}

bool drag_is_valid_box(const DragState& drag) {
  return drag.b_dragging && drag_length(drag) > kSelectionDeadzone;
}

void selection_handle_mouse(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                            SelectionModel& selection, DragState& drag, const MouseInput& mi) {
  drag.int4_mouse_x = view_to_world_px(mi.int4_x);
  drag.int4_mouse_y = view_to_world_px(mi.int4_y);

  if (mi.event == MouseInputEvent::kDown && mi.button == MouseButton::kLeft) {
    drag.b_dragging = true;
    drag.int4_start_x = drag.int4_mouse_x;
    drag.int4_start_y = drag.int4_mouse_y;
    return;
  }

  if (mi.event == MouseInputEvent::kUp && mi.button == MouseButton::kLeft && drag.b_dragging) {
    const std::vector<std::uint32_t> vec_new{select_actors_in_box(
        w, u4_viewer_player, drag.int4_start_x, drag.int4_start_y, drag.int4_mouse_x,
        drag.int4_mouse_y, mi.mods)};
    const bool b_click{drag.int4_start_x == drag.int4_mouse_x &&
                       drag.int4_start_y == drag.int4_mouse_y};
    selection_combine(selection, vec_new, has_modifier(mi.mods, ModifierKeys::kShift), b_click);
    drag.b_dragging = false;
  }
}

}
