#include "net/order_manager.hpp"

#include "sim/world.hpp"

namespace ora::net {

OrderManager::OrderManager(std::unique_ptr<IConnection> conn)
    : connection_(std::move(conn)),
      tick_time_([this] { return SuggestedTimestep(); }, 0) {}

OrderManager::~OrderManager() {

  b_disposed_ = true;
}

int OrderManager::OrderQueueLength() const {

  if (map_pending_orders_.empty())
    return 0;
  int len = std::numeric_limits<int>::max();
  for (auto& [id, q] : map_pending_orders_)
    len = std::min<int>(len, static_cast<int>(q.size()));
  return len;
}

void OrderManager::StartGame() {

  if (GameStarted())
    return;

  for (auto& client : lobby_info_.vec_clients)
    if (!client.IsBot())
      map_pending_orders_.emplace(client.Index,
                                  std::deque<FramePacket>{});

  b_generate_sync_report_ =
      !connection_->IsReplay() && lobby_info_.global_settings.EnableSyncReports;

  int4_net_frame_number_ = 1;
  int4_local_frame_number_ = 0;
  tick_time_.SetValue(fn_clock_());

  connection_->StartGame();
}

void OrderManager::IssueOrders(const std::vector<Order>& orders) {

  for (const auto& order : orders)
    IssueOrder(order);
}

void OrderManager::IssueOrder(const Order& order) {

  if (order.b_is_immediate)
    vec_local_immediate_orders_.push_back(order);
  else
    vec_local_orders_.push_back(order);
}

void OrderManager::SendImmediateOrders() {

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

  if (GameStarted()) {

    auto it = map_pending_orders_.find(client_id);
    if (it != map_pending_orders_.end())
      it->second.push_back(FramePacket{frame, std::nullopt});
    else
      throw std::runtime_error("Received packet from disconnected client '" +
                               std::to_string(client_id) + "'");
  }

  auto* client = lobby_info_.ClientWithIndex(client_id);
  if (client != nullptr)
    client->State = ClientState::Disconnected;
}

void OrderManager::ReceiveSync(const SyncPacketData& sync) {

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

  auto parsed = orders.GetOrders(p_world_);
  for (auto& o : parsed) {
    if (fn_process_order_)
      fn_process_order_(*this, p_world_, client_id, *o);

    if (b_disposed_)
      return;
  }
}

void OrderManager::ReceiveOrders(int client_id, int frame,
                                 OrderPacket orders) {

  auto it = map_pending_orders_.find(client_id);
  if (it != map_pending_orders_.end())
    it->second.push_back(
        FramePacket{frame, std::move(orders)});
  else
    throw std::runtime_error("Received packet from disconnected client '" +
                             std::to_string(client_id) + "'");
}

void OrderManager::OutOfSync(int frame) {

  if (b_is_out_of_sync_)
    return;

  if (fn_out_of_sync_)
    fn_out_of_sync_(frame);
  b_is_out_of_sync_ = true;
}

bool OrderManager::IsReadyForNextFrame() const {

  if (!GameStarted())
    return false;
  for (auto& [id, q] : map_pending_orders_)
    if (q.empty())
      return false;
  return true;
}

int OrderManager::SuggestedTimestep() const {

  if (p_world_ == nullptr)
    return 40;

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

  for (auto& [client_id, frame_orders] : map_pending_orders_) {

    auto frame_packet = std::move(frame_orders.front());
    frame_orders.pop_front();

    if (frame_packet.frame != int4_net_frame_number_)
      throw std::runtime_error(
          "Attempted to process orders from client " +
          std::to_string(client_id) + " for frame " +
          std::to_string(frame_packet.frame) + " on frame " +
          std::to_string(int4_net_frame_number_));

    if (!frame_packet.orders.has_value()) {

      vec_process_clients_to_remove_.push_back(client_id);
      if (p_world_ != nullptr)
        p_world_->OnClientDisconnectedProxy(client_id);

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

  vec_process_client_orders_.clear();
  vec_process_clients_to_remove_.clear();

  ++int4_net_frame_number_;
}

void OrderManager::TickImmediate() {

  SendImmediateOrders();

  connection_->Receive(*this);
}

bool OrderManager::TryTick() {

  auto should_tick = true;

  if (IsNetFrame()) {

    for (auto& [id, q] : map_pending_orders_)
      if (id != connection_->LocalClientId() && q.empty()) {
        should_tick = false;
        break;
      }

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

}
