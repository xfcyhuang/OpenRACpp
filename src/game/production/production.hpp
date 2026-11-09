#pragma once

import std;

#include "game/support/mersenne_twister.hpp"
#include "game/world/trait.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::production {

enum class PowerState : std::uint8_t { kNormal = 0, kLow = 1, kCritical = 2 };

struct LobbyModel {
  std::vector<std::pair<std::string, std::string>> vec_playable_factions;
  std::int32_t int4_starting_cash{5000};
  std::string str_techlevel{"unrestricted"};
  bool b_build_radius{true};
};

struct ProductionItemModel {
  std::string str_item;
  std::int32_t int4_total_cost{};
  std::int32_t int4_remaining_cost{};
  std::int32_t int4_resources_paid{};
  std::int32_t int4_total_time{1};
  std::int32_t int4_remaining_time{1};
  std::int32_t int4_slowdown{};
  bool b_started{};
  bool b_paused{};
  bool b_done{};
  bool b_ready_notified{};
};

struct ProductionQueueState {
  std::string str_type;
  std::int32_t int4_low_power_modifier{100};
  bool b_enabled{};
  bool b_valid_faction{true};
  std::vector<ProductionItemModel> vec_queue;
  std::string str_ready_audio;
  std::string str_blocked_audio;
};

struct PowerManagerState {
  std::map<std::uint32_t, std::int32_t> map_power_drain;
  std::int32_t int4_power_provided{};
  std::int32_t int4_power_drained{};
};

struct PlayerResourcesState {
  std::int32_t int4_cash{};
  std::int32_t int4_resources{};
  std::int32_t int4_capacity{};
};

struct PlayerProductionState {
  std::vector<ProductionQueueState> vec_queues;
  PowerManagerState power;
  PlayerResourcesState res;
};

struct ProductionWorldState {
  std::vector<PlayerProductionState> vec_players;
};

[[nodiscard]] PowerState power_state(std::int32_t int4_provided, std::int32_t int4_drained);

[[nodiscard]] std::string_view power_state_name(PowerState state);

[[nodiscard]] std::int32_t apply_percentage_modifiers(
    std::int32_t int4_value, std::span<const std::int32_t> vec4_modifiers);

bool take_cash(PlayerResourcesState& res, std::int32_t int4_num);

void refund_cash(PlayerResourcesState& res, std::int32_t int4_num);

void refund_resources(PlayerResourcesState& res, std::int32_t int4_num);

template <typename T>
void shuffle_forward(support::MersenneTwister& rng, std::span<T> span_items) {
  for (std::size_t idx{0}; idx + 1 < span_items.size(); ++idx) {
    const std::int32_t int4_j{
        support::next(rng, static_cast<std::int32_t>(span_items.size() - idx))};
    std::swap(span_items[idx], span_items[static_cast<std::size_t>(int4_j) + idx]);
  }
}

[[nodiscard]] std::string production_observation(const world::WorldModel& w,
                                                 std::uint32_t u4_player);

[[nodiscard]] std::vector<std::string> queue_buildable_items(const world::WorldModel& w,
                                                             std::uint32_t u4_player,
                                                             std::string_view str_queue_type);

void production_queue_state_build(world::WorldModel& w);

void production_on_actor_added(world::WorldModel& w, std::uint32_t u4_actor_id);

void production_on_actor_removed(world::WorldModel& w, std::uint32_t u4_actor_id);

void production_frame_end_task(world::WorldModel& w, std::uint32_t u4_kind,
                               std::uint32_t u4_player, std::uint32_t u4_value,
                               const std::string& str_item);

[[nodiscard]] const std::vector<world::TraitSpec>& production_trait_specs();

}
