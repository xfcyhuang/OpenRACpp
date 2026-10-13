// UPSTREAM: 测试面(Phase 7 第十四批):回环真 socket 的服务器全链验收
//  —— 握手(协议 7 + 令牌)/HandshakeResponse 六门/槽位装配与
//  SyncInfo/Chat 广播与洪水闸/Ack 前投影/StartGame 空帧注入/服务器侧
//  desync 检测(录像作废)/DropClient 断连包与 Fluent 通知/Ping 往返/
//  OrderBuffer 的 TickScale 结算/专用服 RecordFakeHandshake/
//  GameInformation 往返/FluentMessage 与 Sanitize 的逐字面。
//  The test face (Phase 7 batch 14): the loopback real-socket full-chain
//  acceptance of the server — the handshake (protocol 7 + the token)/the
//  HandshakeResponse six gates/the slot assembly and SyncInfo/the Chat
//  broadcast and the flood gate/the Ack forward projection/StartGame's
//  empty-frame injection/the server-side desync detection (the recording
//  invalidated)/DropClient's disconnect packet and Fluent notifications/
//  the Ping round-trip/OrderBuffer's TickScale settlement/the dedicated
//  RecordFakeHandshake/GameInformation's round-trip/FluentMessage's and
//  the Sanitize family's verbatim faces.
import std;

#include "net/order.hpp"
#include "net/session.hpp"
#include "net/fluent_message.hpp"
#include "net/network_connection.hpp"
#include "net/order_manager.hpp"
#include "net/text_notifications.hpp"
#include "net/unit_orders.hpp"
#include "server/game_information.hpp"
#include "server/game_server.hpp"
#include "net/handshake.hpp"
#include "server/lobby_commands.hpp"
#include "server/master_server_pinger.hpp"
#include "server/order_buffer.hpp"
#include "server/player_pinger.hpp"
#include "server/protocol_version.hpp"
#include "server/server.hpp"
#include "server/server_settings.hpp"

#include "platform/tcp_socket.hpp"
#include "platform/udp_socket.hpp"

#include <cstdio>

namespace srv = ora::server;
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

struct FramePacket {
  int int4_client = 0;
  int int4_frame = 0;
  std::vector<std::uint8_t> vec_data;
};

/// 测试侧迷你客户端:阻塞读 + 帧状态机(与服务器 Connection 的读循环
/// 同构)
/// The test-side mini client: a blocking read + the frame state machine
/// (isomorphic to the server Connection's read loop).
struct TestClient {
  platform::TcpSocket tcp;
  int int4_client_id = -1;
  std::deque<std::uint8_t> deque_read;
  std::vector<FramePacket> vec_frames;
  bool b_eof = false;

  bool Connect(int int4_port) {
    const auto vec_eps = platform::ResolveDns("127.0.0.1", int4_port);
    if (vec_eps.empty())
      return false;
    tcp = platform::TcpSocket::Connect(vec_eps.front());
    if (!tcp.IsValid())
      return false;

    std::int32_t int4_handshake = 0;
    std::int32_t int4_id = 0;
    return tcp.ReadInt32(int4_handshake) && tcp.ReadInt32(int4_id) &&
           int4_handshake == srv::kHandshakeProtocol &&
           (int4_client_id = int4_id, true);
  }

  /// 客户端上行帧 = [len][frame][data](OrderPacket.Serialize(frame) 前置
  /// 长度;client id 不上线 —— 服务器按连接记账)
  /// The client's uplink frame = [len][frame][data] (OrderPacket.Serialize(
  /// frame) with a length prefix; the client id never rides the wire —
  /// the server accounts per connection).
  void SendPacket(int int4_frame, const std::vector<std::uint8_t>& vec_data) {
    auto w = net::ByteWriter{};
    w.Write(static_cast<std::int32_t>(vec_data.size() + 4));
    w.Write(int4_frame);
    w.WriteBytes(vec_data);
    tcp.SendAll(w.Bytes());
  }

  void SendOrderPacket(int int4_frame, const net::Order& order) {
    SendPacket(int4_frame, order.Serialize());
  }

  void SendSyncPacket(int int4_frame, int int4_hash,
                      std::uint64_t uint8_defeat) {
    auto vec = std::vector<std::uint8_t>{};
    vec.push_back(static_cast<std::uint8_t>(net::OrderType::SyncHash));
    for (auto i = 0; i < 4; ++i)
      vec.push_back(static_cast<std::uint8_t>(
          (static_cast<std::uint32_t>(int4_hash) >> (i * 8)) & 0xFF));
    for (auto i = 0; i < 8; ++i)
      vec.push_back(static_cast<std::uint8_t>(
          (uint8_defeat >> (i * 8)) & 0xFF));
    SendPacket(int4_frame, vec);
  }

  void Pump(int int4_wait_ms) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(int4_wait_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      if (!tcp.PollReadable(10000))
        continue;

      auto buf = std::array<std::uint8_t, 4096>{};
      const int int4_n = tcp.ReceiveUpTo(buf.data(), buf.size());
      if (int4_n <= 0) {
        b_eof = true;
        return;
      }
      for (int i = 0; i < int4_n; ++i)
        deque_read.push_back(buf[static_cast<std::size_t>(i)]);

      while (true) {
        if (deque_read.size() < 8)
          break;
        const auto len = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(deque_read[0]) |
            (static_cast<std::uint32_t>(deque_read[1]) << 8) |
            (static_cast<std::uint32_t>(deque_read[2]) << 16) |
            (static_cast<std::uint32_t>(deque_read[3]) << 24));
        const auto client = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(deque_read[4]) |
            (static_cast<std::uint32_t>(deque_read[5]) << 8) |
            (static_cast<std::uint32_t>(deque_read[6]) << 16) |
            (static_cast<std::uint32_t>(deque_read[7]) << 24));
        // 服务器下行帧:len = frame(4) + data;8 字节头([len][client])后
        // 的剩余恰为 len 字节
        // The server's downlink frame: len = frame(4) + data; the bytes
        // after the 8-byte header ([len][client]) number exactly len.
        const auto payload = len;
        if (payload < 0 || deque_read.size() < 8 + static_cast<std::size_t>(payload))
          break;

        for (int i = 0; i < 4; ++i)
          deque_read.pop_front();
        auto packet = FramePacket{};
        packet.int4_client = client;
        for (int i = 0; i < 4; ++i)
          deque_read.pop_front();
        for (auto i = 0; i < payload; ++i) {
          packet.vec_data.push_back(deque_read.front());
          deque_read.pop_front();
        }
        const auto frame = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(packet.vec_data[0]) |
            (static_cast<std::uint32_t>(packet.vec_data[1]) << 8) |
            (static_cast<std::uint32_t>(packet.vec_data[2]) << 16) |
            (static_cast<std::uint32_t>(packet.vec_data[3]) << 24));
        packet.vec_data.erase(packet.vec_data.begin(),
                              packet.vec_data.begin() + 4);
        packet.int4_frame = frame;
        vec_frames.push_back(std::move(packet));
      }
    }
  }

  const FramePacket* FindFrame(int int4_frame) const {
    for (const auto& f : vec_frames)
      if (f.int4_frame == int4_frame)
        return &f;
    return nullptr;
  }

  /// 命中任一 Order 载荷帧(Fields/Handshake;裸 Ack/Ping/SyncHash/
  /// TickScale/Disconnect 帧跳过)
  /// Hits any Order-bearing frame (Fields/Handshake; the raw Ack/Ping/
  /// SyncHash/TickScale/Disconnect frames are skipped).
  const FramePacket* FindOrderFrame(std::string_view str_order) const {
    for (const auto& f : vec_frames) {
      if (f.vec_data.empty())
        continue;
      const auto first = f.vec_data[0];
      if (first == static_cast<std::uint8_t>(net::OrderType::Ack) ||
          first == static_cast<std::uint8_t>(net::OrderType::Ping) ||
          first == static_cast<std::uint8_t>(net::OrderType::SyncHash) ||
          first == static_cast<std::uint8_t>(net::OrderType::TickScale) ||
          first == static_cast<std::uint8_t>(net::OrderType::Disconnect))
        continue;
      auto reader = net::ByteReader{f.vec_data};
      auto o = net::Order::Deserialize(nullptr, reader);
      if (o && o->str_order_string == str_order)
        return &f;
    }
    return nullptr;
  }

  void Handshake(const net::HandshakeResponse& response) {
    auto order = net::Order("HandshakeResponse", nullptr, false);
    order.type = net::OrderType::Handshake;
    order.b_is_immediate = true;
    order.str_target_string = response.Serialize();
    SendOrderPacket(0, order);
  }
};

net::HandshakeResponse MakeResponse(std::string_view str_name) {
  auto response = net::HandshakeResponse{};
  response.Mod = "ra";
  response.Version = "test";
  response.OrdersProtocol = srv::kOrdersProtocol;
  response.Client.Name = std::string{str_name};
  return response;
}

srv::ServerDeps MakeDeps(std::vector<std::string>* p_vec_log = nullptr) {
  auto deps = srv::ServerDeps{};
  deps.str_mod_id = "ra";
  deps.str_mod_version = "test";
  deps.fn_player_reference = [](std::string_view str_slot)
      -> const ora::map::PlayerReference* {
    static const std::map<std::string, ora::map::PlayerReference> kRefs = [] {
      std::map<std::string, ora::map::PlayerReference> refs;
      auto& multi0 = refs["Multi0"];
      multi0.LockColor = true;
      multi0.Color = ora::core::Color::FromArgb(255, 255, 0, 0);
      multi0.LockTeam = true;
      multi0.Team = 1;
      refs["Multi1"];
      return refs;
    }();
    const auto it = kRefs.find(std::string{str_slot});
    return it != kRefs.end() ? &it->second : nullptr;
  };
  deps.fn_create_server_players =
      [](net::Session& lobby_info,
         std::vector<std::optional<srv::GameInformationPlayer>>& vec_out,
         ora::MersenneTwister& player_random) {
        static_cast<void>(player_random);
        vec_out.push_back(std::nullopt);
        for (const auto& slot : lobby_info.vec_slots) {
          auto* p_client = lobby_info.ClientInSlot(slot.first);
          if (p_client == nullptr)
            continue;
          auto player = srv::GameInformationPlayer{};
          player.ClientIndex = p_client->Index;
          player.Name = p_client->Name;
          player.IsHuman = !p_client->IsBot();
          player.FactionName = "Allies";
          player.FactionId = "allies";
          player.Color = p_client->Color;
          player.Team = p_client->Team;
          vec_out.push_back(std::move(player));
        }
        vec_out.push_back(std::nullopt);
      };
  deps.fn_game_speeds = [] {
    static const ora::game::GameSpeeds kView = [] {
      auto speeds = ora::game::GameSpeeds{};
      speeds.DefaultSpeed = "default";
      speeds.Speeds = std::vector<std::pair<std::string, ora::game::GameSpeed>>{
          {"default", {"default", 40, 3}}};
      return speeds;
    }();
    return &kView;
  };
  if (p_vec_log != nullptr)
    deps.fn_server_log = [p_vec_log](std::string_view str_line) {
      p_vec_log->emplace_back(str_line);
    };
  return deps;
}

void FillSlots(srv::Server& server) {
  std::lock_guard lock{server.mutex_lobby};
  server.LobbyInfo.vec_slots.push_back({"Multi0", net::SessionSlot{}});
  server.LobbyInfo.vec_slots.push_back({"Multi1", net::SessionSlot{}});
}

int FreePort() {
  auto tcp = platform::TcpSocket::ListenEndpoint("127.0.0.1", 0);
  return tcp.BoundPort();
}

std::unique_ptr<srv::Server> MakeServer(int int4_port, srv::ServerType type,
                                        srv::ServerSettings settings,
                                        std::vector<std::string>* p_vec_log) {
  auto deps = MakeDeps(p_vec_log);
  auto server = std::make_unique<srv::Server>(
      std::vector<std::pair<std::string, int>>{{"127.0.0.1", int4_port}},
      std::move(settings), std::move(deps), type);
  FillSlots(*server);
  return server;
}

// ———— 1. 握手 + 六门 + 槽位装配 + SyncInfo ————
// 1. The handshake + the six gates + the slot assembly + SyncInfo.
void TestHandshakeChain() {
  std::println("[server] handshake chain");
  auto vec_log = std::vector<std::string>{};
  const auto int4_port = FreePort();
  auto settings = srv::ServerSettings{};
  settings.Name = "Test#Server";
  auto server = MakeServer(int4_port, srv::ServerType::Local, settings,
                           &vec_log);
  Check(server->IsValid(), "server listens");
  CheckEq(server->Settings.Name, "TestServer", "sanitized server name");

  auto client = TestClient{};
  Check(client.Connect(int4_port), "client connects");
  Check(client.int4_client_id == 0, "first client index 0");

  client.Pump(1500);
  Check(client.FindOrderFrame("HandshakeRequest") != nullptr,
        "handshake request order");
  client.Handshake(MakeResponse("Bob#1"));

  auto b_synced = WaitUntil(
      [&] {
        client.Pump(200);
        return client.FindOrderFrame("SyncInfo") != nullptr;
      },
      5000);
  Check(b_synced, "SyncInfo after validation");

  const auto* p_sync = client.FindOrderFrame("SyncInfo");
  Check(p_sync != nullptr, "SyncInfo present");
  if (p_sync != nullptr) {
    auto reader = net::ByteReader{p_sync->vec_data};
    auto o = net::Order::Deserialize(nullptr, reader);
    Check(o && o->str_target_string, "SyncInfo order payload");
    if (o && o->str_target_string) {
      auto session = net::Session::Deserialize(*o->str_target_string, "sync");
      Check(session.vec_clients.size() == 1, "one client in lobby");
      if (!session.vec_clients.empty()) {
        const auto& c = session.vec_clients.front();
        CheckEq(c.Name, "Bob1", "sanitized player name");
        CheckEq(c.Slot, "Multi0", "first empty slot");
        Check(c.IsAdmin, "first client admin");
        CheckEq(c.Faction, "Random", "faction starts Random");
        Check(c.Color.ToString() == ora::core::Color::FromArgb(255, 255, 0, 0).ToString(),
              "locked slot color applied");
        Check(c.Team == 1, "locked slot team applied");
        Check(c.State == net::ClientState::Invalid, "state starts Invalid");
      }
    }
  }

  server->Shutdown();
}

// ———— 2. 拒绝面:错误 mod / 错误协议 / 密码 ————
// 2. The rejection faces: wrong mod / wrong protocol / password.
void TestValidationGates() {
  std::println("[server] validation gates");
  const auto int4_port = FreePort();
  auto vec_log = std::vector<std::string>{};
  auto settings = srv::ServerSettings{};
  settings.Password = "pw";
  auto server = MakeServer(int4_port, srv::ServerType::Local, settings, &vec_log);

  {
    auto client = TestClient{};
    Check(client.Connect(int4_port), "gate client connects");
    client.Pump(1500);
    auto response = MakeResponse("Bob");
    response.Password = "wrong";
    client.Handshake(response);
    auto b_rejected = WaitUntil(
        [&] {
          client.Pump(200);
          return client.FindOrderFrame("AuthenticationError") != nullptr;
        },
        5000);
    Check(b_rejected, "wrong password → AuthenticationError");
  }

  {
    auto client = TestClient{};
    Check(client.Connect(int4_port), "mod gate client connects");
    client.Pump(1500);
    auto response = MakeResponse("Bob");
    response.Password = "pw";
    response.Mod = "cnc";
    client.Handshake(response);
    auto b_rejected = WaitUntil(
        [&] {
          client.Pump(200);
          return client.FindOrderFrame("ServerError") != nullptr;
        },
        5000);
    Check(b_rejected, "wrong mod → ServerError");
  }

  {
    auto client = TestClient{};
    Check(client.Connect(int4_port), "protocol gate client connects");
    client.Pump(1500);
    auto response = MakeResponse("Bob");
    response.Password = "pw";
    response.OrdersProtocol = 7;
    client.Handshake(response);
    auto b_rejected = WaitUntil(
        [&] {
          client.Pump(200);
          return client.FindOrderFrame("ServerError") != nullptr;
        },
        5000);
    Check(b_rejected, "old orders protocol → ServerError");
  }

  auto b_mod_log = WaitUntil(
      [&] {
        for (const auto& line : vec_log)
          if (line.find("mods do not match") != std::string::npos)
            return true;
        return false;
      },
      1000);
  Check(b_mod_log, "rejection log line");

  server->Shutdown();
}

// ———— 3. Chat 广播 + MP 洪水闸 ————
// 3. The Chat broadcast + the MP flood gate.
void TestChatAndFlood() {
  std::println("[server] chat + flood gate");
  const auto int4_port = FreePort();
  auto settings = srv::ServerSettings{};
  settings.FloodLimitJoinCooldown = 0;
  settings.FloodLimitMessageCount = 2;
  auto server = MakeServer(int4_port, srv::ServerType::Multiplayer, settings,
                           nullptr);

  auto a = TestClient{};
  auto b = TestClient{};
  Check(a.Connect(int4_port) && b.Connect(int4_port), "two clients connect");
  a.Pump(1500);
  b.Pump(1500);
  a.Handshake(MakeResponse("Alice"));
  b.Handshake(MakeResponse("Bob"));

  auto b_both = WaitUntil(
      [&] {
        a.Pump(100);
        b.Pump(100);
        return a.FindOrderFrame("SyncInfo") != nullptr &&
               b.FindOrderFrame("SyncInfo") != nullptr;
      },
      5000);
  Check(b_both, "both validated");

  a.SendOrderPacket(0, net::Order::Chat("hello"));
  auto b_chat = WaitUntil(
      [&] {
        b.Pump(150);
        for (const auto& f : b.vec_frames) {
          if (f.vec_data.empty() ||
              f.vec_data[0] != static_cast<std::uint8_t>(net::OrderType::Fields))
            continue;
          auto reader = net::ByteReader{f.vec_data};
          auto o = net::Order::Deserialize(nullptr, reader);
          if (o && o->str_order_string == "Chat" &&
              o->str_target_string == "hello")
            return true;
        }
        return false;
      },
      5000);
  Check(b_chat, "chat forwarded to peer");

  auto HasChatTempWarning = [](const TestClient& c) {
    for (const auto& f : c.vec_frames) {
      if (f.vec_data.empty() || f.vec_data[0] != static_cast<std::uint8_t>(net::OrderType::Fields))
        continue;
      auto reader = net::ByteReader{f.vec_data};
      auto o = net::Order::Deserialize(nullptr, reader);
      if (o && o->str_order_string == "FluentMessage" && o->str_target_string &&
          o->str_target_string->find("notification-chat-temp-disabled") !=
              std::string::npos)
        return true;
    }
    return false;
  };

  Check(!HasChatTempWarning(a), "under limit: no chat-temp warning yet");

  a.SendOrderPacket(0, net::Order::Chat("two"));
  a.SendOrderPacket(0, net::Order::Chat("three"));
  auto b_warned = WaitUntil(
      [&] {
        a.Pump(150);
        return HasChatTempWarning(a) &&
               a.FindOrderFrame("DisableChatEntry") != nullptr;
      },
      5000);
  Check(b_warned, "flood limit: chat disabled fluent + order");

  server->Shutdown();
}

// ———— 4. StartGame:空帧注入 + Ack 前投影 + 转发 + SyncHash 记账 ————
// 4. StartGame: the empty-frame injection + the Ack forward projection +
// the forwarding + the SyncHash accounting.
void TestStartGameOrders() {
  std::println("[server] start game + orders");
  const auto int4_port = FreePort();
  auto server = MakeServer(int4_port, srv::ServerType::Local,
                           srv::ServerSettings{}, nullptr);

  auto a = TestClient{};
  Check(a.Connect(int4_port), "orders client connects");
  a.Pump(1500);
  a.Handshake(MakeResponse("Alice"));
  WaitUntil(
      [&] {
        a.Pump(100);
        return a.FindOrderFrame("SyncInfo") != nullptr;
      },
      5000);

  server->PostEvent([&] {
    auto* p_client = server->LobbyInfo.ClientWithIndex(a.int4_client_id);
    if (p_client != nullptr)
      p_client->State = net::ClientState::Ready;
    server->StartGame();
  });

  auto b_started = WaitUntil(
      [&] {
        a.Pump(150);
        return a.FindOrderFrame("StartGame") != nullptr;
      },
      5000);
  Check(b_started, "StartGame order dispatched");
  Check(server->State() == srv::ServerState::GameStarted, "state GameStarted");
  Check(server->GameInfo() != nullptr &&
            server->GameInfo()->vec_players.size() == 1,
        "worldPlayers: 1 playable of 3 slots");

  // Local 服 OrderLatency = 1:空帧注入帧 1。
  // A Local server's OrderLatency = 1: the empty-frame injection at
  // frame 1.
  auto b_empty = WaitUntil(
      [&] {
        a.Pump(150);
        return a.FindFrame(1) != nullptr;
      },
      5000);
  Check(b_empty, "empty frame 1 injected");

  // 世界 order 帧 5 → Ack(帧 6)+ 他人转发帧 6。
  // A world order at frame 5 → the Ack (frame 6) + the peer forward at
  // frame 6.
  auto order = net::Order("TestOrder", nullptr, false);
  a.SendOrderPacket(5, order);
  auto b_ack = WaitUntil(
      [&] {
        a.Pump(150);
        for (const auto& f : a.vec_frames)
          if (f.int4_frame == 6 && f.vec_data.size() == 2 &&
              f.vec_data[0] == static_cast<std::uint8_t>(net::OrderType::Ack) &&
              f.vec_data[1] == 1)
            return true;
        return false;
      },
      5000);
  Check(b_ack, "Ack frame 6 count 1");

  // SyncHash 帧 10:Local 无录像,记账副作用 = 静默接受(无 desync)。
  // The SyncHash at frame 10: no Local recording, so the accounting's
  // observable face is silent acceptance (no desync).
  a.SendSyncPacket(10, 12345, 0);
  a.Pump(500);
  Check(server->State() == srv::ServerState::GameStarted,
        "sync order accepted silently");

  server->Shutdown();
}

// ———— 5. 服务器侧 desync 检测(专用服 + 录像)————
// 5. The server-side desync detection (dedicated + recording).
void TestServerDesync() {
  std::println("[server] server-side desync");
  const auto int4_port = FreePort();
  auto vec_log = std::vector<std::string>{};
  auto settings = srv::ServerSettings{};
  settings.RecordReplays = true;
  auto server = std::make_unique<srv::Server>(
      std::vector<std::pair<std::string, int>>{{"127.0.0.1", int4_port}},
      settings, MakeDeps(&vec_log), srv::ServerType::Dedicated);
  FillSlots(*server);
  Check(server->Recorder() != nullptr, "dedicated recorder started");

  // RecordFakeHandshake 的两条帧 0 条目。
  // RecordFakeHandshake's two frame-0 entries.
  const auto& vec_bytes = server->Recorder()->Bytes();
  Check(vec_bytes.size() >= 16, "fake handshake recorded");
  auto b_handshake_entries = false;
  {
    auto at = std::size_t{0};
    auto count = 0;
    while (at + 8 <= vec_bytes.size()) {
      const auto len = static_cast<std::int32_t>(
          static_cast<std::uint32_t>(vec_bytes[at + 4]) |
          (static_cast<std::uint32_t>(vec_bytes[at + 5]) << 8) |
          (static_cast<std::uint32_t>(vec_bytes[at + 6]) << 16) |
          (static_cast<std::uint32_t>(vec_bytes[at + 7]) << 24));
      at += 8;
      if (len < 0 || at + static_cast<std::size_t>(len) > vec_bytes.size())
        break;
      // ReceiveFrame 的载荷 = [frame][data],帧 0 即四零字节
      // ReceiveFrame's payload = [frame][data]; a frame-0 prefix is four
      // zero bytes.
      if (len > 4 &&
          vec_bytes[at + 4] == static_cast<std::uint8_t>(net::OrderType::Handshake))
        ++count;
      at += static_cast<std::size_t>(len);
    }
    b_handshake_entries = count == 2;
  }
  Check(b_handshake_entries, "two handshake order entries");

  auto a = TestClient{};
  auto b = TestClient{};
  Check(a.Connect(int4_port) && b.Connect(int4_port), "desync clients connect");
  a.Pump(1500);
  b.Pump(1500);
  a.Handshake(MakeResponse("Alice"));
  b.Handshake(MakeResponse("Bob"));
  WaitUntil(
      [&] {
        a.Pump(100);
        b.Pump(100);
        return a.FindOrderFrame("SyncInfo") != nullptr &&
               b.FindOrderFrame("SyncInfo") != nullptr;
      },
      5000);

  server->PostEvent([&] {
    for (auto* p_client : server->LobbyInfo.NonBotClients())
      p_client->State = net::ClientState::Ready;
    server->StartGame();
  });
  WaitUntil(
      [&] {
        a.Pump(100);
        b.Pump(100);
        return a.FindOrderFrame("StartGame") != nullptr &&
               b.FindOrderFrame("StartGame") != nullptr;
      },
      5000);

  a.SendSyncPacket(10, 111, 0);
  b.SendSyncPacket(10, 222, 0);
  auto b_desync = WaitUntil(
      [&] {
        for (const auto& line : vec_log)
          if (line.find("Out of sync detected at frame 10") !=
              std::string::npos)
            return true;
        return false;
      },
      5000);
  Check(b_desync, "server desync log");
  auto b_recorder_gone = WaitUntil(
      [&] { return server->Recorder() == nullptr; }, 5000);
  Check(b_recorder_gone, "recorder invalidated");

  server->Shutdown();
}

// ———— 6. DropClient:断连包 + Fluent 通知 + 观察者流 ————
// 6. DropClient: the disconnect packet + the Fluent notification + the
// observer flow.
void TestDropClient() {
  std::println("[server] drop client");
  const auto int4_port = FreePort();
  auto server = MakeServer(int4_port, srv::ServerType::Local,
                           srv::ServerSettings{}, nullptr);

  auto a = TestClient{};
  auto b = TestClient{};
  Check(a.Connect(int4_port) && b.Connect(int4_port), "drop clients connect");
  a.Pump(1500);
  b.Pump(1500);
  a.Handshake(MakeResponse("Alice"));
  b.Handshake(MakeResponse("Bob"));
  WaitUntil(
      [&] {
        a.Pump(100);
        b.Pump(100);
        return a.FindOrderFrame("SyncInfo") != nullptr &&
               b.FindOrderFrame("SyncInfo") != nullptr;
      },
      5000);
  {
    // accept 次序竞态防护:显式钉住 a 为管理员
    // An accept-order race guard: pin a as the admin explicitly.
    std::lock_guard lock{server->mutex_lobby};
    auto* p_a = server->LobbyInfo.ClientWithIndex(0);
    auto* p_b = server->LobbyInfo.ClientWithIndex(1);
    const auto int4_a = a.int4_client_id;
    const auto int4_b = b.int4_client_id;
    if (p_a != nullptr && p_b != nullptr && int4_a != 0) {
      p_a->Index = int4_b;
      p_b->Index = int4_a;
      std::swap(p_a->Slot, p_b->Slot);
    }
    for (auto& c : server->LobbyInfo.vec_clients)
      c.IsAdmin = c.Index == int4_a;
  }

  a.tcp.Close();

  auto b_notified = WaitUntil(
      [&] {
        b.Pump(200);
        return b.FindOrderFrame("FluentMessage") != nullptr;
      },
      8000);
  Check(b_notified, "peer gets lobby-disconnect fluent");

  auto b_disconnect = WaitUntil(
      [&] {
        b.Pump(200);
        for (const auto& f : b.vec_frames)
          if (f.vec_data.size() == 5 &&
              f.vec_data[0] == static_cast<std::uint8_t>(net::OrderType::Disconnect))
            return true;
        return false;
      },
      8000);
  Check(b_disconnect, "disconnect order packet at LastOrdersFrame+1");

  auto b_admin = WaitUntil(
      [&] {
        std::lock_guard lock{server->mutex_lobby};
        const auto* p = server->LobbyInfo.ClientWithIndex(1);
        return p != nullptr && p->IsAdmin;
      },
      5000);
  Check(!b_admin, "non-dedicated admin drop shuts server down (no reassign)");
  Check(server->State() == srv::ServerState::ShuttingDown,
        "local server shuts down when admin leaves");

  server->Shutdown();
}

// ———— 7. Ping 往返 + OrderBuffer 结算 ————
// 7. The Ping round-trip + the OrderBuffer settlement.
void TestPingAndOrderBuffer() {
  std::println("[server] ping + order buffer");
  const auto int4_port = FreePort();
  auto server = MakeServer(int4_port, srv::ServerType::Local,
                           srv::ServerSettings{}, nullptr);

  auto a = TestClient{};
  Check(a.Connect(int4_port), "ping client connects");
  a.Pump(1500);
  a.Handshake(MakeResponse("Alice"));
  WaitUntil(
      [&] {
        a.Pump(100);
        return a.FindOrderFrame("SyncInfo") != nullptr;
      },
      5000);

  // 服务器连接线程每秒发 Ping;回显时间戳即计入历史。
  // The server's connection thread pings every second; echoing the
  // timestamp feeds the history.
  auto b_pinged = WaitUntil(
      [&] {
        a.Pump(300);
        for (const auto& f : a.vec_frames)
          if (f.vec_data.size() == 9 &&
              f.vec_data[0] == static_cast<std::uint8_t>(net::OrderType::Ping))
            return true;
        return false;
      },
      5000);
  Check(b_pinged, "server ping received");

  if (b_pinged) {
    for (const auto& f : a.vec_frames) {
      if (f.vec_data.size() == 9 &&
          f.vec_data[0] == static_cast<std::uint8_t>(net::OrderType::Ping)) {
        auto vec = std::vector<std::uint8_t>{f.vec_data};
        vec.push_back(0);
        a.SendPacket(0, vec);
        break;
      }
    }
  }
  a.Pump(300);

  server->Shutdown();

  auto buffer = srv::OrderBuffer{};
  auto speed = ora::game::GameSpeed{"default", 40, 3};
  buffer.Start(speed, {1});
  Check(buffer.GetTickScales().empty(), "window unfilled: no scales");
  for (auto i = 0; i < srv::OrderBuffer::kNumberOfFrames; ++i)
    buffer.AddOrderTimestamp(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  const auto vec_scales = buffer.GetTickScales();
  Check(vec_scales.size() == 1 && vec_scales.front().first == 1,
        "single player scale row");
  Check(!vec_scales.empty() && vec_scales.front().second == 1.0f,
        "self-baseline scale 1.0");
  buffer.RemovePlayer(1);
  Check(buffer.GetTickScales().empty(), "removed player: no scales");
}

// ———— 8. 纯逻辑面:FluentMessage/Handshake/GameInformation/Sanitize ————
// 8. The pure-logic faces: FluentMessage/Handshake/GameInformation/
// Sanitize.
void TestPureLogic() {
  std::println("[server] pure logic faces");

  const auto str_fluent = net::FluentMessage::Serialize(
      "notification-joined",
      std::vector<srv::FluentArgument>{{"player", std::string{"Bob"}},
                                       {"team", std::string{"2"},
                                        srv::FluentArgumentType::Number}});
  const auto str_expected = std::string{
      "FluentMessage:\n"
      "\tProtocol: 1\n"
      "\tKey: notification-joined\n"
      "\tArguments:\n"
      "\t\tArgument@0:\n"
      "\t\t\tKey: player\n"
      "\t\t\tValue: Bob\n"
      "\t\t\tType: String\n"
      "\t\tArgument@1:\n"
      "\t\t\tKey: team\n"
      "\t\t\tValue: 2\n"
      "\t\t\tType: Number"};
  CheckEq(str_fluent, str_expected, "FluentMessage wire text");

  auto request = net::HandshakeRequest{};
  request.Mod = "ra";
  request.Version = "1.0";
  request.AuthToken = "tok";
  const auto str_request = request.Serialize();
  const auto back_request =
      net::HandshakeRequest::Deserialize(str_request, "h");
  Check(back_request && back_request->Mod == "ra" &&
            back_request->Version == "1.0" && back_request->AuthToken == "tok",
        "HandshakeRequest roundtrip");

  auto response = MakeResponse("Zed");
  response.Password = "pw";
  response.Fingerprint = "fp";
  const auto back_response =
      net::HandshakeResponse::Deserialize(response.Serialize(), "h");
  Check(back_response && back_response->Mod == "ra" &&
            back_response->Password == "pw" &&
            back_response->OrdersProtocol == srv::kOrdersProtocol &&
            back_response->Client.Name == "Zed",
        "HandshakeResponse roundtrip");

  auto info = srv::GameInformation{};
  info.Mod = "ra";
  info.Version = "test";
  info.MapUid = "ABC";
  info.MapTitle = "Great Sahara";
  info.FinalGameTick = 42;
  info.StartTimeUtc = srv::GameDateTime::UtcNow();
  info.vec_disabled_spawn_points = {3, 5};
  auto player = srv::GameInformationPlayer{};
  player.ClientIndex = 1;
  player.Name = "Alice";
  player.IsHuman = true;
  player.FactionId = "allies";
  player.Outcome = srv::WinState::Won;
  info.vec_players.push_back(player);
  const auto back_info = srv::GameInformation::Deserialize(info.Serialize(), "g");
  Check(back_info && back_info->MapUid == "ABC" && back_info->FinalGameTick == 42 &&
            back_info->vec_players.size() == 1 &&
            back_info->vec_players.front().Name == "Alice" &&
            back_info->vec_players.front().Outcome == srv::WinState::Won &&
            back_info->vec_disabled_spawn_points ==
                std::vector<int>{3, 5},
        "GameInformation roundtrip");

  CheckEq(srv::SanitizedPlayerName("X#Y:Z", "Commander"), "XYZ",
          "sanitize strips reserved");
  CheckEq(srv::SanitizedPlayerName("Open", "Commander"), "Commander",
          "forbidden name falls back");
  CheckEq(srv::SanitizedPlayerName("01234567890123456789", "Commander"),
          "0123456789012345", "16 char truncation");
  CheckEq(srv::ServerSettings{}.SanitizedServerName("  ", "Commander"),
          "Commander's Game", "empty server name fallback");

  auto session = net::Session{};
  session.vec_slots.push_back({"Multi0", net::SessionSlot{}});
  session.vec_slots.push_back({"Multi1", net::SessionSlot{}});
  auto occupant = net::SessionClient{};
  occupant.Slot = "Multi0";
  session.vec_clients.push_back(occupant);
  CheckEq(session.FirstEmptySlot(), "Multi1", "FirstEmptySlot skips occupied");
  session.vec_slots.back().second.Closed = true;
  CheckEq(session.FirstEmptySlot(), "", "all closed/skip → empty");
}

}  // namespace

// ———— 15. 第十五批:LobbyCommands/Pinger 族/LanBeacon/GameServer ————
// 15. Batch 15: LobbyCommands / the Pingers / LanBeacon / GameServer.
namespace {

/// 倒序找 (order, target) 命中帧 | The reverse scan for an (order, target)
/// hit frame.
const FramePacket* FindOrderWithTarget(const TestClient& client,
                                       std::string_view str_order,
                                       std::string_view str_target_contains) {
  for (auto it = client.vec_frames.rbegin(); it != client.vec_frames.rend();
       ++it) {
    if (it->vec_data.empty())
      continue;
    const auto first = it->vec_data[0];
    if (first == static_cast<std::uint8_t>(net::OrderType::Ack) ||
        first == static_cast<std::uint8_t>(net::OrderType::Ping) ||
        first == static_cast<std::uint8_t>(net::OrderType::SyncHash) ||
        first == static_cast<std::uint8_t>(net::OrderType::TickScale) ||
        first == static_cast<std::uint8_t>(net::OrderType::Disconnect))
      continue;
    auto reader = net::ByteReader{it->vec_data};
    auto o = net::Order::Deserialize(nullptr, reader);
    if (o && o->str_order_string == str_order && o->str_target_string &&
        o->str_target_string->find(str_target_contains) !=
            std::string::npos)
      return &*it;
  }
  return nullptr;
}

void SendCommand(TestClient& client, std::string_view str_cmd) {
  auto order = net::Order{"Command", nullptr, false};
  order.str_target_string = std::string{str_cmd};
  client.SendOrderPacket(0, order);
}

std::unique_ptr<srv::Server> MakeLobbyServer(
    int int4_port, srv::ServerSettings settings,
    std::vector<std::string>* p_vec_log) {
  auto server = MakeServer(int4_port, srv::ServerType::Local, settings,
                           p_vec_log);
  server->AddServerTrait(std::make_unique<srv::LobbyCommands>());
  return server;
}

bool JoinLobby(TestClient& client, int int4_port, std::string_view str_name) {
  if (!client.Connect(int4_port))
    return false;
  client.Pump(300);
  client.Handshake(MakeResponse(str_name));
  return WaitUntil(
      [&] {
        client.Pump(150);
        return client.FindOrderFrame("SyncInfo") != nullptr;
      },
      5000);
}

void TestLobbyCommandsBatch() {
  std::println("[server] lobby commands batch");
  auto vec_log = std::vector<std::string>{};
  const auto int4_port = FreePort();
  auto settings = srv::ServerSettings{};
  settings.EnableSingleplayer = true;
  settings.Name = "Test#Server";
  auto server = MakeLobbyServer(int4_port, settings, &vec_log);

  auto alice = TestClient{};
  Check(JoinLobby(alice, int4_port, "Alice"), "alice joins");
  auto bob = TestClient{};
  Check(JoinLobby(bob, int4_port, "Bob"), "bob joins");

  // ———— state 命令(Ready 物化 + SyncLobbyClients 广播)————
  // The state command (Ready materialized + the SyncLobbyClients broadcast).
  alice.vec_frames.clear();
  SendCommand(alice, "state Ready");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "SyncLobbyClients",
                                         "State: Ready") != nullptr;
            },
            1500),
        "state Ready reflected in SyncLobbyClients");

  // Ready 后非 state 命令被拒
  // A non-state command is rejected once Ready.
  alice.vec_frames.clear();
  SendCommand(alice, "name Alice2");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(
                         alice, "FluentMessage",
                         "notification-state-unchanged-ready") != nullptr;
            },
            1500),
        "ready-state command gate");

  SendCommand(alice, "state NotReady");

  // ———— name 命令(通知 + SyncLobbyClients)————
  // The name command (the notification + SyncLobbyClients).
  alice.vec_frames.clear();
  SendCommand(alice, "name  Alice#2 ");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-nick-changed") !=
                     nullptr;
            },
            1500),
        "nick-changed notification");

  // ———— allow_spectators 畸形(上游 allow_spectate 文本)————
  // allow_spectators' malformed branch (upstream's allow_spectate text).
  alice.vec_frames.clear();
  SendCommand(alice, "allow_spectators maybe");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-malformed-command") !=
                     nullptr;
            },
            1500),
        "allow_spectators malformed");

  // ———— faction:无 map = 一切非法
  // faction: without a map everything is invalid.
  alice.vec_frames.clear();
  SendCommand(alice, "faction 0 allies");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(
                         alice, "FluentMessage",
                         "notification-invalid-faction-selected") != nullptr;
            },
            1500),
        "invalid faction rejected");

  // ———— assignteams 畸形数字
  // assignteams' malformed number.
  alice.vec_frames.clear();
  SendCommand(alice, "assignteams two");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-error-number-teams") !=
                     nullptr;
            },
            1500),
        "assignteams malformed");

  // ———— slot_bot:占位人类 = invalid-bot-slot;空槽 + IBotInfo 空集 =
  //      invalid-bot-type
  // slot_bot: a human occupant = invalid-bot-slot; an empty slot + the
  // empty IBotInfo set = invalid-bot-type.
  alice.vec_frames.clear();
  SendCommand(alice, "slot_bot Multi0 0 NullBot");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-invalid-bot-slot") !=
                     nullptr;
            },
            1500),
        "slot_bot occupied slot rejected");
  {
    std::lock_guard lock{server->mutex_lobby};
    net::SessionSlot slot_extra{};
    slot_extra.PlayerReference = "Multi2";
    server->LobbyInfo.vec_slots.push_back({"Multi2", slot_extra});
  }
  alice.vec_frames.clear();
  SendCommand(alice, "slot_bot Multi2 0 NullBot");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-invalid-bot-type") !=
                     nullptr;
            },
            1500),
        "slot_bot invalid type");

  // ———— map:缓存空 = unknown-map(池外同样)
  // map: the empty cache = unknown-map (the out-of-pool the same).
  alice.vec_frames.clear();
  SendCommand(alice, "map " + std::string(40, 'a'));
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-unknown-map") !=
                     nullptr;
            },
            1500),
        "map unknown uid rejected");

  // ———— clear_spawn:空点点名禁用 → SyncInfo;再点恢复
  // clear_spawn: an empty spawn toggles to disabled → SyncInfo; toggling
  // again restores it.
  alice.vec_frames.clear();
  SendCommand(alice, "clear_spawn 2");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "SyncInfo",
                                         "DisabledSpawnPoints: 2") != nullptr;
            },
            1500),
        "clear_spawn disables empty spawn");
  alice.vec_frames.clear();
  SendCommand(alice, "clear_spawn 2");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              const auto* p_frame =
                  FindOrderWithTarget(alice, "SyncInfo", "DisabledSpawnPoints");
              return p_frame != nullptr &&
                     FindOrderWithTarget(alice, "SyncInfo",
                                         "DisabledSpawnPoints: 2") ==
                         nullptr;
            },
            1500),
        "clear_spawn toggles back");

  // ———— make_admin:转交 + 广播
  // make_admin: the handover + the broadcast.
  alice.vec_frames.clear();
  SendCommand(alice, "make_admin 1");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-new-admin") != nullptr;
            },
            1500),
        "make_admin broadcast");
  {
    std::lock_guard lock{server->mutex_lobby};
    const auto* p_alice = server->LobbyInfo.ClientWithIndex(0);
    const auto* p_bob = server->LobbyInfo.ClientWithIndex(1);
    Check(p_alice != nullptr && !p_alice->IsAdmin, "alice no longer admin");
    Check(p_bob != nullptr && p_bob->IsAdmin, "bob is admin");
  }

  // ———— kick:temp ban 快照 + ServerError 回执
  // kick: the temp-ban snapshot + the ServerError receipt.
  {
    std::lock_guard lock{server->mutex_lobby};
    if (auto* p_alice = server->LobbyInfo.ClientWithIndex(0))
      p_alice->IsAdmin = true;
    if (auto* p_bob = server->LobbyInfo.ClientWithIndex(1))
      p_bob->IsAdmin = false;
  }
  bob.vec_frames.clear();
  alice.vec_frames.clear();
  SendCommand(alice, "kick 1 True");
  Check(WaitUntil(
            [&] {
              bob.Pump(150);
              return FindOrderWithTarget(bob, "ServerError",
                                         "notification-you-were-kicked") !=
                     nullptr;
            },
            1500),
        "kick delivers you-were-kicked");
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-temp-ban") != nullptr;
            },
            1500),
        "kick temp-ban notification");
  {
    std::lock_guard lock{server->mutex_lobby};
    Check(server->vec_temp_bans.size() == 1 &&
              !server->vec_temp_bans.front().empty(),
          "temp ban recorded from IPAddress snapshot");
    Check(server->LobbyInfo.ClientWithIndex(1) == nullptr,
          "kicked client removed from lobby");
  }

  // ———— sync_lobby:管理员装载合法 Session(往返)————
  // sync_lobby: the admin loads a valid Session (the round-trip).
  std::string str_session;
  {
    std::lock_guard lock{server->mutex_lobby};
    str_session = server->LobbyInfo.Serialize();
  }
  alice.vec_frames.clear();
  SendCommand(alice, "sync_lobby " + str_session);
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "SyncInfo",
                                         "Name: Alice2") != nullptr;
            },
            1500),
        "sync_lobby valid reload");
  {
    std::lock_guard lock{server->mutex_lobby};
    Check(server->LobbyInfo.ClientWithIndex(0) != nullptr,
          "reload keeps clients");
  }

  // ———— sync_lobby:字段装载失败 → invalid-lobby-info(装载保持原状)
  // sync_lobby: a field-load failure → invalid-lobby-info (the load is
  // abandoned).
  alice.vec_frames.clear();
  SendCommand(alice, std::string{"sync_lobby Client@0:\n\tIndex: NaN"});
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-invalid-lobby-info") !=
                     nullptr;
            },
            1500),
        "sync_lobby invalid rejected");
  {
    std::lock_guard lock{server->mutex_lobby};
    Check(server->LobbyInfo.ClientWithIndex(0) != nullptr,
          "invalid load leaves lobby intact");
  }

  alice.tcp.Close();
  bob.tcp.Close();
  server->Shutdown();

  // ———— vote_kick:无战况面(HasClientWonOrLost 恒 true)→ 管理员一票
  //      即踢
  // vote_kick: without a game-info face (HasClientWonOrLost constantly
  // true) an admin's single vote kicks at once.
  const auto int4_vk_port = FreePort();
  auto settings_vk = srv::ServerSettings{};
  settings_vk.EnableSingleplayer = true;
  auto server_vk = MakeLobbyServer(int4_vk_port, settings_vk, &vec_log);

  auto voter = TestClient{};
  Check(JoinLobby(voter, int4_vk_port, "Voter"), "voter joins");
  auto target = TestClient{};
  Check(JoinLobby(target, int4_vk_port, "Target"), "target joins");
  {
    std::lock_guard lock{server_vk->mutex_lobby};
    if (auto* p_voter = server_vk->LobbyInfo.ClientWithIndex(0))
      p_voter->IsAdmin = true;
  }
  server_vk->SetState(srv::ServerState::GameStarted);

  voter.vec_frames.clear();
  target.vec_frames.clear();
  SendCommand(voter, "vote_kick 1 True");
  Check(WaitUntil(
            [&] {
              target.Pump(150);
              return FindOrderWithTarget(target, "ServerError",
                                         "notification-you-were-kicked") !=
                     nullptr;
            },
            1500),
        "vote kick drops target");
  Check(WaitUntil(
            [&] {
              voter.Pump(150);
              return FindOrderWithTarget(voter, "FluentMessage",
                                         "notification-kicked") != nullptr;
            },
            1500),
        "vote kick kicked notification (dead-kickee shortcut)");
  {
    std::lock_guard lock{server_vk->mutex_lobby};
    Check(server_vk->LobbyInfo.ClientWithIndex(1) == nullptr,
          "vote-kicked client gone");
  }
  voter.tcp.Close();
  server_vk->Shutdown();
}

void TestPlayerPingerDrop() {
  std::println("[server] player pinger drop");
  auto vec_log = std::vector<std::string>{};
  const auto int4_port = FreePort();
  auto settings = srv::ServerSettings{};
  settings.EnableSingleplayer = true;
  settings.Name = "Test#Server";
  auto server = MakeLobbyServer(int4_port, settings, &vec_log);

  auto alice = TestClient{};
  Check(JoinLobby(alice, int4_port, "Alice"), "alice joins");
  auto bob = TestClient{};
  Check(JoinLobby(bob, int4_port, "Bob"), "bob joins");
  auto claire = TestClient{};
  Check(JoinLobby(claire, int4_port, "Claire"), "claire joins");

  // 中位 client 的 LobbyInfo 条目移除 → PlayerPinger.Tick 踢无主连接
  // (alice+claire 保持 ≥2 非_bot 计数门开)
  // The middle client's LobbyInfo entry removed → PlayerPinger.Tick drops
  // the client-less connection (alice+claire keep the >=2 non-bot count
  // gate open).
  {
    std::lock_guard lock{server->mutex_lobby};
    const auto int4_bob = bob.int4_client_id;
    auto& vec = server->LobbyInfo.vec_clients;
    for (auto it = vec.begin(); it != vec.end(); ++it)
      if (it->Index == int4_bob) {
        vec.erase(it);
        break;
      }
  }

  alice.vec_frames.clear();
  auto pinger = srv::PlayerPinger{};
  pinger.Tick(*server);
  Check(WaitUntil(
            [&] {
              alice.Pump(150);
              return FindOrderWithTarget(alice, "FluentMessage",
                                         "notification-player-dropped") !=
                     nullptr;
            },
            5000),
        "player-dropped notification after client-less kick");
  {
    std::lock_guard lock{server->mutex_lobby};
    Check(server->LobbyInfo.ClientWithIndex(bob.int4_client_id) ==
                  nullptr &&
              server->LobbyInfo.ClientWithIndex(0) != nullptr,
          "only the client-less connection dropped");
  }
  alice.tcp.Close();
  claire.tcp.Close();
  server->Shutdown();
}

void TestMasterServerPingerAndGameServer() {
  std::println("[server] master pinger + game server");
  auto vec_posted = std::vector<std::pair<std::string, std::string>>{};
  auto str_response = std::string{"[0]Welcome"};
  auto b_online = true;

  auto deps = srv::MasterServerPingerDeps{};
  deps.fn_advertise_endpoint = [] { return std::string{"http://master/test"}; };
  deps.fn_post = [&](std::string str_endpoint, std::string str_data,
                     std::function<void(bool, std::string)> fn_done) {
    vec_posted.emplace_back(str_endpoint, str_data);
    fn_done(b_online, str_response);
  };

  const auto int4_port = FreePort();
  auto settings = srv::ServerSettings{};
  settings.AdvertiseOnLocalNetwork = false;
  auto server = MakeServer(int4_port, srv::ServerType::Multiplayer, settings,
                           nullptr);
  server->AddServerTrait(std::make_unique<srv::MasterServerPinger>(
      std::move(deps)));

  auto alice = TestClient{};
  Check(JoinLobby(alice, int4_port, "Alice"), "alice joins");

  server->Shutdown();
  Check(WaitUntil([&] { return vec_posted.size() >= 2; }, 5000),
        "initial + shutdown advertisements posted");
  bool b_protocol2 = false;
  for (const auto& [str_endpoint, str_data] : vec_posted)
    if (str_data.find("Protocol: 2") != std::string::npos)
      b_protocol2 = true;
  Check(b_protocol2, "GameServer POST carries Protocol 2");
  Check(vec_posted.back().second.find("Client@0") != std::string::npos,
        "shutdown advertisement carries the Client@0 node");

  // ———— ToPOSTData 的 LAN 附加面(直构)————
  // ToPOSTData's LAN extras (constructed directly).
  {
    std::lock_guard lock{server->mutex_lobby};
    auto gs = srv::GameServerData{*server};
    CheckEq(gs.Address, std::format("0.0.0.0:{}", server->Settings.ListenPort),
            "game server address");
    Check(gs.State == static_cast<int>(srv::ServerState::ShuttingDown),
          "state field captured");
    const auto str_online = gs.ToPOSTData(false);
    const auto str_lan = gs.ToPOSTData(true);
    Check(str_online.find("\nMods: ra@test") == std::string::npos,
          "online POST omits the legacy Mods field");
    Check(str_lan.find("Mods: ra@test") != std::string::npos,
          "LAN POST carries the legacy Mods field");
    Check(str_lan.find("Players: 1") != std::string::npos,
          "LAN POST players count");
    Check(str_lan.find("Protected: False") != std::string::npos,
          "LAN POST protected flag");
    Check(str_online.find("Protected: False") != std::string::npos,
          "online POST protected flag");
  }
}

// ———— MasterServerPinger 的 fn_post 缺省直调(boost::beast + boost::url;
//      回环伪 HTTP 服务器全链:POST 行/Host 头/StringContent 默认媒体型/
//      body 的 Protocol 2/initial ping 的 [0] 应答解析)————
// MasterServerPinger's fn_post default by the direct call (boost::beast +
// boost::url; the loopback fake-HTTP-server full chain: the POST line/the
// Host header/the StringContent default media type/the body's Protocol 2/
// the initial ping's [0]-reply parse).
void TestMasterServerPingerDefaultBeastPost() {
  std::println("[server] master pinger default beast post");
  auto vec_log = std::vector<std::string>{};
  auto mutex_log = std::mutex{};

  auto listener = platform::TcpSocket::Listen(0);
  Check(listener.IsValid(), "fake HTTP server listens");
  const auto int4_http_port = listener.BoundPort();

  // 上游对一次会话恰两投(事件循环 Tick 的 initial + ServerShutdown 的
  // final),伪服务器须逐连接服务每一个,否则另一投滞留 backlog、收尾
  // RST 后其 detach 回调越过本函数栈(窄窗悬垂 + 时序 flaky)。
  // Upstream posts exactly twice per session (the event loop Tick's
  // initial + ServerShutdown's final), so the fake server must serve
  // every connection: the other post would otherwise sit in the backlog
  // and, after the shutdown RST, its detached callback would outlive this
  // function's stack (a narrow dangling window + timing flakiness).
  auto str_request = std::string{};
  auto mutex_request = std::mutex{};
  auto atomic_served = std::atomic<int>{0};
  auto thread_serve = std::thread{[&] {
    while (true) {
      auto client = listener.Accept();
      if (!client.IsValid())
        return;
      auto str_req = std::string{};
      const auto read_more = [&]() {
        std::array<std::uint8_t, 4096> arr_buf{};
        const auto int4_n = client.ReceiveUpTo(arr_buf.data(), arr_buf.size());
        if (int4_n <= 0)
          return false;
        str_req.append(reinterpret_cast<const char*>(arr_buf.data()),
                       static_cast<std::size_t>(int4_n));
        return true;
      };
      auto b_complete = true;
      while (str_req.find("\r\n\r\n") == std::string::npos) {
        if (!client.PollReadable(3000000) || !read_more()) {
          b_complete = false;
          break;
        }
      }
      if (b_complete) {
        auto int4_content_length = 0;
        const auto pos_length = str_req.find("Content-Length:");
        if (pos_length != std::string::npos) {
          const auto pos_length_end = str_req.find("\r\n", pos_length);
          std::from_chars(str_req.data() + pos_length + 15,
                          str_req.data() + pos_length_end,
                          int4_content_length);
        }
        const auto pos_body = str_req.find("\r\n\r\n") + 4;
        while (str_req.size() - pos_body <
               static_cast<std::size_t>(int4_content_length)) {
          if (!client.PollReadable(3000000) || !read_more()) {
            b_complete = false;
            break;
          }
        }
      }
      if (b_complete) {
        const auto str_reply = std::string{"[0]Beast OK"};
        const auto str_response =
            std::format("HTTP/1.1 200 OK\r\nContent-Length: {}\r\n\r\n{}",
                        str_reply.size(), str_reply);
        client.SendAll(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(str_response.data()),
            str_response.size()});
        atomic_served.fetch_add(1);
        {
          const std::lock_guard lock{mutex_request};
          if (str_request.empty())
            str_request = str_req;
        }
      }
    }
  }};

  auto settings = srv::ServerSettings{};
  settings.AdvertiseOnLocalNetwork = false;
  auto deps = srv::MasterServerPingerDeps{};
  deps.fn_advertise_endpoint = [int4_http_port] {
    return std::format("http://127.0.0.1:{}/master", int4_http_port);
  };
  deps.fn_server_log = [&](std::string_view sv_line) {
    const std::lock_guard lock{mutex_log};
    vec_log.emplace_back(sv_line);
  };

  const auto int4_port = FreePort();
  auto server = MakeServer(int4_port, srv::ServerType::Multiplayer, settings,
                           nullptr);
  server->AddServerTrait(std::make_unique<srv::MasterServerPinger>(
      std::move(deps)));
  server->Shutdown();

  const auto b_beast_seen = WaitUntil([&] {
    const std::lock_guard lock{mutex_log};
    return std::any_of(vec_log.begin(), vec_log.end(),
                       [](const std::string& str_line) {
                         return str_line.find(
                                    "Master server: [0]Beast OK") !=
                                std::string::npos;
                       });
  }, 5000);
  if (!b_beast_seen) {
    const std::lock_guard lock{mutex_log};
    for (const auto& str_line : vec_log)
      std::println("beast-debug log: {}", str_line);
    std::println("beast-debug request ({} bytes): {}",
                 str_request.size(), str_request);
  }
  Check(b_beast_seen, "beast POST round-trip reaches the initial-ping log");

  // 两投都被服务后收尾:served>=2 时两响应均已上线,POST 线程的 fn_done
  // 只差毫秒级收尾,100ms 余量让 detach 线程跑完再动本函数的栈;随后
  // Close 解除 serve 线程的 Accept 阻塞,join 方能返回(单投时序下靠
  // 4s 超时兜底)。
  // Wind down after both posts are served: once served >= 2 both replies
  // are on the wire, the POST threads' fn_done calls are milliseconds from
  // done, and the 100ms grace lets the detached threads finish before this
  // function's stack moves; Close then unblocks the serve thread's Accept
  // so join can return (the single-post timing falls back on the 4s
  // timeout).
  WaitUntil([&] { return atomic_served.load() >= 2; }, 4000);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  listener.Close();
  thread_serve.join();

  Check(str_request.starts_with("POST /master HTTP/1.1"),
        "beast request line carries the URL path");
  Check(str_request.find("Host: 127.0.0.1:" +
                         std::to_string(int4_http_port)) !=
            std::string::npos,
        "beast Host header carries the authority");
  Check(str_request.find("Content-Type: text/plain; charset=utf-8") !=
            std::string::npos,
        "StringContent's single-argument default media type");
  const auto pos_body_check = str_request.find("\r\n\r\n");
  Check(pos_body_check != std::string::npos &&
            str_request.substr(pos_body_check + 4).find("Protocol: 2") !=
                std::string::npos,
        "POST body carries Protocol 2");
}

void TestLanBeacon() {
  std::println("[server] lan beacon");
  auto beacon = srv::LanBeacon{"OpenRALANGame", 43210, nullptr};
  Check(beacon.IsValid(), "beacon binds discovery port");
  if (!beacon.IsValid())
    return;
  beacon.SetBeaconData("Game\n\tName: LanTest");
  beacon.Start();

  // 探测包:beaconType 编码前缀 → 应答 [type][port 大端 2B][data 编码]
  // The probe packet: the encoded beaconType prefix → the reply
  // [type][advertisedPort, 2B big-endian][the encoded data].
  auto udp = platform::UdpSocket::BindAny(0);
  Check(udp.IsValid(), "probe socket binds");
  if (!udp.IsValid())
    return;

  const auto vec_probe = [&] {
    auto vec = std::vector<std::uint8_t>{};
    const std::string str_type = "OpenRALANGame";
    const auto u2_len = static_cast<std::uint16_t>(str_type.size());
    vec.push_back(static_cast<std::uint8_t>(u2_len & 0xFF));
    vec.push_back(static_cast<std::uint8_t>((u2_len >> 8) & 0xFF));
    vec.insert(vec.end(), str_type.begin(), str_type.end());
    return vec;
  }();

  // 定向到 35891(回环) | Directed at 35891 (loopback).
  {
    const auto vec_eps = platform::ResolveDns("127.0.0.1", 35891);
    Check(!vec_eps.empty() && udp.SendTo(vec_eps.front(), vec_probe),
          "probe sent");
  }

  auto addr_peer = platform::SocketAddress{};
  auto arr_buf = std::array<std::uint8_t, 4096>{};
  int int4_n = -1;
  for (int i = 0; i < 100; ++i) {
    if (!udp.PollReadable(100000))
      continue;
    int4_n = udp.ReceiveFrom(arr_buf.data(), arr_buf.size(), addr_peer);
    if (int4_n > 0)
      break;
  }
  Check(int4_n > static_cast<int>(vec_probe.size()), "beacon replied");
  if (int4_n > static_cast<int>(vec_probe.size())) {
    const auto u2_port =
        static_cast<std::uint16_t>((arr_buf[vec_probe.size()] << 8) |
                                   arr_buf[vec_probe.size() + 1]);
    CheckEq(std::to_string(u2_port), "43210", "beacon advertised port");
    // data 编码:2 字节小端长度 + UTF-8
    // The encoded data: a 2-byte little-endian length + UTF-8.
    const auto sz_data_begin = vec_probe.size() + 2;
    const auto u2_data_len =
        static_cast<std::uint16_t>(arr_buf[sz_data_begin] |
                                   (arr_buf[sz_data_begin + 1] << 8));
    const auto str_data = std::string{
        reinterpret_cast<const char*>(arr_buf.data() + sz_data_begin + 2),
        u2_data_len};
    Check(str_data.find("Name: LanTest") != std::string::npos,
          "beacon data round-trip");
  }
}

}  // namespace

// ———— 9. 客户端接线(NetworkConnection + OrderManager + ProcessOrder 的
// 真 socket 全链:HandshakeRequest 自动应答 → 槽位装配 → 命令上行 →
// SyncLobby*/SyncMapPool/FluentMessage 下行)————
// 9. The client wiring (the real-socket full chain of NetworkConnection +
// OrderManager + ProcessOrder: the HandshakeRequest auto-answer → the
// slot assembly → the command uplink → the SyncLobby*/SyncMapPool/
// FluentMessage downlink).
struct WiredClient {
  std::unique_ptr<net::OrderManager> om;
  std::mutex mutex_om;  // OM 单线程契约:发布线程与泵线程互斥 | the OM's
                        // single-thread contract: the posting thread and
                        // the pump thread mutually excluded.
  std::atomic<bool> b_stop{false};
  std::thread thread_pump;

  explicit WiredClient(int int4_port_target)
      : om(std::make_unique<net::OrderManager>(
            std::make_unique<net::NetworkConnection>(
                net::ConnectionTarget{"127.0.0.1", int4_port_target}))) {
    om->SetOrderProcessor(
        [](net::OrderManager& om_target, ora::sim::World* p_world,
           int int4_client, const net::Order& order_target) {
          net::ProcessOrder(om_target, p_world, int4_client, order_target);
        });
    thread_pump = std::thread([this] {
      while (!b_stop.load()) {
        {
          std::lock_guard lock{mutex_om};
          om->TickImmediate();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }

  /// 主线程发布 order 并即冲刷(与泵线程互斥)
  /// The main thread posts an order and flushes it right away (mutually
  /// excluded from the pump thread).
  void Post(const net::Order& order_new) {
    std::lock_guard lock{mutex_om};
    om->IssueOrder(order_new);
    om->TickImmediate();
  }

  /// 断言侧的互斥读 | the assertion side's mutually excluded read.
  template <class Fn>
  bool WithOm(Fn fn) {
    std::lock_guard lock{mutex_om};
    return fn(*om);
  }

  ~WiredClient() {
    b_stop.store(true);
    thread_pump.join();
  }
};

void TestClientWiring() {
  std::println("[server] client wiring");
  auto& tnm = net::TextNotificationsManager::Instance();
  tnm.ResetForTest();

  const auto int4_port = FreePort();
  auto settings = srv::ServerSettings{};
  settings.EnableSingleplayer = true;
  settings.Name = "Test#Server";
  auto server = MakeLobbyServer(int4_port, settings, nullptr);
  server->opt_map_pool = std::vector<std::string>{"a.oramap", "b.oramap"};

  auto face = net::ClientJoinFace{};
  face.str_mod_id = "ra";
  face.str_mod_version = "test";
  face.str_player_name = "Wired#Player";  // sanitize 剥 '#' → "WiredPlayer"
  face.color_player = ora::core::Color::FromArgb(255, 12, 34, 56);

  auto int4_changes = 0;
  net::SetLobbyInfoChangedHandler([&] { ++int4_changes; });

  auto make_client = [](net::ClientJoinFace face_join, int int4_port_target) {
    net::SetClientJoinFace(std::move(face_join));
    return std::make_unique<WiredClient>(int4_port_target);
  };

  auto alice = make_client(face, int4_port);
  Check(WaitUntil(
            [&] {
              return alice->WithOm([](net::OrderManager& m) {
                auto* p_client = m.LobbyInfo().ClientWithIndex(
                    m.Connection().LocalClientId());
                return p_client != nullptr &&
                       p_client->Name == "WiredPlayer" &&
                       !p_client->Slot.empty() &&
                       p_client->State == net::ClientState::Invalid;
              });
            },
            5000),
        "wired client auto-handshake + slot assembly");
  Check(int4_changes > 0, "LobbyInfoChanged fired on SyncInfo");
  Check(WaitUntil(
            [&] {
              return alice->WithOm([](net::OrderManager& m) {
                return m.ServerMapPool().has_value() &&
                       m.ServerMapPool()->size() == 2 &&
                       m.ServerMapPool()->count("a.oramap") == 1 &&
                       m.ServerMapPool()->count("b.oramap") == 1;
              });
            },
            5000),
        "SyncMapPool received");

  // ———— 命令上行:Order.Command → 服务器 LobbyCommands → SyncLobbyClients
  // 下行 → 本端 LobbyInfo 物化(name 命令 —— state 会触发 CheckAutoStart
  // 的失败复位:无图服务器上 Ready 即被回写 NotReady,上游 quirk)
  // The command uplink: Order.Command → the server's LobbyCommands → the
  // SyncLobbyClients downlink → the local LobbyInfo materialization (the
  // name command — state would trigger CheckAutoStart's failure reset:
  // on a mapless server Ready is written straight back to NotReady, the
  // upstream quirk).
  const auto int4_changes_before = int4_changes;
  alice->Post(net::Order::Command("name WiredPlayer2"));
  Check(WaitUntil(
            [&] {
              return alice->WithOm([](net::OrderManager& m) {
                auto* p_client = m.LobbyInfo().ClientWithIndex(
                    m.Connection().LocalClientId());
                return p_client != nullptr &&
                       p_client->Name == "WiredPlayer2";
              });
            },
            5000),
        "name uplink round-trip");

  // state Ready 的 CheckAutoStart 复位 quirk:无图 → 全员回写 NotReady
  // The state-Ready CheckAutoStart reset quirk: no map → everyone written
  // back to NotReady.
  alice->Post(net::Order::Command("state Ready"));
  Check(WaitUntil(
            [&] {
              return alice->WithOm([](net::OrderManager& m) {
                auto* p_client = m.LobbyInfo().ClientWithIndex(
                    m.Connection().LocalClientId());
                return p_client != nullptr &&
                       p_client->State == net::ClientState::NotReady;
              });
            },
            5000),
        "state Ready → auto-start reset → NotReady (upstream quirk)");
  Check(int4_changes > int4_changes_before,
        "SyncLobbyClients fired LobbyInfoChanged");

  // ———— 第二客户端:joined FluentMessage 通知 + Chat 广播 ————
  // The second client: the joined FluentMessage notification + the Chat
  // broadcast.
  auto face_bob = face;
  face_bob.str_player_name = "Bobba";
  auto bob = make_client(face_bob, int4_port);
  Check(WaitUntil(
            [&] {
              return alice->WithOm([](net::OrderManager& m) {
                for (const auto& c_bob : m.LobbyInfo().vec_clients)
                  if (c_bob.Name == "Bobba" && c_bob.Slot == "Multi1")
                    return true;
                return false;
              });
            },
            5000),
        "bob visible in alice lobby");
  Check(WaitUntil(
            [&] {
              for (const auto& n : tnm.Notifications())
                if (n.Pool() == net::TextNotificationPool::Join &&
                    n.Text() == "notification-joined")
                  return true;
              return false;
            },
            5000),
        "joined FluentMessage → Join pool notification");

  bob->Post(net::Order::Chat("hello from bob"));
  Check(WaitUntil(
            [&] {
              for (const auto& n : tnm.Notifications())
                if (n.Pool() == net::TextNotificationPool::Chat &&
                    n.Prefix() == "Bobba" && n.Text() == "hello from bob")
                  return true;
              return false;
            },
            5000),
        "chat broadcast → Chat pool line");

  net::ClearLobbyInfoChangedHandler();

  // ———— AuthenticationError / ServerError 的真实链 ————
  // The AuthenticationError / ServerError real chains.
  const auto int4_port_locked = FreePort();
  auto locked_settings = srv::ServerSettings{};
  locked_settings.EnableSingleplayer = true;
  locked_settings.Name = "Locked";
  locked_settings.Password = "sesame";
  auto server_locked = MakeServer(int4_port_locked, srv::ServerType::Local,
                                  locked_settings, nullptr);

  auto face_no_pass = face;
  face_no_pass.str_player_name = "NoPass";
  face_no_pass.str_password = "wrong";
  auto wrong_password = make_client(face_no_pass, int4_port_locked);
  Check(WaitUntil(
            [&] {
              return wrong_password->WithOm([](net::OrderManager& m) {
                return m.AuthenticationFailed() &&
                       !m.ServerError().empty();
              });
            },
            5000),
        "wrong password → AuthenticationError");

  auto face_alien = face;
  face_alien.str_mod_id = "cnc";
  // mod 门在无密码服务器上直验(密码门在前 —— 上游 ValidateClient 序)
  // The mod gate checks straight on the passwordless server (the password
  // gate precedes it — upstream's ValidateClient order).
  auto alien = make_client(face_alien, int4_port);
  Check(WaitUntil(
            [&] {
              return alien->WithOm([](net::OrderManager& m) {
                return !m.ServerError().empty() &&
                       !m.AuthenticationFailed();
              });
            },
            5000),
        "wrong mod → ServerError (AuthenticationFailed reset)");

  net::SetClientJoinFace(net::ClientJoinFace{});
}

// ———— 10. 客户端分支的纯逻辑面(无世界/无服务器:大厅期 Chat 变体/
// StartKickVote·EndKickVote 门/DisableChatEntry 算术/SyncConnectionQuality/
// GenerateMap 早退)————
// 10. The client branches' pure-logic faces (no world/no server: the
// lobby-phase Chat variants / the StartKickVote·EndKickVote gates / the
// DisableChatEntry arithmetic / SyncConnectionQuality / GenerateMap's
// early-outs).
void TestClientPureBranches() {
  std::println("[server] client pure branches");
  auto& tnm = net::TextNotificationsManager::Instance();
  tnm.ResetForTest();
  net::ClearUnitOrdersStatics();
  net::SetRuntimeMsFn([] { return std::int64_t{1000}; });

  auto om = net::OrderManager{std::make_unique<net::EchoConnection>()};
  om.LobbyInfo().vec_clients.push_back(net::SessionClient{});
  om.LobbyInfo().vec_clients.push_back(net::SessionClient{});
  om.LobbyInfo().vec_clients[0].Index = 2;
  om.LobbyInfo().vec_clients[0].Name = "Sender";
  om.LobbyInfo().vec_clients[0].Team = 2;
  om.LobbyInfo().vec_clients[0].Color =
      ora::core::Color::FromArgb(255, 1, 2, 3);
  om.LobbyInfo().vec_clients[1].Index = 1;  // EchoConnection 本地 | the local
  om.LobbyInfo().vec_clients[1].Name = "Local";
  om.LobbyInfo().vec_clients[1].Team = 2;
  om.LobbyInfo().vec_clients.push_back(net::SessionClient{});
  om.LobbyInfo().vec_clients.back().Index = 3;
  om.LobbyInfo().vec_clients.back().Name = "Foreign";
  om.LobbyInfo().vec_clients.back().Team = 5;

  auto run = [&](std::string_view str_name, std::string str_target,
                 std::uint32_t uint4_extra, int int4_client) {
    auto order = net::Order{std::string{str_name}, nullptr, false};
    order.str_target_string = std::move(str_target);
    order.uint4_extra_data = uint4_extra;
    net::ProcessOrder(om, nullptr, int4_client, order);
  };

  // 大厅期队伍聊天:同队可见(client.Team == local.Team);异队滤除。
  // 上游 lobby 分支只比 client.Team 与 local.Team —— ExtraData 的目标队
  // 号不参与(quirk 保真:同队发任意队号都上屏)
  // The lobby-phase team chat: same-team visible (client.Team ==
  // local.Team); cross-team filtered. Upstream's lobby branch compares
  // only client.Team vs local.Team — ExtraData's target team number never
  // joins in (the quirk kept: a same-team sender shows under any team
  // number).
  run("Chat", "[Team] secret", 2, 2);
  run("Chat", "[Spectators] hi", std::numeric_limits<std::uint32_t>::max(), 2);
  run("Chat", "[Team] quirk-extra", 3, 2);
  run("Chat", "[Team] hidden", 5, 3);
  bool b_team_line = false;
  bool b_spectator_line = false;
  bool b_quirk_extra_line = false;
  bool b_cross_team_hidden = true;
  for (const auto& n : tnm.Notifications()) {
    if (n.Pool() != net::TextNotificationPool::Chat)
      continue;
    if (n.Prefix() == "[Team] Sender" && n.Text() == "[Team] secret")
      b_team_line = true;
    if (n.Prefix() == "[Spectators] Sender")
      b_spectator_line = true;
    if (n.Text() == "[Team] quirk-extra")
      b_quirk_extra_line = true;
    if (n.Text() == "[Team] hidden")
      b_cross_team_hidden = false;
  }
  Check(b_team_line, "lobby team chat same-team line");
  Check(b_spectator_line, "lobby spectator-channel same-team line");
  Check(b_quirk_extra_line, "lobby same-team quirk: extra team number ignored");
  Check(b_cross_team_hidden, "lobby cross-team chat filtered");

  // 截断:>2500 字符裁到限
  // The truncation: over 2500 characters clipped.
  tnm.ResetForTest();
  run("Chat", std::string(2600, 'x'), 0, 2);
  Check(!tnm.Notifications().empty() &&
            tnm.Notifications().back().Text().size() == 2500,
        "chat hard-limit truncation");

  // StartKickVote/EndKickVote:非服务器源(id!=0)拒;匹配的 End 清除
  // StartKickVote/EndKickVote: a non-server source (id!=0) rejected; the
  // matching End clears.
  tnm.ResetForTest();
  run("StartKickVote", "", 3, 1);
  Check(!net::KickVoteTarget().has_value(), "kick vote from non-server ignored");
  run("StartKickVote", "", 3, 0);
  Check(net::KickVoteTarget().has_value() && *net::KickVoteTarget() == 3,
        "kick vote target set");
  run("EndKickVote", "", 4, 0);
  Check(net::KickVoteTarget().has_value() && *net::KickVoteTarget() == 3,
        "non-matching end vote keeps target");
  run("EndKickVote", "", 3, 0);
  Check(!net::KickVoteTarget().has_value(), "matching end vote clears");

  // DisableChatEntry:RunTime + ExtraData;MaxValue = 永久
  // DisableChatEntry: RunTime + ExtraData; MaxValue = permanent.
  tnm.SetChatDisabledUntil(0);
  run("DisableChatEntry", "", 500, 0);
  Check(tnm.ChatDisabledUntil() == 1500, "chat disabled until = runtime+extra");
  run("DisableChatEntry", "", std::numeric_limits<std::uint32_t>::max(), 0);
  Check(tnm.ChatDisabledUntil() ==
            std::numeric_limits<std::int64_t>::max(),
        "chat disabled permanently on MaxValue");
  run("DisableChatEntry", "", 500, 1);
  Check(tnm.ChatDisabledUntil() ==
            std::numeric_limits<std::int64_t>::max(),
        "chat disable from non-server ignored");

  // SyncConnectionQuality:服务器节点格式("ConnectionQuality@N: Poor")
  // SyncConnectionQuality: the server's node format.
  auto quality_order = net::Order{"SyncConnectionQuality", nullptr, false};
  quality_order.str_target_string = "ConnectionQuality@2: Poor";
  net::ProcessOrder(om, nullptr, 0, quality_order);
  Check(om.LobbyInfo().vec_clients[0].ConnectionQualityKind ==
            net::ConnectionQuality::Poor,
        "connection quality synced");

  // GenerateMap:未注入 MapCache 面 → 早退不抛
  // GenerateMap: with the MapCache face uninjected → the early-out throws
  // nothing.
  net::SetMapCacheAtFn(nullptr);
  auto generate_order = net::Order{"GenerateMap", nullptr, false};
  generate_order.str_target_string =
      "Uid: abc\nGenerator: gen\nTileset: temperat\nSize: 64,64\n"
      "Title: T\nAuthor: A";
  net::ProcessOrder(om, nullptr, 0, generate_order);

  // FluentMessage 解析回环:Serialize → FromYaml(参数保留)
  // The FluentMessage parse round-trip: Serialize → FromYaml (the
  // arguments preserved).
  const auto str_wire = net::FluentMessage::Serialize(
      "notification-kicked", std::vector<net::FluentArgument>{{"admin", "A"}});
  const auto vec_nodes =
      ora::yaml::MiniYaml::FromString(str_wire, "fluent");
  Check(!vec_nodes.empty(), "fluent wire parses");
  const auto message = net::FluentMessage::FromYaml(vec_nodes.front().Value);
  Check(message.Key() == "notification-kicked" &&
            message.Arguments().size() == 1 &&
            message.Arguments()[0].str_key == "admin" &&
            message.Arguments()[0].str_value == "A",
        "fluent message round-trip");

  net::SetRuntimeMsFn([] { return std::int64_t{0}; });

  // SyncLobbyClients 装载回环(服务器 SerializeSessionClient 线格式)
  // The SyncLobbyClients load round-trip (the server's
  // SerializeSessionClient wire format).
  om.LobbyInfo().vec_clients.clear();
  auto client_ready = net::SessionClient{};
  client_ready.Index = 4;
  client_ready.Name = "RoundTrip";
  client_ready.State = net::ClientState::Ready;
  client_ready.Slot = "Multi0";
  auto pool_rt = ora::yaml::StringPool{};
  auto vec_rt = std::vector<ora::yaml::MiniYamlNode>{};
  vec_rt.push_back(net::SerializeSessionClient(pool_rt, client_ready));
  auto sync_order = net::Order{"SyncLobbyClients", nullptr, false};
  sync_order.str_target_string = ora::yaml::WriteToString(vec_rt);
  net::ProcessOrder(om, nullptr, 0, sync_order);
  Check(om.LobbyInfo().vec_clients.size() == 1 &&
            om.LobbyInfo().vec_clients[0].Name == "RoundTrip" &&
            om.LobbyInfo().vec_clients[0].IsReady() &&
            om.LobbyInfo().vec_clients[0].Slot == "Multi0",
        "SyncLobbyClients round-trip");
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  TestPureLogic();
  TestHandshakeChain();
  TestValidationGates();
  TestChatAndFlood();
  TestStartGameOrders();
  TestServerDesync();
  TestDropClient();
  TestPingAndOrderBuffer();
  TestLobbyCommandsBatch();
  TestPlayerPingerDrop();
  TestMasterServerPingerAndGameServer();
  TestMasterServerPingerDefaultBeastPost();
  TestLanBeacon();
  TestClientWiring();
  TestClientPureBranches();

  if (g_failures != 0) {
    std::println("server_test: {} FAILURES", g_failures);
    return 1;
  }
  std::println("server_test: all checks passed");
  return 0;
}
