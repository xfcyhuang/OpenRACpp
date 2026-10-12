import std;

#include "net/connection_target.hpp"
#include "net/network_connection.hpp"
#include "net/order_io.hpp"
#include "net/order_manager.hpp"
#include "net/replay_recorder.hpp"

#include "platform/tcp_socket.hpp"

#include <cstdio>

namespace net = ora::net;
namespace platform = ora::platform;

static int g_failures = 0;

void Check(bool b_ok, std::string_view str_what) {
  if (!b_ok) {
    std::println("FAIL: {}", str_what);
    ++g_failures;
  }
}

void CheckEq(const std::string& a, const std::string& b,
             std::string_view str_what) {
  if (!(a == b)) {
    std::println("FAIL: {} ({} != {})", str_what, a, b);
    ++g_failures;
  }
}

namespace {

void AppendInt32(std::vector<std::uint8_t>& vec, std::int32_t v) {
  for (auto i = 0; i < 4; ++i)
    vec.push_back(static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(v) >> (i * 8)) & 0xFF));
}

void AppendInt64(std::vector<std::uint8_t>& vec, std::int64_t v) {
  for (auto i = 0; i < 8; ++i)
    vec.push_back(static_cast<std::uint8_t>(
        (static_cast<std::uint64_t>(v) >> (i * 8)) & 0xFF));
}

void AppendFloat(std::vector<std::uint8_t>& vec, float v) {
  std::uint32_t bits;
  std::memcpy(&bits, &v, 4);
  AppendInt32(vec, static_cast<std::int32_t>(bits));
}

std::int32_t ReadInt32At(const std::vector<std::uint8_t>& vec,
                         std::size_t at) {
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(vec[at]) |
      (static_cast<std::uint32_t>(vec[at + 1]) << 8) |
      (static_cast<std::uint32_t>(vec[at + 2]) << 16) |
      (static_cast<std::uint32_t>(vec[at + 3]) << 24));
}

std::int64_t ReadInt64At(const std::vector<std::uint8_t>& vec,
                         std::size_t at) {
  std::uint64_t v = 0;
  for (auto i = 0; i < 8; ++i)
    v |= static_cast<std::uint64_t>(vec[at + i]) << (i * 8);
  return static_cast<std::int64_t>(v);
}

bool WaitUntil(const std::function<bool()>& fn_pred, int int4_timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(int4_timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (fn_pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return fn_pred();
}

struct RecordedEntry {
  int int4_client = 0;
  std::vector<std::uint8_t> vec_data;
};

std::vector<RecordedEntry> ParseRecorderBytes(
    const std::vector<std::uint8_t>& vec_bytes) {
  auto vec_out = std::vector<RecordedEntry>{};
  auto at = std::size_t{0};
  while (at + 8 <= vec_bytes.size()) {
    const auto int4_client = ReadInt32At(vec_bytes, at);
    const auto int4_len = ReadInt32At(vec_bytes, at + 4);
    at += 8;
    if (int4_len < 0 || at + static_cast<std::size_t>(int4_len) >
                            vec_bytes.size())
      break;
    vec_out.push_back(
        RecordedEntry{int4_client,
                      std::vector<std::uint8_t>(
                          vec_bytes.begin() + static_cast<std::ptrdiff_t>(at),
                          vec_bytes.begin() +
                              static_cast<std::ptrdiff_t>(
                                  at + int4_len))});
    at += static_cast<std::size_t>(int4_len);
  }
  return vec_out;
}

bool HasRecordedFrame(const std::vector<RecordedEntry>& vec_entries,
                      int int4_client, int int4_frame) {
  for (const auto& entry : vec_entries)
    if (entry.int4_client == int4_client && !entry.vec_data.empty() &&
        entry.vec_data.size() >= 4 &&
        ReadInt32At(entry.vec_data, 0) == int4_frame)
      return true;
  return false;
}

struct FakeServer {
  platform::TcpSocket tcp_listen;
  int int4_port = 0;
  std::string str_failures;
  std::vector<std::uint8_t> vec_first_order_payload;

  bool WriteAll(const std::uint8_t* p, std::size_t sz) {
    return tcp_accepted.SendAll(std::span<const std::uint8_t>(p, sz));
  }

  bool WriteInt32(std::int32_t v) {
    std::uint8_t buf[4];
    for (auto i = 0; i < 4; ++i)
      buf[i] = static_cast<std::uint8_t>(
          (static_cast<std::uint32_t>(v) >> (i * 8)) & 0xFF);
    return WriteAll(buf, sizeof(buf));
  }

  /// 服务端 → 客户端:from 字段 + 载荷(Connection.Receive 的读线程格式)
  /// Server → client: the from field + payload (the reader-thread format
  /// of Connection.Receive).
  bool SendToClient(std::int32_t int4_from,
                    const std::vector<std::uint8_t>& vec_payload) {
    return WriteInt32(static_cast<std::int32_t>(vec_payload.size())) &&
           WriteInt32(int4_from) &&
           WriteAll(vec_payload.data(), vec_payload.size());
  }

  bool ReadInt32(std::int32_t& int4_out) {
    return tcp_accepted.ReadInt32(int4_out);
  }

  /// 客户端 → 服务端:len + 载荷(SendRaw 的写格式)
  /// Client → server: len + payload (SendRaw's write format).
  bool ReadFromClient(std::vector<std::uint8_t>& vec_payload) {
    auto int4_len = std::int32_t{0};
    if (!ReadInt32(int4_len) || int4_len < 0)
      return false;
    vec_payload.resize(static_cast<std::size_t>(int4_len));
    return tcp_accepted.ReceiveExact(vec_payload.data(), vec_payload.size());
  }

  void Fail(std::string_view str_what) {
    str_failures += "FAIL: ";
    str_failures += str_what;
    str_failures += '\n';
  }

  platform::TcpSocket tcp_accepted;
};

std::vector<std::uint8_t> MakeSyncPayload(int int4_frame, int int4_hash,
                                          std::uint64_t uint8_defeat) {
  auto vec = std::vector<std::uint8_t>{};
  AppendInt32(vec, int4_frame);
  vec.push_back(0x65);
  AppendInt32(vec, int4_hash);
  AppendInt64(vec, static_cast<std::int64_t>(uint8_defeat));
  return vec;
}

}  // namespace

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::println("[network] connection_target face");

  const auto target_default = net::ConnectionTarget{};
  CheckEq(target_default.ToString(), "invalid:0",
          "default ConnectionTarget form");

  const auto target_pair = net::ConnectionTarget{
      std::vector<net::DnsEndpoint>{{"a", 1}, {"b", 2}}};
  CheckEq(target_pair.ToString(), "a:1/b:2", "ToString joins with /");

  auto b_threw = false;
  try {
    const auto empty = net::ConnectionTarget{std::vector<net::DnsEndpoint>{}};
    (void)empty;
  } catch (const std::invalid_argument& ex) {
    b_threw = true;
    CheckEq(std::string(ex.what()),
            "ConnectionTarget must have at least one address.",
            "empty-endpoint ctor text");
  }
  Check(b_threw, "empty-endpoint ctor throws");

  const auto vec_resolved =
      net::ConnectionTarget{"127.0.0.1", 12345}.GetConnectEndPoints();
  Check(vec_resolved.size() == 1, "loopback resolves to one endpoint");
  if (!vec_resolved.empty())
    CheckEq(vec_resolved.front().str_display, "127.0.0.1:12345",
            "resolved display string");

  const auto vec_unresolvable =
      net::ConnectionTarget{"no.such.host.invalid.", 12345}
          .GetConnectEndPoints();
  Check(vec_unresolvable.empty(),
        "unresolvable host resolves to empty (upstream catch → empty)");

  std::println("[network] loopback NetworkConnection chain");
  auto server = FakeServer{};
  server.tcp_listen = platform::TcpSocket::Listen(0);
  server.int4_port = server.tcp_listen.BoundPort();
  Check(server.int4_port != 0, "fake server binds ephemeral port");
  if (server.int4_port == 0)
    return 1;

  auto server_thread = std::thread([&server] {
    server.tcp_accepted = server.tcp_listen.Accept();
    if (!server.tcp_accepted.IsValid()) {
      server.Fail("accept failed");
      return;
    }

    if (!server.WriteInt32(net::kHandshakeProtocolVersion) ||
        !server.WriteInt32(1))
      return server.Fail("handshake write");

    auto vec_payload = std::vector<std::uint8_t>{};
    if (!server.ReadFromClient(vec_payload))
      return server.Fail("first client read");
    if (vec_payload.size() < 4 || ReadInt32At(vec_payload, 0) != 5)
      server.Fail("first order packet frame != 5");
    server.vec_first_order_payload = vec_payload;

    auto vec_ack = std::vector<std::uint8_t>{};
    AppendInt32(vec_ack, 6);
    vec_ack.push_back(0x10);
    vec_ack.push_back(1);
    if (!server.SendToClient(0, vec_ack))
      return server.Fail("ack write");

    if (!server.ReadFromClient(vec_payload))
      return server.Fail("immediate read");
    if (vec_payload.size() < 4 || ReadInt32At(vec_payload, 0) != 0)
      server.Fail("immediate packet frame != 0");

    if (!server.ReadFromClient(vec_payload))
      return server.Fail("order(7) read");
    if (vec_payload.size() < 4 || ReadInt32At(vec_payload, 0) != 7)
      server.Fail("second order packet frame != 7");
    if (!server.ReadFromClient(vec_payload))
      return server.Fail("sync flush read");
    if (ReadInt32At(vec_payload, 0) != 6 ||
        vec_payload.size() != 17 || vec_payload[4] != 0x65 ||
        ReadInt32At(vec_payload, 5) != 0x2ABCD ||
        ReadInt64At(vec_payload, 9) != 0x1122334455667788LL)
      server.Fail("flushed sync payload mismatch");

    if (!server.SendToClient(2, MakeSyncPayload(6, 0x2ABCD, 0x1122334455667788ULL)))
      return server.Fail("matching sync echo");

    if (!server.ReadFromClient(vec_payload))
      return server.Fail("order(9) read");
    if (!server.ReadFromClient(vec_payload))
      return server.Fail("sync(8) flush read");
    if (!server.SendToClient(
            2, MakeSyncPayload(8, 0x2ABCD ^ 1, 0x1122334455667788ULL)))
      return server.Fail("mismatched sync echo");

    auto vec_scale = std::vector<std::uint8_t>{};
    AppendInt32(vec_scale, 0);
    vec_scale.push_back(0x76);
    AppendFloat(vec_scale, 1.5f);
    if (!server.SendToClient(0, vec_scale))
      return server.Fail("tickscale write");

    auto vec_ping = std::vector<std::uint8_t>{};
    AppendInt32(vec_ping, 0);
    vec_ping.push_back(0x20);
    AppendInt64(vec_ping, 0x0123456789ABCDEFLL);
    if (!server.SendToClient(0, vec_ping))
      return server.Fail("ping request write");

    if (!server.ReadFromClient(vec_payload))
      return server.Fail("ping response read");
    // 响应比请求多一尾字节 queueLen(服务端 13,客户端响应 14)
    // The response carries one extra trailing queueLen byte (the
    // server-side request is 13, the client response 14).
    if (vec_payload.size() != 14 || vec_payload[4] != 0x20 ||
        ReadInt64At(vec_payload, 5) != 0x0123456789ABCDEFLL)
      server.Fail("ping response payload mismatch");

    auto vec_remote = std::vector<std::uint8_t>{};
    AppendInt32(vec_remote, 9);
    vec_remote.insert(vec_remote.end(),
                      server.vec_first_order_payload.begin() + 4,
                      server.vec_first_order_payload.end());
    if (!server.SendToClient(3, vec_remote))
      return server.Fail("remote order write");

    auto vec_remote_immediate = std::vector<std::uint8_t>{};
    AppendInt32(vec_remote_immediate, 0);
    vec_remote_immediate.insert(vec_remote_immediate.end(),
                                server.vec_first_order_payload.begin() + 4,
                                server.vec_first_order_payload.end());
    if (!server.SendToClient(4, vec_remote_immediate))
      return server.Fail("remote immediate write");

    auto vec_disconnect = std::vector<std::uint8_t>{};
    AppendInt32(vec_disconnect, 0);
    vec_disconnect.push_back(0xBF);
    AppendInt32(vec_disconnect, 4);
    if (!server.SendToClient(0, vec_disconnect))
      return server.Fail("disconnect write");

    auto vec_ack_overflow = std::vector<std::uint8_t>{};
    AppendInt32(vec_ack_overflow, 10);
    vec_ack_overflow.push_back(0x10);
    vec_ack_overflow.push_back(3);
    if (!server.SendToClient(0, vec_ack_overflow))
      return server.Fail("ack overflow write");

    server.tcp_accepted.Close();
  });

  auto up_connection = std::make_unique<net::NetworkConnection>(
      net::ConnectionTarget{"127.0.0.1", server.int4_port});
  auto& connection = *up_connection;

  Check(connection.State() == net::ConnectionState::Connecting ||
            connection.State() == net::ConnectionState::Connected,
        "initial state is Connecting/Connected");
  Check(WaitUntil([&] { return connection.State() ==
                               net::ConnectionState::Connected; },
                  5000),
        "connection reaches Connected");
  Check(connection.LocalClientId() == 1, "server-assigned client id");
  CheckEq(connection.EndPoint(), "127.0.0.1:" + std::to_string(server.int4_port),
          "EndPoint materialized at connect");

  connection.StartRecording([] { return std::string{"network_test"}; });

  auto om = net::OrderManager{std::move(up_connection)};

  // 溢出 Ack 可能被任一驱动循环消费 —— 统一安全 tick,首异常即捕获
  // The overflow Ack may be consumed by any driving loop — one safe
  // tick helper captures the first exception.
  auto b_overflow_threw = false;
  auto str_overflow_msg = std::string{};
  std::function<void()> tick = [&] {
    if (b_overflow_threw)
      return;
    try {
      om.TickImmediate();
    } catch (const std::runtime_error& ex) {
      b_overflow_threw = true;
      str_overflow_msg = ex.what();
    }
  };

  // 上游装配语义:StartGame 把大厅非 bot 客户端登记进 pendingOrders,
  // 否则 ReceiveOrders 视为 disconnected client 抛出(OrderManager.cs L98-116)
  // The upstream assembly semantics: StartGame registers the lobby's
  // non-bot clients into pendingOrders — otherwise ReceiveOrders treats
  // the sender as a disconnected client and throws (OrderManager.cs
  // L98-116).
  for (auto int4_index = 1; int4_index <= 4; ++int4_index) {
    auto client = net::SessionClient{};
    client.Index = int4_index;
    client.Name = "client" + std::to_string(int4_index);
    om.LobbyInfo().vec_clients.push_back(client);
  }
  om.StartGame();
  Check(om.GameStarted(), "StartGame seeds pending orders");

  auto vec_orders = std::vector<net::Order>{net::Order::Chat("first")};
  auto vec_ptrs = std::vector<const net::Order*>{&vec_orders[0]};
  std::println("[network] order + ack roundtrip");
  connection.Send(5, vec_ptrs);
  tick();
  Check(!om.IsOutOfSync(), "ack processing stays in sync");

  connection.SendImmediate({&vec_orders[0]});
  tick();

  auto vec_orders2 = std::vector<net::Order>{net::Order::Chat("second")};
  std::println("[network] sync flush + echo");
  connection.SendSync(6, 0x2ABCD, 0x1122334455667788ULL);
  connection.Send(7, {&vec_orders2[0]});
  tick();
  Check(!om.IsOutOfSync(), "local + echoed sync stay in sync");

  auto vec_orders3 = std::vector<net::Order>{net::Order::Chat("third")};
  connection.SendSync(8, 0x2ABCD, 0x1122334455667788ULL);
  connection.Send(9, {&vec_orders3[0]});
  tick();

  Check(WaitUntil([&] {
          tick();
          return om.IsOutOfSync();
        }, 5000),
        "mismatched sync echo triggers desync");

  std::println("[network] tickscale + ping");
  tick();
  tick();

  std::println("[network] remote packets + disconnect + ack overflow");
  tick();
  Check(WaitUntil([&] {
          tick();
          return HasRecordedFrame(
              ParseRecorderBytes(connection.Recorder()->Bytes()), 3, 9);
        }, 2000),
        "remote order packet reaches recorder");
  Check(WaitUntil([&] {
          tick();
          return HasRecordedFrame(
              ParseRecorderBytes(connection.Recorder()->Bytes()), 4, 0);
        }, 2000),
        "remote immediate packet reaches recorder");

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  tick();
  Check(b_overflow_threw, "ack overflow throws");
  CheckEq(str_overflow_msg, "Received Ack for 3 > 2 frames.",
          "ack overflow message text");

  Check(WaitUntil([&] {
          return connection.State() == net::ConnectionState::NotConnected;
        }, 5000),
        "server close drives NotConnected");
  CheckEq(connection.ErrorMessage(), "Connection failed",
          "reader-thread failure message");

  const auto vec_entries =
      ParseRecorderBytes(connection.Recorder()->Bytes());
  Check(HasRecordedFrame(vec_entries, 1, 6),
        "recorder holds the ack-processed packet at frame 6");
  Check(HasRecordedFrame(vec_entries, 1, 0),
        "recorder holds the local immediate order");

  connection.Dispose();
  connection.Dispose();

  server_thread.join();
  if (!server.str_failures.empty()) {
    std::print("{}", server.str_failures);
    g_failures += static_cast<int>(
        std::count(server.str_failures.begin(), server.str_failures.end(),
                   '\n'));
  }

  if (g_failures == 0)
    std::println("network_test: all passed");
  else
    std::println("network_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
