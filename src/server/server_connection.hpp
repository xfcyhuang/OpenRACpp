#pragma once
import std;

#include "net/order_io.hpp"
#include "platform/tcp_socket.hpp"

namespace ora::server {

class Server;

class ServerConnection : public std::enable_shared_from_this<ServerConnection> {
 public:
  static constexpr int kMaxOrderLength = 131072;
  static constexpr int kMaxPingSamples = 15;

  using ClockFn = std::function<std::int64_t()>;

  const int int4_player_index = 0;
  const std::string str_auth_token;
  const std::string str_end_point;

  std::atomic<bool> b_timeout_message_shown{false};
  std::atomic<bool> b_validated{false};
  std::atomic<int> int4_last_orders_frame{0};

  ServerConnection(int int4_player_index, std::string str_auth_token,
                   std::string str_end_point,
                   platform::TcpSocket tcp_accepted, ClockFn fn_clock);

  void StartReader(Server& server, bool b_is_multiplayer);

  std::int64_t TimeSinceLastResponse() const;

  std::int64_t ConnectionTimerMs() const;

  bool TrySendData(std::vector<std::uint8_t> vec_data);

  void Dispose();

  ~ServerConnection();

 private:

  std::vector<std::uint8_t> CreatePingFrame() const;

  void SendReceiveLoop(Server* p_server, bool b_is_multiplayer);

  ClockFn fn_clock_;
  platform::TcpSocket tcp_;
  std::chrono::steady_clock::time_point time_connected_{};
  std::atomic<std::int64_t> i8_last_received{0};

  std::mutex mutex_send_;
  std::condition_variable cv_send_;
  std::deque<std::vector<std::uint8_t>> deque_send_;
  bool b_send_completed_ = false;

  std::thread thread_;
  bool b_reader_started_ = false;
};

}
