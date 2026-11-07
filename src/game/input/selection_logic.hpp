#pragma once

import std;

#include "game/input/input_events.hpp"
#include "game/input/selection.hpp"
#include "game/world/world.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::input {

constexpr std::int32_t kSelectionDeadzone{24};

enum class SelectionRelationship {
  kSelf,
  kNeutral,
  kEnemy,
};

[[nodiscard]] SelectionRelationship selection_relationship(const world::WorldModel& w,
                                                           std::uint32_t u4_viewer_player,
                                                           std::uint32_t u4_owner_player);

[[nodiscard]] std::int32_t selection_priority(const world::WorldModel& w,
                                              const world::ActorModel& actor,
                                              std::uint32_t u4_viewer_player,
                                              ModifierKeys mods);

[[nodiscard]] std::int32_t pixel_distance(std::int32_t int4_dx, std::int32_t int4_dy);

[[nodiscard]] std::optional<std::uint32_t> select_highest_priority_at_point(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_x,
    std::int32_t int4_y, ModifierKeys mods);

[[nodiscard]] std::vector<std::uint32_t> select_actors_in_box(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_ax,
    std::int32_t int4_ay, std::int32_t int4_bx, std::int32_t int4_by, ModifierKeys mods);

struct DragState {
  bool b_dragging{};
  std::int32_t int4_start_x{};
  std::int32_t int4_start_y{};
  std::int32_t int4_mouse_x{};
  std::int32_t int4_mouse_y{};
};

[[nodiscard]] std::int32_t drag_length(const DragState& drag);

[[nodiscard]] bool drag_is_valid_box(const DragState& drag);

void selection_handle_mouse(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                            SelectionModel& selection, DragState& drag, const MouseInput& mi);

}
