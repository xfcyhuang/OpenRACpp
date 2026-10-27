#pragma once
import std;

#include "net/connection.hpp"
#include "net/connection_target.hpp"
#include "net/replay_recorder.hpp"

namespace ora::net {

inline constexpr int kHandshakeProtocolVersion = 7;

class NetworkConnection final : public IConnection {
 public:
  explicit NetworkConnection(ConnectionTarget target);
  ~NetworkConnection() override { Dispose(); }

  NetworkConnection(const NetworkConnection&) = delete;
  NetworkConnection& operator=(const NetworkConnection&) = delete;

  const ConnectionTarget& Target() const { return target_; }

  ConnectionState State() const {
    return connection_state_.load(std::memory_order_relaxed);
  }

  std::string EndPoint() const;

  std::string ErrorMessage() const;

  ReplayRecorder* Recorder() { return ptr_recorder_.get(); }

  int LocalClientId() override {
    return client_id_.load(std::memory_order_relaxed);
  }

  void StartGame() override {}

  void Send(int frame, const std::vector<const Order*>& orders) override;
  void SendImmediate(const std::vector<const Order*>& orders) override;
  void SendSync(int frame, int sync_hash,
                std::uint64_t uint8_defeat_state) override;
  void Receive(OrderManager& order_manager) override;

  void StartRecording(ReplayRecorder::ChooseFilename fn_choose_filename);

  void Dispose();

 private:
  void ConnectBody();
  void ReceiveBody();
  void FailReceive();

  void SendRaw(const std::vector<std::uint8_t>& vec_packet);

  struct ConnectCollection {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<platform::TcpSocket> vec_ready;
    bool b_completed = false;

    bool Add(platform::TcpSocket tcp);
    bool TryTake(platform::TcpSocket& tcp_out, int int4_timeout_ms);
    void CompleteAdding();
    std::vector<platform::TcpSocket> TakeRemainder();
  };

  ConnectionTarget target_;
  std::unique_ptr<ReplayRecorder> ptr_recorder_;

  std::deque<SyncPacketData> deque_sent_sync_;
  std::deque<SyncPacketData> deque_queued_sync_;

  std::deque<std::pair<int, OrderPacket>> deque_sent_orders_;
  std::deque<OrderPacket> deque_sent_immediate_;

  std::mutex mutex_received_;
  std::deque<Packet> deque_received_;

  std::mutex mutex_socket_;
  platform::TcpSocket tcp_;

  std::atomic<ConnectionState> connection_state_{ConnectionState::Connecting};
  std::atomic<int> client_id_{0};
  std::atomic<bool> b_disposed_{false};

  mutable std::mutex mutex_status_;
  std::string str_end_point_;
  std::string str_error_;

  std::thread thread_connect_;
  std::thread thread_receive_;
  std::shared_ptr<ConnectCollection> ptr_connect_collection_;
};

}
