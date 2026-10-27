#include "net/network_connection.hpp"

#include "net/byte_io.hpp"
#include "net/order_manager.hpp"

namespace ora::net {

namespace {

void SetErrorMessage(std::mutex& mutex, std::string& str_out,
                      std::string str_message) {
  std::lock_guard lock(mutex);
  str_out = std::move(str_message);
}

}

bool NetworkConnection::ConnectCollection::Add(platform::TcpSocket tcp) {
  std::lock_guard lock(mutex);
  if (b_completed) {
    tcp.Close();
    return false;
  }
  vec_ready.push_back(std::move(tcp));
  cv.notify_one();
  return true;
}

bool NetworkConnection::ConnectCollection::TryTake(
    platform::TcpSocket& tcp_out, int int4_timeout_ms) {
  std::unique_lock lock(mutex);
  const bool b_signalled = cv.wait_for(
      lock, std::chrono::milliseconds(int4_timeout_ms),
      [this] { return !vec_ready.empty() || b_completed; });
  if (vec_ready.empty()) {
    (void)b_signalled;
    return false;
  }
  tcp_out = std::move(vec_ready.back());
  vec_ready.pop_back();
  return true;
}

void NetworkConnection::ConnectCollection::CompleteAdding() {
  std::lock_guard lock(mutex);
  b_completed = true;
  cv.notify_all();
}

std::vector<platform::TcpSocket>
NetworkConnection::ConnectCollection::TakeRemainder() {
  std::lock_guard lock(mutex);
  return std::move(vec_ready);
}

NetworkConnection::NetworkConnection(ConnectionTarget target)
    : target_(std::move(target)) {
  ptr_connect_collection_ = std::make_shared<ConnectCollection>();
  thread_connect_ = std::thread([this] { ConnectBody(); });
}

std::string NetworkConnection::EndPoint() const {
  std::lock_guard lock(mutex_status_);
  return str_end_point_;
}

std::string NetworkConnection::ErrorMessage() const {
  std::lock_guard lock(mutex_status_);
  return str_error_;
}

void NetworkConnection::ConnectBody() {
  auto& collection = *ptr_connect_collection_;

  const auto vec_endpoints = target_.GetConnectEndPoints();
  std::vector<std::thread> vec_endpoint_threads;

  auto b_at_least_one = false;
  for (const auto& endpoint : vec_endpoints) {
    b_at_least_one = true;
    vec_endpoint_threads.emplace_back([this, endpoint, &collection] {
      auto tcp = platform::TcpSocket::Connect(endpoint);
      if (!tcp.IsValid()) {
        SetErrorMessage(mutex_status_, str_error_, "Failed to connect");
        return;
      }
      collection.Add(std::move(tcp));
    });
  }

  if (!b_at_least_one) {
    SetErrorMessage(mutex_status_, str_error_, "Failed to resolve address");
    connection_state_.store(ConnectionState::NotConnected,
                            std::memory_order_relaxed);
  } else {
    auto tcp_won = platform::TcpSocket{};
    if (collection.TryTake(tcp_won, 5000)) {
      {
        std::lock_guard lock(mutex_status_);
        str_end_point_ = tcp_won.RemoteDisplay();
      }
      {
        std::lock_guard lock(mutex_socket_);
        tcp_ = std::move(tcp_won);
      }
      thread_receive_ = std::thread([this] { ReceiveBody(); });
    } else {
      connection_state_.store(ConnectionState::NotConnected,
                              std::memory_order_relaxed);
    }
  }

  collection.CompleteAdding();
  for (auto& tcp : collection.TakeRemainder())
    tcp.Close();
  for (auto& thread : vec_endpoint_threads)
    thread.join();
}

void NetworkConnection::ReceiveBody() {
  auto int4_handshake = std::int32_t{0};
  if (!tcp_.ReadInt32(int4_handshake) ||
      int4_handshake != kHandshakeProtocolVersion) {
    FailReceive();
    return;
  }

  auto int4_client_id = std::int32_t{0};
  if (!tcp_.ReadInt32(int4_client_id)) {
    FailReceive();
    return;
  }
  client_id_.store(int4_client_id, std::memory_order_relaxed);
  connection_state_.store(ConnectionState::Connected,
                          std::memory_order_relaxed);

  for (;;) {
    auto int4_len = std::int32_t{0};
    auto int4_client = std::int32_t{0};
    if (!tcp_.ReadInt32(int4_len) || !tcp_.ReadInt32(int4_client)) {
      FailReceive();
      return;
    }

    auto vec_buf = std::vector<std::uint8_t>{};
    auto b_bad = int4_len < 0;
    if (!b_bad && int4_len != 0) {
      vec_buf.resize(static_cast<std::size_t>(int4_len));
      b_bad = !tcp_.ReceiveExact(vec_buf.data(), vec_buf.size());
    }
    if (b_bad) {
      FailReceive();
      return;
    }

    {
      std::lock_guard lock(mutex_received_);
      deque_received_.push_back(Packet{int4_client, std::move(vec_buf)});
    }
  }
}

void NetworkConnection::FailReceive() {
  SetErrorMessage(mutex_status_, str_error_, "Connection failed");
  connection_state_.store(ConnectionState::NotConnected,
                          std::memory_order_relaxed);
}

void NetworkConnection::Send(int frame,
                             const std::vector<const Order*>& vec_orders) {
  auto packet = OrderPacket{vec_orders};
  deque_sent_orders_.emplace_back(frame, packet);
  SendRaw(packet.Serialize(frame));
}

void NetworkConnection::SendImmediate(
    const std::vector<const Order*>& vec_orders) {
  auto packet = OrderPacket{vec_orders};
  deque_sent_immediate_.push_back(packet);
  SendRaw(packet.Serialize(0));
}

void NetworkConnection::SendSync(int frame, int sync_hash,
                                 std::uint64_t uint8_defeat_state) {
  deque_queued_sync_.push_back(
      SyncPacketData{frame, sync_hash, uint8_defeat_state});
}

void NetworkConnection::SendRaw(const std::vector<std::uint8_t>& vec_packet) {
  auto writer = ByteWriter{};
  writer.Write(static_cast<std::int32_t>(vec_packet.size()));
  writer.WriteBytes(vec_packet);

  for (const auto& sync : deque_queued_sync_) {
    auto vec_q = OrderIO::SerializeSync(sync);
    writer.Write(static_cast<std::int32_t>(vec_q.size()));
    writer.WriteBytes(vec_q);
    deque_sent_sync_.push_back(sync);
  }
  deque_queued_sync_.clear();

  std::lock_guard lock(mutex_socket_);
  (void)tcp_.SendAll(writer.Bytes());
}

void NetworkConnection::Receive(OrderManager& order_manager) {
  while (!deque_sent_immediate_.empty()) {
    auto immediate = std::move(deque_sent_immediate_.front());
    deque_sent_immediate_.pop_front();
    order_manager.ReceiveImmediateOrders(
        client_id_.load(std::memory_order_relaxed), immediate);
    if (ptr_recorder_)
      ptr_recorder_->Receive(client_id_.load(std::memory_order_relaxed),
                             immediate.Serialize(0));

    if (b_disposed_.load(std::memory_order_relaxed))
      return;
  }

  while (!deque_sent_sync_.empty()) {
    auto sync = deque_sent_sync_.front();
    deque_sent_sync_.pop_front();
    order_manager.ReceiveSync(sync);
    if (ptr_recorder_)
      ptr_recorder_->Receive(
          client_id_.load(std::memory_order_relaxed),
          OrderIO::SerializeSync(sync));
  }

  for (;;) {
    auto packet = Packet{};
    {
      std::lock_guard lock(mutex_received_);
      if (deque_received_.empty())
        break;
      packet = std::move(deque_received_.front());
      deque_received_.pop_front();
    }

    auto disconnect = DisconnectData{};
    auto sync = SyncPacketData{};
    auto fp4_scale = float{0};
    auto int8_timestamp = std::int64_t{0};
    auto int4_ack_frame = std::int32_t{0};
    auto uint1_ack_count = std::uint8_t{0};
    auto int4_frame = std::int32_t{0};
    auto orders = OrderPacket{};

    if (OrderIO::TryParseDisconnect(packet, disconnect)) {
      order_manager.ReceiveDisconnect(disconnect.client_id,
                                      disconnect.frame);
      if (ptr_recorder_)
        ptr_recorder_->Receive(packet.from_client, packet.vec_data);
    } else if (OrderIO::TryParseSync(packet.vec_data, sync)) {
      order_manager.ReceiveSync(sync);
      if (ptr_recorder_)
        ptr_recorder_->Receive(packet.from_client, packet.vec_data);
    } else if (OrderIO::TryParseTickScale(packet, fp4_scale)) {
      order_manager.ReceiveTickScale(fp4_scale);
    } else if (OrderIO::TryParsePingRequest(packet, int8_timestamp)) {

      SendRaw(OrderIO::SerializePingResponse(
          int8_timestamp,
          static_cast<std::uint8_t>(order_manager.OrderQueueLength())));
    } else if (OrderIO::TryParseAck(packet, int4_ack_frame,
                                    uint1_ack_count)) {
      if (uint1_ack_count > deque_sent_orders_.size())
        throw std::runtime_error(
            "Received Ack for " + std::to_string(uint1_ack_count) + " > " +
            std::to_string(deque_sent_orders_.size()) + " frames.");

      auto packet_orders = OrderPacket{};
      if (uint1_ack_count != 1) {
        auto vec_taken = std::vector<OrderPacket>{};
        vec_taken.reserve(uint1_ack_count);
        for (auto i = std::size_t{0}; i < uint1_ack_count; ++i) {
          vec_taken.push_back(std::move(deque_sent_orders_.front().second));
          deque_sent_orders_.pop_front();
        }
        auto vec_ptrs = std::vector<const OrderPacket*>{};
        vec_ptrs.reserve(vec_taken.size());
        for (const auto& taken : vec_taken)
          vec_ptrs.push_back(&taken);
        packet_orders = OrderPacket::Combine(vec_ptrs);
      } else {
        packet_orders = std::move(deque_sent_orders_.front().second);
        deque_sent_orders_.pop_front();
      }

      order_manager.ReceiveOrders(
          client_id_.load(std::memory_order_relaxed), int4_ack_frame,
          std::move(packet_orders));
      if (ptr_recorder_) {
        auto vec_recorded = packet_orders.Serialize(int4_ack_frame);
        ptr_recorder_->Receive(
            client_id_.load(std::memory_order_relaxed), vec_recorded);
      }
    } else if (OrderIO::TryParseOrderPacket(packet.vec_data, int4_frame,
                                            orders)) {
      if (int4_frame == 0)
        order_manager.ReceiveImmediateOrders(packet.from_client, orders);
      else
        order_manager.ReceiveOrders(packet.from_client, int4_frame,
                                    std::move(orders));

      if (ptr_recorder_)
        ptr_recorder_->Receive(packet.from_client, packet.vec_data);
    } else {
      throw std::runtime_error(
          "Received unknown packet from client " +
          std::to_string(packet.from_client) + " with length " +
          std::to_string(packet.vec_data.size()));
    }
  }
}

void NetworkConnection::StartRecording(
    ReplayRecorder::ChooseFilename fn_choose_filename) {
  if (ptr_recorder_)
    ptr_recorder_->Dispose();
  ptr_recorder_ =
      std::make_unique<ReplayRecorder>(std::move(fn_choose_filename));
}

void NetworkConnection::Dispose() {
  auto b_expected = false;
  if (!b_disposed_.compare_exchange_strong(b_expected, true))
    return;

  tcp_.Close();
  if (ptr_connect_collection_)
    ptr_connect_collection_->CompleteAdding();
  if (thread_connect_.joinable())
    thread_connect_.join();
  if (thread_receive_.joinable())
    thread_receive_.join();

  if (ptr_recorder_)
    ptr_recorder_->Dispose();
}

}
