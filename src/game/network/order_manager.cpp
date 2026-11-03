#include "game/network/order_manager.hpp"

#include "game/network/order_packet.hpp"
#include "game/world/trait.hpp"
#include "game/world/world.hpp"

namespace ora::game::network {

namespace {

void process_order(OrderManagerModel& om, world::WorldModel& w, std::int32_t int4_client,
                   const OrderModel& order);

void resolve_order(world::WorldModel& w, const OrderModel& order) {
  world::ActorModel* ptr_subject{
      order.b_has_subject ? world::world_actor_by_id(w, order.u4_subject) : nullptr};
  if (ptr_subject == nullptr || ptr_subject->b_disposed) return;

  for (const std::uint32_t u4_handle : ptr_subject->vec_trait_handles) {
    world::TraitModel& trait{w.vec_traits[u4_handle]};
    const world::TraitSpec* ptr_spec{world::trait_spec_find(trait.str_kind)};
    if (ptr_spec != nullptr && ptr_spec->fn_resolve)
      ptr_spec->fn_resolve(world::TraitResolveCtx{w, *ptr_subject, order}, trait);
  }
}

void system_chat(OrderManagerModel& om, std::int32_t int4_client, const OrderModel& order) {
  std::string str_message{order.opt_str_target.value_or("")};
  if (str_message.size() > kChatMessageMaxLength) str_message.resize(kChatMessageMaxLength);
  om.vec_system_log.push_back(
      std::format("chat {} {} {}", int4_client, order.u4_extra_data, str_message));
}

void system_command(OrderManagerModel& om, std::int32_t int4_client, const OrderModel& order) {
  om.vec_system_log.push_back(
      std::format("command {} {}", int4_client, order.opt_str_target.value_or("")));
}

void system_pause_game(world::WorldModel& w, const OrderModel& order) {
  w.b_paused = order.opt_str_target == "Pause";
}

struct SystemOrderEntry {
  std::string_view str_name;
  void (*fn_handle)(OrderManagerModel&, world::WorldModel&, std::int32_t, const OrderModel&);
};

void run_chat(OrderManagerModel& om, world::WorldModel& w, std::int32_t int4_client,
              const OrderModel& order) {
  system_chat(om, int4_client, order);
}

void run_command(OrderManagerModel& om, world::WorldModel& w, std::int32_t int4_client,
                 const OrderModel& order) {
  system_command(om, int4_client, order);
}

void run_pause_game(OrderManagerModel& om, world::WorldModel& w, std::int32_t int4_client,
                    const OrderModel& order) {
  system_pause_game(w, order);
}

const std::vector<SystemOrderEntry>& system_order_table() {
  static const std::vector<SystemOrderEntry> vec_table{
      SystemOrderEntry{"Message", nullptr},
      SystemOrderEntry{"FluentMessage", nullptr},
      SystemOrderEntry{"Chat", run_chat},
      SystemOrderEntry{"DisableChatEntry", nullptr},
      SystemOrderEntry{"StartKickVote", nullptr},
      SystemOrderEntry{"EndKickVote", nullptr},
      SystemOrderEntry{"StartGame", nullptr},
      SystemOrderEntry{"SaveTraitData", nullptr},
      SystemOrderEntry{"GameSaved", nullptr},
      SystemOrderEntry{"PauseGame", run_pause_game},
      SystemOrderEntry{"HandshakeRequest", nullptr},
      SystemOrderEntry{"ServerError", nullptr},
      SystemOrderEntry{"AuthenticationError", nullptr},
      SystemOrderEntry{"SyncInfo", nullptr},
      SystemOrderEntry{"SyncLobbyClients", nullptr},
      SystemOrderEntry{"SyncLobbySlots", nullptr},
      SystemOrderEntry{"SyncLobbyGlobalSettings", nullptr},
      SystemOrderEntry{"SyncConnectionQuality", nullptr},
      SystemOrderEntry{"SyncMapPool", nullptr},
      SystemOrderEntry{"GenerateMap", nullptr},
      SystemOrderEntry{"Command", run_command},
  };
  return vec_table;
}

void process_order(OrderManagerModel& om, world::WorldModel& w, std::int32_t int4_client,
                   const OrderModel& order) {
  if (order.b_handshake) return;

  for (const SystemOrderEntry& entry : system_order_table()) {
    if (entry.str_name != order.str_order) continue;
    if (entry.fn_handle != nullptr) entry.fn_handle(om, w, int4_client, order);
    return;
  }

  if (!order.b_has_grouped) {
    resolve_order(w, order);
    return;
  }

  for (const std::uint32_t u4_subject : order.vec_grouped) {
    OrderModel order_single{order};
    order_single.b_has_grouped = false;
    order_single.vec_grouped.clear();
    order_single.b_has_subject = true;
    order_single.u4_subject = u4_subject;
    resolve_order(w, order_single);
  }
}

void receive_immediate_orders(OrderManagerModel& om, world::WorldModel& w,
                              std::int32_t int4_client,
                              const std::vector<std::uint8_t>& vec_bytes) {
  for (const OrderModel& order : order_packet_get_orders(vec_bytes, &w))
    process_order(om, w, int4_client, order);
}

void receive_orders(OrderManagerModel& om, std::int32_t int4_client, std::int32_t int4_frame,
                    std::vector<std::uint8_t>&& vec_bytes) {
  for (ClientPendingQueue& queue : om.vec_pending) {
    if (queue.int4_client != int4_client) continue;
    queue.deque_pending.emplace_back(int4_frame, std::move(vec_bytes));
    return;
  }
  om.str_fatal = std::format("Received packet from disconnected client '{}'", int4_client);
}

void receive_sync(OrderManagerModel& om, std::int32_t int4_frame, std::int32_t int4_sync_hash,
                  std::uint64_t u8_defeat_state) {
  if (int4_frame == om.int4_last_sync_frame) {
    if (int4_sync_hash != om.int4_last_sync_hash || u8_defeat_state != om.u8_last_sync_defeat)
      om.b_out_of_sync = true;
  } else {
    om.int4_last_sync_frame = int4_frame;
    om.int4_last_sync_hash = int4_sync_hash;
    om.u8_last_sync_defeat = u8_defeat_state;
  }
  om.vec_sync_received.emplace_back(int4_frame, int4_sync_hash);
}

void connection_receive(OrderManagerModel& om, world::WorldModel& w) {
  while (!om.deque_echo_immediate.empty()) {
    receive_immediate_orders(om, w, kLocalClientId, om.deque_echo_immediate.front());
    om.deque_echo_immediate.pop_front();
  }

  while (!om.deque_echo_orders.empty()) {
    auto [int4_frame, vec_bytes]{om.deque_echo_orders.front()};
    om.deque_echo_orders.pop_front();
    receive_orders(om, kLocalClientId, int4_frame + 1, std::move(vec_bytes));
  }

  for (const std::vector<std::uint8_t>& vec_packet : om.vec_echo_sync) {
    const auto opt_frame{try_parse_sync_packet(vec_packet)};
    if (opt_frame.has_value())
      receive_sync(om, opt_frame->int4_frame, opt_frame->int4_sync_hash,
                   opt_frame->u8_defeat_state);
  }
  om.vec_echo_sync.clear();
}

void send_immediate_orders(OrderManagerModel& om) {
  if (om.vec_local_immediate.empty()) return;
  om.deque_echo_immediate.push_back(order_packet_from_orders(om.vec_local_immediate));
  om.vec_local_immediate.clear();
}

void send_orders(OrderManagerModel& om, const world::WorldModel& w) {
  if (!om.b_game_started || om.int4_sent_orders_frame >= static_cast<std::int32_t>(w.u4_net_frame))
    return;
  om.deque_echo_orders.emplace_back(static_cast<std::int32_t>(w.u4_net_frame),
                                    order_packet_from_orders(om.vec_local));
  om.vec_local.clear();
  om.int4_sent_orders_frame = static_cast<std::int32_t>(w.u4_net_frame);
}

void process_orders_frame(OrderManagerModel& om, world::WorldModel& w) {
  for (ClientPendingQueue& queue : om.vec_pending) {
    if (queue.deque_pending.empty()) continue;

    auto [int4_frame, vec_bytes]{queue.deque_pending.front()};
    queue.deque_pending.pop_front();

    if (int4_frame != static_cast<std::int32_t>(w.u4_net_frame)) {
      om.str_fatal = std::format(
          "Attempted to process orders from client {} for frame {} on frame {}",
          queue.int4_client, int4_frame, w.u4_net_frame);
      return;
    }

    for (const OrderModel& order : order_packet_get_orders(vec_bytes, &w))
      process_order(om, w, queue.int4_client, order);
  }

  const std::int32_t int4_frame{static_cast<std::int32_t>(w.u4_net_frame)};
  const std::int32_t int4_sync_hash{world::process_orders(w)};
  om.vec_echo_sync.push_back(serialize_sync_packet(int4_frame, int4_sync_hash, 0));
}

}  // namespace

void order_manager_start_game(OrderManagerModel& om, world::WorldModel& w,
                              std::span<const std::int32_t> vec_client_ids) {
  if (om.b_game_started) return;

  om.vec_pending.clear();
  for (const std::int32_t int4_client : vec_client_ids)
    om.vec_pending.push_back(ClientPendingQueue{int4_client, {}});

  om.b_game_started = true;
  w.u4_net_frame = 1;
  om.int4_local_frame = 0;
  om.int4_sent_orders_frame = 0;
  om.deque_echo_orders.clear();
  om.deque_echo_immediate.clear();
  om.vec_echo_sync.clear();

  om.deque_echo_orders.emplace_back(0, std::vector<std::uint8_t>{});
}

void issue_order(OrderManagerModel& om, const OrderModel& order) {
  if (order.b_is_immediate)
    om.vec_local_immediate.push_back(order);
  else
    om.vec_local.push_back(order);
}

void tick_immediate(OrderManagerModel& om, world::WorldModel& w) {
  send_immediate_orders(om);
  connection_receive(om, w);
}

bool try_tick(OrderManagerModel& om, world::WorldModel& w) {
  bool b_should_tick{true};

  const bool b_net_frame{om.int4_local_frame % kNetFrameInterval == 0};
  if (b_net_frame) {
    for (const ClientPendingQueue& queue : om.vec_pending)
      if (queue.int4_client != kLocalClientId && queue.deque_pending.empty()) b_should_tick = false;

    if (b_should_tick) send_orders(om, w);
  }

  bool b_will_tick{b_should_tick};
  if (b_will_tick && b_net_frame) {
    b_will_tick = om.b_game_started;
    for (const ClientPendingQueue& queue : om.vec_pending)
      if (queue.deque_pending.empty()) b_will_tick = false;

    if (b_will_tick) process_orders_frame(om, w);
  }

  if (b_will_tick) ++om.int4_local_frame;
  return b_will_tick;
}

bool run_frame(OrderManagerModel& om, world::WorldModel& w) {
  tick_immediate(om, w);
  const bool b_ticked{try_tick(om, w)};
  world::world_tick(w);
  return b_ticked;
}

}  // namespace ora::game::network
