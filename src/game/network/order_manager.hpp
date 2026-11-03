#pragma once

import std;

#include "game/network/order.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {

constexpr std::int32_t kLocalClientId{1};
constexpr std::int32_t kNetFrameInterval{1};
constexpr std::size_t kChatMessageMaxLength{2500};

struct ClientPendingQueue {
  std::int32_t int4_client{};
  std::deque<std::pair<std::int32_t, std::vector<std::uint8_t>>> deque_pending;
};

struct OrderManagerModel {
  std::vector<OrderModel> vec_local;
  std::vector<OrderModel> vec_local_immediate;
  std::deque<std::pair<std::int32_t, std::vector<std::uint8_t>>> deque_echo_orders;
  std::deque<std::vector<std::uint8_t>> deque_echo_immediate;
  std::vector<std::vector<std::uint8_t>> vec_echo_sync;
  std::vector<ClientPendingQueue> vec_pending;
  std::vector<std::pair<std::int32_t, std::int32_t>> vec_sync_received;
  std::vector<std::string> vec_system_log;
  std::int32_t int4_local_frame{};
  std::int32_t int4_sent_orders_frame{};
  std::int32_t int4_last_sync_frame{-1};
  std::int32_t int4_last_sync_hash{};
  std::uint64_t u8_last_sync_defeat{};
  bool b_game_started{};
  bool b_out_of_sync{};
  std::string str_fatal;
};

void order_manager_start_game(OrderManagerModel& om, world::WorldModel& w,
                              std::span<const std::int32_t> vec_client_ids);

void issue_order(OrderManagerModel& om, const OrderModel& order);

void tick_immediate(OrderManagerModel& om, world::WorldModel& w);

bool try_tick(OrderManagerModel& om, world::WorldModel& w);

bool run_frame(OrderManagerModel& om, world::WorldModel& w);

}  // namespace ora::game::network
