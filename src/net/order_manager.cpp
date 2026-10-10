// UPSTREAM: OpenRA.Game/Network/OrderManager.cs @b6fc03f(实现部分:锁步核心)
//          The implementation half of OrderManager.cs (the lockstep core).
#include "net/order_manager.hpp"

#include "sim/world.hpp"

namespace ora::net {

OrderManager::OrderManager(std::unique_ptr<IConnection> conn)
    : connection_(std::move(conn)),
      tick_time_([this] { return SuggestedTimestep(); }, 0) {}

OrderManager::~OrderManager() {
  // Dispose(L287-291)
  b_disposed_ = true;
}

int OrderManager::OrderQueueLength() const {
  // L38
  if (map_pending_orders_.empty())
    return 0;
  int len = std::numeric_limits<int>::max();
  for (auto& [id, q] : map_pending_orders_)
    len = std::min<int>(len, static_cast<int>(q.size()));
  return len;
}

void OrderManager::StartGame() {
  // L98-116
  if (GameStarted())
    return;

  for (auto& client : lobby_info_.vec_clients)
    if (!client.IsBot())
      map_pending_orders_.emplace(client.Index,
                                  std::deque<FramePacket>{});

  // Generating sync reports is expensive, so only do it if we have
  // other players to compare against if a desync did occur
  b_generate_sync_report_ =
      !connection_->IsReplay() && lobby_info_.global_settings.EnableSyncReports;

  int4_net_frame_number_ = 1;
  int4_local_frame_number_ = 0;
  tick_time_.SetValue(fn_clock_());

  connection_->StartGame();
}

void OrderManager::IssueOrders(const std::vector<Order>& orders) {
  // L126-130
  for (const auto& order : orders)
    IssueOrder(order);
}

void OrderManager::IssueOrder(const Order& order) {
  // L132-138
  if (order.b_is_immediate)
    vec_local_immediate_orders_.push_back(order);
  else
    vec_local_orders_.push_back(order);
}

void OrderManager::SendImmediateOrders() {
  // L140-145
  if (!vec_local_immediate_orders_.empty() &&
      int4_game_save_last_frame_ < int4_net_frame_number_) {
    vec_local_immediate_ptrs_.clear();
    for (const auto& o : vec_local_immediate_orders_)
      vec_local_immediate_ptrs_.push_back(&o);
    connection_->SendImmediate(vec_local_immediate_ptrs_);
  }
  vec_local_immediate_orders_.clear();
}

void OrderManager::ReceiveDisconnect(int client_id, int frame) {
  // L147-159
  // All clients must process the disconnect on the same world tick to allow
  // synced actions to run deterministically. The server guarantees that we
  // will not receive any more order packets from this client from this
  // frame, so we can insert a marker in the orders stream and process the
  // synced disconnect behaviours on the first tick of that frame.
  if (GameStarted()) {
    // ClientDisconnected 标记 = 空 optional 载荷(上游 null packet)
    auto it = map_pending_orders_.find(client_id);
    if (it != map_pending_orders_.end())
      it->second.push_back(FramePacket{frame, std::nullopt});
    else
      throw std::runtime_error("Received packet from disconnected client '" +
                               std::to_string(client_id) + "'");
  }

  // The Client state field is not synced; update it immediately so it can
  // be shown in the UI
  auto* client = lobby_info_.ClientWithIndex(client_id);
  if (client != nullptr)
    client->State = ClientState::Disconnected;
}

void OrderManager::ReceiveSync(const SyncPacketData& sync) {
  // L161-170
  auto it = map_sync_for_frame_.find(sync.frame);
  if (it != map_sync_for_frame_.end()) {
    if (it->second.sync_hash != sync.sync_hash ||
        it->second.uint8_defeat_state != sync.uint8_defeat_state)
      OutOfSync(sync.frame);
  } else {
    map_sync_for_frame_.emplace(sync.frame, sync);
  }
}

void OrderManager::ReceiveImmediateOrders(int client_id,
                                          const OrderPacket& orders) {
  // L177-187
  auto parsed = orders.GetOrders(p_world_);
  for (auto& o : parsed) {
    if (fn_process_order_)
      fn_process_order_(*this, p_world_, client_id, *o);

    // A mod switch or other event has pulled the ground from beneath us
    if (b_disposed_)
      return;
  }
}

void OrderManager::ReceiveOrders(int client_id, int frame,
                                 OrderPacket orders) {
  // L189-195
  auto it = map_pending_orders_.find(client_id);
  if (it != map_pending_orders_.end())
    it->second.push_back(
        FramePacket{frame, std::move(orders)});
  else
    throw std::runtime_error("Received packet from disconnected client '" +
                             std::to_string(client_id) + "'");
}

void OrderManager::OutOfSync(int frame) {
  // L86-96
  if (b_is_out_of_sync_)
    return;

  // syncReport.DumpSyncReport(frame)(Phase 7)
  if (fn_out_of_sync_)
    fn_out_of_sync_(frame);
  b_is_out_of_sync_ = true;
}

bool OrderManager::IsReadyForNextFrame() const {
  // L202
  if (!GameStarted())
    return false;
  for (auto& [id, q] : map_pending_orders_)
    if (q.empty())
      return false;
  return true;
}

int OrderManager::SuggestedTimestep() const {
  // L204-222(World 依赖面逐步接通:loading/replay/tickScale 分支保真,
  // 其余以 World.Timestep 兜底)
  if (p_world_ == nullptr)
    return 40;  // Ui.Timestep 默认(Phase 6 接线)

  if (p_world_->IsLoadingGameSave())
    return 1;

  if (p_world_->IsReplay() && !b_is_out_of_sync_ &&
      int4_net_frame_number_ < connection_->ReplayTickCount())
    return p_world_->ReplayTimestep();

  if (fp4_tick_scale_ != 1.0f)
    return std::max(static_cast<int>(fp4_tick_scale_ * p_world_->Timestep()), 1);

  return p_world_->Timestep();
}

void OrderManager::SendOrders() {
  // L224-232
  if (GameStarted() && int4_game_save_last_frame_ < int4_net_frame_number_ &&
      int4_sent_orders_frame_ < int4_net_frame_number_) {
    vec_local_order_ptrs_.clear();
    for (const auto& o : vec_local_orders_)
      vec_local_order_ptrs_.push_back(&o);
    connection_->Send(int4_net_frame_number_, vec_local_order_ptrs_);
    vec_local_orders_.clear();
    int4_sent_orders_frame_ = int4_net_frame_number_;
  }
}

void OrderManager::ProcessOrders() {
  // L234-285
  for (auto& [client_id, frame_orders] : map_pending_orders_) {
    // The IsReadyForNextFrame check above guarantees that all clients have
    // sent a packet
    auto frame_packet = std::move(frame_orders.front());
    frame_orders.pop_front();

    // We expect every frame to have a queued order packet, even if it
    // contains no orders, as this controls the pacing of the game
    // simulation. Sanity check that we are processing the frame that we
    // expect, so we can crash early instead of desyncing.
    if (frame_packet.frame != int4_net_frame_number_)
      throw std::runtime_error(
          "Attempted to process orders from client " +
          std::to_string(client_id) + " for frame " +
          std::to_string(frame_packet.frame) + " on frame " +
          std::to_string(int4_net_frame_number_));

    if (!frame_packet.orders.has_value()) {
      // orders == ClientDisconnected(null)
      vec_process_clients_to_remove_.push_back(client_id);
      if (p_world_ != nullptr)
        p_world_->OnClientDisconnectedProxy(client_id);  // L250(见 world 接线)

      continue;
    }

    auto parsed = frame_packet.orders->GetOrders(p_world_);
    for (auto& o : parsed) {
      if (fn_process_order_)
        fn_process_order_(*this, p_world_, client_id, *o);
      vec_process_client_orders_.push_back(
          ClientOrder{client_id, std::move(o)});
    }
  }

  for (auto client_id : vec_process_clients_to_remove_)
    map_pending_orders_.erase(client_id);

  if (int4_net_frame_number_ >= int4_game_save_last_sync_frame_) {
    std::uint64_t defeat_state = 0;
    // World.Players 面(Phase 5 玩家链;空面保 0)
    if (p_world_ != nullptr) {
      const auto& players = p_world_->Players();
      for (std::size_t i = 0; i < players.size() && i < 64; ++i)
        if (players[i]->PlayerWinState() == sim::WinState::Lost)
          defeat_state |= 1ULL << i;
    }

    connection_->SendSync(int4_net_frame_number_,
                          p_world_ != nullptr ? p_world_->SyncHash() : 0,
                          defeat_state);
  } else {
    connection_->SendSync(int4_net_frame_number_, 0, 0);
  }

  // syncReport.UpdateSyncReport(L277-279;generateSyncReport 恒 false,
  // Phase 7)

  vec_process_client_orders_.clear();
  vec_process_clients_to_remove_.clear();

  ++int4_net_frame_number_;
}

void OrderManager::TickImmediate() {
  // L293-298
  SendImmediateOrders();

  // ReceiveAllOrdersAndCheckSync
  connection_->Receive(*this);
}

bool OrderManager::TryTick() {
  // L300-328
  auto should_tick = true;

  if (IsNetFrame()) {
    // Check whether or not we will be ready for a tick next frame
    // We don't need to include ourselves in the equation because we can
    // always generate orders this frame
    for (auto& [id, q] : map_pending_orders_)
      if (id != connection_->LocalClientId() && q.empty()) {
        should_tick = false;
        break;
      }

    // Send orders only if we are currently ready, this prevents us sending
    // orders too soon if we are stalling
    if (should_tick)
      SendOrders();
  }

  auto will_tick = should_tick;
  if (will_tick && IsNetFrame()) {
    will_tick = IsReadyForNextFrame();
    if (will_tick)
      ProcessOrders();
  }

  if (will_tick)
    ++int4_local_frame_number_;

  return will_tick;
}

}  // namespace ora::net
