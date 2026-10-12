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
#include "server/fluent_message.hpp"
#include "server/game_information.hpp"
#include "server/handshake.hpp"
#include "server/order_buffer.hpp"
#include "server/protocol_version.hpp"
#include "server/server.hpp"
#include "server/server_settings.hpp"

#include "platform/tcp_socket.hpp"

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

  void Handshake(const srv::HandshakeResponse& response) {
    auto order = net::Order("HandshakeResponse", nullptr, false);
    order.type = net::OrderType::Handshake;
    order.b_is_immediate = true;
    order.str_target_string = response.Serialize();
    SendOrderPacket(0, order);
  }
};

srv::HandshakeResponse MakeResponse(std::string_view str_name) {
  auto response = srv::HandshakeResponse{};
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
      auto& multi1 = refs["Multi1"];
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

  const auto str_fluent = srv::FluentMessage::Serialize(
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

  auto request = srv::HandshakeRequest{};
  request.Mod = "ra";
  request.Version = "1.0";
  request.AuthToken = "tok";
  const auto str_request = request.Serialize();
  const auto back_request =
      srv::HandshakeRequest::Deserialize(str_request, "h");
  Check(back_request && back_request->Mod == "ra" &&
            back_request->Version == "1.0" && back_request->AuthToken == "tok",
        "HandshakeRequest roundtrip");

  auto response = MakeResponse("Zed");
  response.Password = "pw";
  response.Fingerprint = "fp";
  const auto back_response =
      srv::HandshakeResponse::Deserialize(response.Serialize(), "h");
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

  if (g_failures != 0) {
    std::println("server_test: {} FAILURES", g_failures);
    return 1;
  }
  std::println("server_test: all checks passed");
  return 0;
}
