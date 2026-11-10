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

// 上游 TargetModifiers（TraitsInterfaces.cs:129-138）。
enum class TargetModifiers : std::uint8_t {
  kNone = 0,
  kForceAttack = 1,
  kForceQueue = 2,
  kForceMove = 4,
};

[[nodiscard]] constexpr TargetModifiers operator&(TargetModifiers a, TargetModifiers b) {
  return static_cast<TargetModifiers>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr TargetModifiers operator|(TargetModifiers a, TargetModifiers b) {
  return static_cast<TargetModifiers>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr bool has_target_modifier(TargetModifiers mods, TargetModifiers mod) {
  return (mods & mod) != TargetModifiers::kNone;
}

// 上游 UOG.OrderForUnit :169-174：Ctrl→ForceAttack、Shift→ForceQueue、Alt→ForceMove
// （独立位可叠加）。
[[nodiscard]] TargetModifiers target_modifiers_from_keys(ModifierKeys mods);

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

// 通用攻击单构造：str_order_name 可为 "Attack"/"ForceAttack"；目标 actor 或 cell。
[[nodiscard]] network::OrderModel make_attack_order_ex(std::uint32_t u4_subject,
                                                       const network::TargetModel& target,
                                                       std::string_view str_order_name,
                                                       bool b_queued);

// AttackMoveOrderGenerator.OrderInner（AttackMove.cs:118-131）：subjects = 选中
// AttackMove 单；Ctrl→"AssaultMove" 否则 "AttackMove"；Shift→queued 且不退出模式；
// 非 Shift 用后自动 CancelInputMode（无头面无生成器状态可退，省略）。
// 单条订单：Subject 空、Target.FromCell、GroupedActors=subjects。
[[nodiscard]] std::vector<network::OrderModel> attack_move_generator_order(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, const SelectionModel& selection,
    std::int32_t int4_cell_x, std::int32_t int4_cell_y, const MouseInput& mi);

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

}
