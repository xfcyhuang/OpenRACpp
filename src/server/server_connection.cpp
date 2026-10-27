import std;

#include "server/server_connection.hpp"

#include "net/order.hpp"
#include "server/server.hpp"

namespace ora::server {

namespace {

enum class ReceiveState { Header, Data };

}

ServerConnection::ServerConnection(int int4_index, std::string str_token,
                                   std::string str_endpoint,
                                   platform::TcpSocket tcp_accepted,
                                   ClockFn fn_clock)
    : int4_player_index(int4_index),
      str_auth_token(std::move(str_token)),
      str_end_point(std::move(str_endpoint)),
      fn_clock_(std::move(fn_clock)),
      tcp_(std::move(tcp_accepted)),
      time_connected_(std::chrono::steady_clock::now()) {}

void ServerConnection::StartReader(Server& server, bool b_multiplayer) {
  if (b_reader_started_)
    return;
  b_reader_started_ = true;

  server.NotifyConnectionAlive();
  auto self = shared_from_this();
  Server* p_server = &server;
  thread_ = std::thread(
      [self, p_server, b_multiplayer] {
        self->SendReceiveLoop(p_server, b_multiplayer);
      });
}

std::int64_t ServerConnection::TimeSinceLastResponse() const {
  return fn_clock_() - i8_last_received.load(std::memory_order_relaxed);
}

std::int64_t ServerConnection::ConnectionTimerMs() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - time_connected_)
      .count();
}

std::vector<std::uint8_t> ServerConnection::CreatePingFrame() const {
  auto w = net::ByteWriter{};
  w.Write(std::int32_t{13});
  w.Write(std::int32_t{0});
  w.Write(std::int32_t{0});
  w.WriteByte(static_cast<std::uint8_t>(net::OrderType::Ping));
  w.Write(fn_clock_());
  return w.TakeBytes();
}

void ServerConnection::SendReceiveLoop(Server* p_server, bool b_multiplayer) {
  tcp_.SetNonBlocking(true);
  tcp_.SetNoDelay();

  struct LoopExit {
    ServerConnection* p_self;
    Server* p_server;

    ~LoopExit() {
      p_server->OnConnectionDisconnect(p_self->shared_from_this());
      p_self->tcp_.Close();
      p_server->NotifyConnectionGone();
    }
  } loop_exit{this, p_server};

  auto vec_receive_buffer = std::vector<std::uint8_t>(1024);
  auto deque_read = std::deque<std::uint8_t>{};
  auto state = ReceiveState::Header;
  auto sz_expect = std::size_t{8};
  auto int4_frame = 0;
  auto time_last_ping = std::chrono::steady_clock::now();
  auto deque_ping_history = std::deque<int>{};

  while (true) {
    if (tcp_.PollReadable(100000)) {
      const int int4_read = tcp_.ReceiveUpTo(
          vec_receive_buffer.data(), vec_receive_buffer.size());
      if (int4_read == 0)
        return;

      if (int4_read > 0) {
        for (int i = 0; i < int4_read; ++i)
          deque_read.push_back(vec_receive_buffer[static_cast<std::size_t>(i)]);
        i8_last_received.store(fn_clock_(), std::memory_order_relaxed);
        b_timeout_message_shown.store(false, std::memory_order_relaxed);
      }

      while (deque_read.size() >= sz_expect) {
        auto vec_bytes = std::vector<std::uint8_t>{};
        vec_bytes.reserve(sz_expect);
        for (std::size_t i = 0; i < sz_expect; ++i) {
          vec_bytes.push_back(deque_read.front());
          deque_read.pop_front();
        }

        switch (state) {
          case ReceiveState::Header: {
            const auto int4_length = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(vec_bytes[0]) |
                (static_cast<std::uint32_t>(vec_bytes[1]) << 8) |
                (static_cast<std::uint32_t>(vec_bytes[2]) << 16) |
                (static_cast<std::uint32_t>(vec_bytes[3]) << 24));
            int4_frame = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(vec_bytes[4]) |
                (static_cast<std::uint32_t>(vec_bytes[5]) << 8) |
                (static_cast<std::uint32_t>(vec_bytes[6]) << 16) |
                (static_cast<std::uint32_t>(vec_bytes[7]) << 24));
            const auto int4_payload = int4_length - 4;
            if (int4_payload < 0 ||
                (b_multiplayer &&
                 int4_payload > static_cast<int>(kMaxOrderLength)))
              return;

            sz_expect = static_cast<std::size_t>(int4_payload);
            state = ReceiveState::Data;
            break;
          }

          case ReceiveState::Data: {
            if (sz_expect == 10 &&
                vec_bytes[0] == static_cast<std::uint8_t>(net::OrderType::Ping)) {
              if (deque_ping_history.size() == kMaxPingSamples)
                deque_ping_history.pop_front();

              std::int64_t i8_stamp = 0;
              for (auto i = 0; i < 8; ++i)
                i8_stamp |= static_cast<std::int64_t>(vec_bytes[static_cast<std::size_t>(i) + 1])
                            << (i * 8);
              deque_ping_history.push_back(
                  static_cast<int>(fn_clock_() - i8_stamp));

              auto vec_history = std::vector<int>{deque_ping_history.begin(),
                                                  deque_ping_history.end()};
              p_server->OnConnectionPing(shared_from_this(),
                                         std::move(vec_history),
                                         vec_bytes[9]);
            } else {
              p_server->OnConnectionPacket(shared_from_this(), int4_frame,
                                           std::move(vec_bytes));
            }

            sz_expect = 8;
            state = ReceiveState::Header;
            break;
          }
        }
      }
    }

    bool b_completed_now = false;
    {
      std::lock_guard lock{mutex_send_};
      b_completed_now = b_send_completed_;
    }

    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                              time_last_ping)
            .count() > 1000 &&
        TrySendData(CreatePingFrame()))
      time_last_ping = std::chrono::steady_clock::now();

    while (true) {
      auto vec_data = std::vector<std::uint8_t>{};
      {
        std::lock_guard lock{mutex_send_};
        if (deque_send_.empty())
          break;
        vec_data = std::move(deque_send_.front());
        deque_send_.pop_front();
      }

      auto sz_start = std::size_t{0};
      const auto sz_length = vec_data.size();

      while (sz_start < sz_length) {
        bool b_would_block = false;
        const int int4_sent = tcp_.SendNonBlocking(
            vec_data.data() + sz_start, sz_length - sz_start, b_would_block);
        if (b_would_block) {
          tcp_.SetNonBlocking(false);
          const bool b_ok = tcp_.SendAll(
              std::span{vec_data}.subspan(sz_start, sz_length - sz_start));
          tcp_.SetNonBlocking(true);
          if (!b_ok)
            return;
          sz_start = sz_length;
        } else if (int4_sent < 0) {
          return;
        } else {
          sz_start += static_cast<std::size_t>(int4_sent);
        }
      }
    }

    if (b_completed_now)
      return;
  }
}

bool ServerConnection::TrySendData(std::vector<std::uint8_t> vec_data) {
  {
    std::lock_guard lock{mutex_send_};
    if (b_send_completed_)
      return false;
    deque_send_.push_back(std::move(vec_data));
  }
  cv_send_.notify_all();
  return true;
}

void ServerConnection::Dispose() {
  {
    std::lock_guard lock{mutex_send_};
    b_send_completed_ = true;
  }
  cv_send_.notify_all();
}

ServerConnection::~ServerConnection() {
  Dispose();
  if (thread_.joinable()) {
    if (thread_.get_id() == std::this_thread::get_id())
      thread_.detach();
    else
      thread_.join();
  }
}

}
