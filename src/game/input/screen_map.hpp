#pragma once

import std;

#include "game/world/actor.hpp"
#include "game/world/world.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::input {

struct PxRect {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_w{};
  std::int32_t int4_h{};

  [[nodiscard]] friend bool operator==(const PxRect&, const PxRect&) = default;

  [[nodiscard]] constexpr bool contains(std::int32_t int4_px_x, std::int32_t int4_px_y) const {
    return int4_px_x >= int4_x && int4_px_x < int4_x + int4_w && int4_px_y >= int4_y &&
           int4_px_y < int4_y + int4_h;
  }

  [[nodiscard]] constexpr bool intersects(const PxRect& other) const {
    return int4_x < other.int4_x + other.int4_w && other.int4_x < int4_x + int4_w &&
           int4_y < other.int4_y + other.int4_h && other.int4_y < int4_y + int4_h;
  }
};

struct ActorBoundsPair {
  std::uint32_t u4_actor_id{};
  PxRect rect_bounds;
};

[[nodiscard]] std::int32_t view_to_world_px(std::int32_t int4_view_px);

[[nodiscard]] std::int32_t world_px_to_cell(std::int32_t int4_world_px);

[[nodiscard]] std::int32_t actor_center_px(std::int32_t int4_cell);

[[nodiscard]] std::optional<PxRect> actor_mouse_bounds(const world::WorldModel& w,
                                                       const world::ActorModel& actor);

[[nodiscard]] std::vector<ActorBoundsPair> actors_at_mouse(const world::WorldModel& w,
                                                           std::int32_t int4_x,
                                                           std::int32_t int4_y);

[[nodiscard]] std::vector<ActorBoundsPair> actors_in_box(const world::WorldModel& w,
                                                         const PxRect& rect);

}  // namespace ora::game::input
