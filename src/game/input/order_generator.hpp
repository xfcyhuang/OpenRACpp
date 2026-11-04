#pragma once

import std;

#include "game/input/input_events.hpp"
#include "game/input/selection.hpp"
#include "game/input/selection_logic.hpp"
#include "game/network/order.hpp"
#include "game/world/world.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::input {

[[nodiscard]] std::int32_t cpos_bits(std::int32_t int4_x, std::int32_t int4_y,
                                     std::int32_t int4_layer);

[[nodiscard]] network::TargetModel make_cell_target(std::int32_t int4_x, std::int32_t int4_y);

[[nodiscard]] network::TargetModel make_actor_target(std::uint32_t u4_actor,
                                                     std::int32_t int4_generation);

[[nodiscard]] network::OrderModel make_move_order(std::uint32_t u4_subject,
                                                  std::int32_t int4_cell_x,
                                                  std::int32_t int4_cell_y, bool b_queued);

[[nodiscard]] network::OrderModel make_attack_order(std::uint32_t u4_subject,
                                                    std::uint32_t u4_target_actor,
                                                    std::int32_t int4_generation, bool b_queued);

[[nodiscard]] network::OrderModel make_create_group_order(std::uint32_t u4_player_actor,
                                                          std::span<const std::uint32_t> vec_grouped);

[[nodiscard]] network::OrderModel make_start_production_order(std::uint32_t u4_queue_actor,
                                                               std::string_view str_item,
                                                               std::uint32_t u4_count,
                                                               bool b_queued);

[[nodiscard]] network::OrderModel make_pause_production_order(std::uint32_t u4_queue_actor,
                                                              std::string_view str_item,
                                                              bool b_paused);

[[nodiscard]] network::OrderModel make_cancel_production_order(std::uint32_t u4_queue_actor,
                                                               std::string_view str_item,
                                                               std::uint32_t u4_count);

[[nodiscard]] std::vector<network::OrderModel> unit_order_generator_order(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, const SelectionModel& selection,
    std::int32_t int4_cell_x, std::int32_t int4_cell_y, std::int32_t int4_world_px_x,
    std::int32_t int4_world_px_y, const MouseInput& mi);

[[nodiscard]] std::vector<network::OrderModel> order_generator_handle_mouse(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, const SelectionModel& selection,
    const DragState& drag, const MouseInput& mi);

}  // namespace ora::game::input
