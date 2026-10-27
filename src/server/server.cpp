import std;

#include "server/server.hpp"

#include "core/color.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::server {

namespace {

constexpr std::string_view kCustomRules = "notification-custom-rules";
constexpr std::string_view kTwoHumansRequired = "notification-two-humans-required";
constexpr std::string_view kErrorGameStarted = "notification-error-game-started";
constexpr std::string_view kRequiresPassword = "notification-requires-password";
constexpr std::string_view kIncorrectPassword = "notification-incorrect-password";
constexpr std::string_view kIncompatibleMod = "notification-incompatible-mod";
constexpr std::string_view kIncompatibleVersion = "notification-incompatible-version";
constexpr std::string_view kIncompatibleProtocol = "notification-incompatible-protocol";
constexpr std::string_view kBanned = "notification-you-were-banned";
constexpr std::string_view kTempBanned = "notification-you-were-temp-banned";
constexpr std::string_view kFull = "notification-game-full";
constexpr std::string_view kJoined = "notification-joined";
constexpr std::string_view kRequiresAuthentication = "notification-requires-authentication";
constexpr std::string_view kNoPermission = "notification-no-permission-to-join";
constexpr std::string_view kUnknownServerCommand = "notification-unknown-server-command";
constexpr std::string_view kLobbyDisconnected = "notification-lobby-disconnected";
constexpr std::string_view kPlayerDisconnected = "notification-player-disconnected";
constexpr std::string_view kPlayerTeamDisconnected = "notification-team-player-disconnected";
constexpr std::string_view kObserverDisconnected = "notification-observer-disconnected";
constexpr std::string_view kNewAdmin = "notification-new-admin";
constexpr std::string_view kYouWereKicked = "notification-you-were-kicked";
constexpr std::string_view kGameStarted = "notification-game-started";
constexpr std::string_view kChatTemporaryDisabled = "notification-chat-temp-disabled";

std::string Base64Encode(std::span<const std::uint8_t> span_bytes) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  auto str_out = std::string{};
  str_out.reserve((span_bytes.size() + 2) / 3 * 4);

  std::size_t i = 0;
  while (i + 3 <= span_bytes.size()) {
    const auto v = (static_cast<std::uint32_t>(span_bytes[i]) << 16) |
                   (static_cast<std::uint32_t>(span_bytes[i + 1]) << 8) |
                   static_cast<std::uint32_t>(span_bytes[i + 2]);
    str_out += kAlphabet[(v >> 18) & 0x3F];
    str_out += kAlphabet[(v >> 12) & 0x3F];
    str_out += kAlphabet[(v >> 6) & 0x3F];
    str_out += kAlphabet[v & 0x3F];
    i += 3;
  }

  const auto remain = span_bytes.size() - i;
  if (remain == 1) {
    const auto v = static_cast<std::uint32_t>(span_bytes[i]) << 16;
    str_out += kAlphabet[(v >> 18) & 0x3F];
    str_out += kAlphabet[(v >> 12) & 0x3F];
    str_out += "==";
  } else if (remain == 2) {
    const auto v = (static_cast<std::uint32_t>(span_bytes[i]) << 16) |
                   (static_cast<std::uint32_t>(span_bytes[i + 1]) << 8);
    str_out += kAlphabet[(v >> 18) & 0x3F];
    str_out += kAlphabet[(v >> 12) & 0x3F];
    str_out += kAlphabet[(v >> 6) & 0x3F];
    str_out += '=';
  }
  return str_out;
}

std::string AnonymizeIP(std::string_view str_ip) {
  if (str_ip.find(':') != std::string_view::npos)
    return {};
  const auto at = str_ip.rfind('.');
  if (at == std::string_view::npos)
    return {};
  return std::format("{}.*", str_ip.substr(0, at + 1));
}

std::string IpOfEndPoint(std::string_view str_endpoint) {
  if (!str_endpoint.empty() && str_endpoint.front() == '[') {
    const auto close = str_endpoint.find(']');
    if (close != std::string_view::npos)
      return std::string{str_endpoint.substr(1, close - 1)};
    return std::string{str_endpoint};
  }
  const auto colon = str_endpoint.rfind(':');
  if (colon != std::string_view::npos)
    return std::string{str_endpoint.substr(0, colon)};
  return std::string{str_endpoint};
}

std::string FormatQuality(net::ConnectionQuality q) {
  switch (q) {
    case net::ConnectionQuality::Good: return "Good";
    case net::ConnectionQuality::Moderate: return "Moderate";
    case net::ConnectionQuality::Poor: return "Poor";
  }
  return "Good";
}

std::int64_t SteadyMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string FormatLocalTimestamp(std::string_view str_format) {
  const auto now = std::chrono::system_clock::now();
  const auto days = std::chrono::floor<std::chrono::days>(now);
  const auto ymd = std::chrono::year_month_day{days};
  const auto hms = std::chrono::hh_mm_ss{
      std::chrono::floor<std::chrono::seconds>(now - days)};
  static_cast<void>(str_format);
  return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}",
                     static_cast<int>(ymd.year()),
                     static_cast<unsigned>(ymd.month()),
                     static_cast<unsigned>(ymd.day()), hms.hours().count(),
                     hms.minutes().count(), hms.seconds().count());
}

std::vector<std::uint8_t> SerializeImmediateTargetString(
    std::string_view str_order, std::string_view str_target) {
  return net::Order::FromTargetString(str_order, str_target, true).Serialize();
}

std::vector<std::uint8_t> SerializeHandshakeOrder(std::string_view str_name,
                                                  std::string_view str_payload) {
  auto order = net::Order(std::string{str_name}, nullptr, false);
  order.type = net::OrderType::Handshake;
  order.b_is_immediate = true;
  order.str_target_string = std::string{str_payload};
  return order.Serialize();
}

std::optional<int> ParseProfileId(std::string_view str_text) {
  auto value = 0;
  const auto parsed =
      std::from_chars(str_text.data(), str_text.data() + str_text.size(), value);
  if (parsed.ec != std::errc{})
    return std::nullopt;
  return value;
}

}

void Server::EventQueue::Add(std::function<void()> fn) {
  {
    std::lock_guard lock{mutex};
    if (b_completed)
      return;
    deque_events.push_back(std::move(fn));
  }
  cv.notify_all();
}

bool Server::EventQueue::TryTake(std::function<void()>& fn_out,
                                 int int4_timeout_ms) {
  std::unique_lock lock{mutex};
  if (!cv.wait_for(lock, std::chrono::milliseconds(int4_timeout_ms), [this] {
        return !deque_events.empty();
      }))
    return false;

  fn_out = std::move(deque_events.front());
  deque_events.pop_front();
  return true;
}

void Server::SyncClientToPlayerReference(net::SessionClient& c,
                                         const map::PlayerReference* p_pr) {
  if (p_pr == nullptr)
    return;

  if (p_pr->LockFaction)
    c.Faction = p_pr->Faction;
  if (p_pr->LockSpawn)
    c.SpawnPoint = p_pr->Spawn;
  if (p_pr->LockTeam)
    c.Team = p_pr->Team;
  if (p_pr->LockHandicap)
    c.Handicap = p_pr->Handicap;

  c.Color = p_pr->LockColor ? p_pr->Color : c.PreferredColor;
}

void Server::Shutdown() {
  SetState(ServerState::ShuttingDown);
}

void Server::EndGame() {
  for (auto* t : vec_iface_end_game_)
    t->GameEnded(*this);

  if (up_recorder_) {
    up_recorder_->Dispose();
    up_recorder_.reset();
  }
}

void Server::RecordFakeHandshake() {
  if (!up_recorder_)
    return;

  auto request = HandshakeRequest{};
  request.Mod = deps_.str_mod_id;
  request.Version = deps_.str_mod_version;

  up_recorder_->ReceiveFrame(
      0, 0, SerializeHandshakeOrder("HandshakeRequest", request.Serialize()));

  auto response = HandshakeResponse{};
  response.Mod = deps_.str_mod_id;
  response.Version = deps_.str_mod_version;
  response.OrdersProtocol = kOrdersProtocol;

  up_recorder_->ReceiveFrame(
      0, 0,
      SerializeHandshakeOrder("HandshakeResponse", response.Serialize()));
}

Server::Server(std::vector<std::pair<std::string, int>> vec_endpoints,
               ServerSettings settings, ServerDeps deps, ServerType type)
    : Random(static_cast<std::int32_t>(SteadyMs() & 0x7FFFFFFF)),
      Type(type),
      deps_(std::move(deps)) {
  if (!deps_.fn_clock)
    deps_.fn_clock = [] {
      return SteadyMs();
    };
  if (!deps_.fn_new_game_uid) {
    deps_.fn_new_game_uid = [] {
      auto rnd = std::random_device{};
      auto str = std::string{};
      for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20)
          str += '-';
        if (i == 12)
          str += '4';
        else
          str += "0123456789abcdef"[rnd() & 0xF];
      }
      return str;
    };
  }
  if (!deps_.fn_fluent_get)
    deps_.fn_fluent_get = [](std::string_view str_key) {
      return std::string{str_key};
    };

  vec_listeners_.reserve(vec_endpoints.size());
  for (auto& [str_host, int4_port] : vec_endpoints) {
    auto tcp = platform::TcpSocket::ListenEndpoint(str_host, int4_port);
    if (!tcp.IsValid()) {
      ServerLog(std::format("Failed to listen on {}:{}: socket error", str_host,
                            int4_port));
      continue;
    }

    const int int4_bound = tcp.BoundPort();
    vec_local_endpoints_.emplace_back(str_host, int4_bound);
    vec_listeners_.push_back(std::move(tcp));

    vec_listener_threads_.emplace_back([this, int4_bound, idx = vec_listeners_.size() - 1] {
      while (true) {
        if (State() != ServerState::WaitingPlayers)
          return;

        if (!vec_listeners_[idx].PollReadable(1000000))
          continue;

        auto accepted = vec_listeners_[idx].Accept();
        if (accepted.IsValid()) {
          auto* p_accepted = new platform::TcpSocket{std::move(accepted)};
          queue_events_.Add([this, p_accepted] {
            auto tcp_owned = std::unique_ptr<platform::TcpSocket>{p_accepted};
            AcceptConnection(std::move(*tcp_owned));
          });
        }
      }
    });
  }

  b_listening_ = !vec_listeners_.empty();

  Settings = std::move(settings);
  Settings.Name = Settings.SanitizedServerName(Settings.Name,
                                               deps_.str_player_name);

  int4_random_seed_ = static_cast<int>(SteadyMs() ^ (SteadyMs() << 17));

  up_player_message_tracker_ = std::make_unique<PlayerMessageTracker>(
      *this,
      [this](ServerConnection& c, int client, int frame,
             const std::vector<std::uint8_t>& vec_data) {
        DispatchOrdersToClient(c, client, frame, vec_data);
      },
      [this](ServerConnection& c, std::string_view str_key,
             std::span<const FluentArgument> vec_args) {
        SendFluentMessageTo(c, str_key, vec_args);
      });

  {
    auto deps_cache = MapStatusCacheDeps{};
    deps_cache.vec_lint_passes = deps_.vec_lint_passes;
    deps_cache.fn_server_log = [this](std::string_view str_line) {
      ServerLog(str_line);
    };
    up_map_status_cache_ = std::make_unique<MapStatusCache>(
        std::move(deps_cache),
        [this](std::string_view str_uid, int int4_status) {
          std::lock_guard lock{mutex_lobby};
          if (LobbyInfo.global_settings.Map == str_uid)
            LobbyInfo.global_settings.MapStatusFlags = int4_status;
          SyncLobbyInfo();
        },
        type == ServerType::Dedicated && Settings.EnableLintChecks);
  }

  up_vote_kick_tracker_ = std::make_unique<VoteKickTracker>(*this);

  LobbyInfo.global_settings.RandomSeed = int4_random_seed_;
  LobbyInfo.global_settings.ServerName = Settings.Name;
  LobbyInfo.global_settings.EnableSingleplayer =
      Settings.EnableSingleplayer || Type != ServerType::Dedicated;
  LobbyInfo.global_settings.EnableMapGeneration = Settings.EnableMapGeneration;
  LobbyInfo.global_settings.EnableSyncReports = Settings.EnableSyncReports;
  LobbyInfo.global_settings.GameUid = deps_.fn_new_game_uid();
  LobbyInfo.global_settings.Dedicated = Type == ServerType::Dedicated;

  if (Settings.RecordReplays && Type == ServerType::Dedicated) {
    up_recorder_ = std::make_unique<net::ReplayRecorder>(
        deps_.fn_timestamped_filename
            ? deps_.fn_timestamped_filename
            : net::ReplayRecorder::ChooseFilename{[] { return std::string{"server.orarep"}; }});
    RecordFakeHandshake();
  }

  thread_server_ = std::thread([this] { ServerThreadBody(); });
}

Server::~Server() {
  Shutdown();
  if (thread_server_.joinable())
    thread_server_.join();

  for (auto& listener : vec_listeners_)
    listener.Close();
  for (auto& thread : vec_listener_threads_)
    if (thread.joinable())
      thread.join();

  {
    std::unique_lock lock{mutex_connections_gone_};
    cv_connections_gone_.wait(lock,
                              [this] { return int4_live_connections_.load() == 0; });
  }

  for (auto& conn : vec_conns)
    conn->Dispose();
  vec_conns.clear();
}

void Server::NotifyConnectionAlive() {
  int4_live_connections_.fetch_add(1);
}

void Server::NotifyConnectionGone() {
  {
    std::lock_guard lock{mutex_connections_gone_};
    int4_live_connections_.fetch_sub(1);
  }
  cv_connections_gone_.notify_all();
}

void Server::PostEvent(std::function<void()> fn) {
  queue_events_.Add(std::move(fn));
}

void Server::AddServerTrait(std::unique_ptr<ServerTrait> up_trait) {
  if (auto* p = dynamic_cast<IInterpretCommand*>(up_trait.get()))
    vec_iface_interpret_command_.push_back(p);
  if (auto* p = dynamic_cast<INotifySyncLobbyInfo*>(up_trait.get()))
    vec_iface_sync_lobby_info_.push_back(p);
  if (auto* p = dynamic_cast<INotifyServerStart*>(up_trait.get()))
    vec_iface_server_start_.push_back(p);
  if (auto* p = dynamic_cast<INotifyServerEmpty*>(up_trait.get()))
    vec_iface_server_empty_.push_back(p);
  if (auto* p = dynamic_cast<INotifyServerShutdown*>(up_trait.get()))
    vec_iface_server_shutdown_.push_back(p);
  if (auto* p = dynamic_cast<IStartGame*>(up_trait.get()))
    vec_iface_start_game_.push_back(p);
  if (auto* p = dynamic_cast<IClientJoined*>(up_trait.get()))
    vec_iface_client_joined_.push_back(p);
  if (auto* p = dynamic_cast<IEndGame*>(up_trait.get()))
    vec_iface_end_game_.push_back(p);
  if (auto* p = dynamic_cast<ITick*>(up_trait.get()))
    vec_iface_tick_.push_back(p);
  vec_traits_.push_back(std::move(up_trait));
}

void Server::ServerThreadBody() {
  for (auto* t : vec_iface_server_start_)
    t->ServerStarted(*this);

  ServerLog(std::format("Initial mod: {}", deps_.str_mod_id));
  ServerLog(std::format("Initial map: {}", LobbyInfo.global_settings.Map));

  while (true) {
    if (State() != ServerState::ShuttingDown) {
      auto fn = std::function<void()>{};
      if (queue_events_.TryTake(fn, 1000))
        fn();

      for (auto* t : vec_iface_tick_)
        t->Tick(*this);

      if (State() == ServerState::GameStarted) {
        for (const auto& [int4_player_index, fp_scale] :
             order_buffer_.GetTickScales()) {
          const auto vec_frame = CreateTickScaleFrame(fp_scale);

          ServerConnection* con = nullptr;
          for (auto& c : vec_conns)
            if (c->int4_player_index == int4_player_index) {
              con = c.get();
              break;
            }

          if (con != nullptr && con->b_validated.load())
            DispatchFrameToClient(*con, int4_player_index, vec_frame);
        }
      }
    }

    if (State() == ServerState::ShuttingDown) {
      EndGame();
      break;
    }
  }

  for (auto* t : vec_iface_server_shutdown_)
    t->ServerShutdown(*this);

  for (auto& c : vec_conns)
    c->Dispose();
  vec_conns.clear();
}

void Server::OnConnectionPacket(std::shared_ptr<ServerConnection> conn,
                                int int4_frame,
                                std::vector<std::uint8_t> vec_data) {
  queue_events_.Add([this, conn = std::move(conn), int4_frame,
                     vec_data = std::move(vec_data)]() mutable {
    ReceiveOrders(*conn, int4_frame, vec_data);
  });
}

void Server::OnConnectionPing(std::shared_ptr<ServerConnection> conn,
                              std::vector<int> vec_ping_history,
                              std::uint8_t u1_queue_length) {
  static_cast<void>(u1_queue_length);
  queue_events_.Add([this, conn = std::move(conn),
                     vec_ping_history = std::move(vec_ping_history)]() mutable {
    ReceivePing(*conn, vec_ping_history);
  });
}

void Server::OnConnectionDisconnect(std::shared_ptr<ServerConnection> conn) {
  queue_events_.Add([this, conn = std::move(conn)] { DropClient(*conn); });
}

void Server::AcceptConnection(platform::TcpSocket socket) {
  if (State() != ServerState::WaitingPlayers)
    return;

  auto vec_token_bytes = std::vector<std::uint8_t>{};
  vec_token_bytes.resize(256);
  for (auto& byte : vec_token_bytes)
    byte = static_cast<std::uint8_t>(Random.Next());
  const auto str_token = Base64Encode(vec_token_bytes);

  auto conn = std::make_shared<ServerConnection>(
      ChooseFreePlayerIndex(), str_token, socket.RemoteDisplay(),
      std::move(socket), deps_.fn_clock);

  {
    auto w = net::ByteWriter{};
    w.Write(kHandshakeProtocol);
    w.Write(conn->int4_player_index);
    if (!conn->TrySendData(w.TakeBytes())) {
      ServerLog(std::format("Handshake for client {} failed: send failed",
                            conn->str_end_point));
    }
  }

  auto request = HandshakeRequest{};
  request.Mod = deps_.str_mod_id;
  request.Version = deps_.str_mod_version;
  request.AuthToken = str_token;

  DispatchOrdersToClient(
      *conn, 0, 0,
      SerializeHandshakeOrder("HandshakeRequest", request.Serialize()));

  vec_conns.push_back(std::move(conn));
  vec_conns.back()->StartReader(*this, IsMultiplayer());
}

void Server::ValidateClient(std::shared_ptr<ServerConnection> up_new_conn,
                            std::string_view str_data) {
  auto& new_conn = *up_new_conn;
  auto handshake = HandshakeResponse::Deserialize(str_data, "HandshakeResponse");
  if (!handshake) {
    ServerLog(std::format(
        "Dropping connection {} because an error occurred:",
        new_conn.str_end_point));
    DropClient(new_conn);
    return;
  }

  if (State() == ServerState::GameStarted) {
    ServerLog(std::format(
        "Rejected connection from {}; game is already started.",
        new_conn.str_end_point));
    SendOrderTo(new_conn, "ServerError", kErrorGameStarted);
    DropClient(new_conn);
    return;
  }

  if (!Settings.Password.empty() && handshake->Password != Settings.Password) {
    const auto* message =
        handshake->Password.empty() ? kRequiresPassword.data()
                                    : kIncorrectPassword.data();
    SendOrderTo(new_conn, "AuthenticationError", message);
    DropClient(new_conn);
    return;
  }

  const auto str_ip = IpOfEndPoint(new_conn.str_end_point);
  auto client = net::SessionClient{};
  client.Name = SanitizedPlayerName(handshake->Client.Name, "Commander");
  client.AnonymizedIPAddress =
      IsMultiplayer() && Settings.ShareAnonymizedIPs ? AnonymizeIP(str_ip)
                                                     : std::string{};
  client.IPAddress = str_ip;
  client.Index = new_conn.int4_player_index;
  client.PreferredColor = handshake->Client.PreferredColor;
  client.Color = handshake->Client.Color;
  client.Faction = "Random";
  client.SpawnPoint = 0;
  client.Team = 0;
  client.Handicap = 0;
  client.State = net::ClientState::Invalid;

  if (deps_.str_mod_id != handshake->Mod) {
    ServerLog(std::format(
        "Rejected connection from {}; mods do not match.",
        new_conn.str_end_point));
    SendOrderTo(new_conn, "ServerError", kIncompatibleMod);
    DropClient(new_conn);
    return;
  }

  if (deps_.str_mod_version != handshake->Version) {
    ServerLog(std::format(
        "Rejected connection from {}; Not running the same version.",
        new_conn.str_end_point));
    SendOrderTo(new_conn, "ServerError", kIncompatibleVersion);
    DropClient(new_conn);
    return;
  }

  if (handshake->OrdersProtocol != kOrdersProtocol) {
    ServerLog(std::format(
        "Rejected connection from {}; incompatible Orders protocol version {}.",
        new_conn.str_end_point, handshake->OrdersProtocol));
    SendOrderTo(new_conn, "ServerError", kIncompatibleProtocol);
    DropClient(new_conn);
    return;
  }

  auto vec_bans = Settings.vec_ban;
  for (const auto& ban : vec_temp_bans)
    if (std::find(vec_bans.begin(), vec_bans.end(), ban) == vec_bans.end())
      vec_bans.push_back(ban);

  const bool b_banned =
      std::find(vec_bans.begin(), vec_bans.end(), str_ip) != vec_bans.end();
  if (b_banned) {
    ServerLog(std::format("Rejected connection from {}; Banned.",
                          new_conn.str_end_point));
    const bool b_perm =
        std::find(Settings.vec_ban.begin(), Settings.vec_ban.end(), str_ip) !=
        Settings.vec_ban.end();
    SendOrderTo(new_conn, "ServerError",
                b_perm ? kBanned.data() : kTempBanned.data());
    DropClient(new_conn);
    return;
  }

  if (!IsMultiplayer()) {
    client.Fingerprint = handshake->Fingerprint;
    CompleteConnection(std::move(up_new_conn), std::move(client));
    return;
  }

  if (!handshake->Fingerprint.empty() && !handshake->AuthSignature.empty() &&
      deps_.fn_verify_auth) {
    auto str_profile_id = std::string{};
    const bool b_ok = deps_.fn_verify_auth(
        handshake->Fingerprint, handshake->AuthSignature, new_conn.str_auth_token,
        str_profile_id);
    ServerLog(std::format(
        "{} {} as {}", new_conn.str_end_point,
        b_ok ? "authenticated" : "failed to authenticate", handshake->Fingerprint));

    if (b_ok)
      client.Fingerprint = handshake->Fingerprint;

    queue_events_.Add([this, up_new_conn, client, b_ok, str_profile_id]() mutable {
      const bool not_authenticated =
          Type == ServerType::Dedicated && !b_ok &&
          (Settings.RequireAuthentication ||
           !Settings.vec_profile_id_whitelist.empty());
      auto blacklisted = false;
      auto not_whitelisted = false;
      if (b_ok) {
        const auto id = ParseProfileId(str_profile_id);
        if (id &&
            std::find(Settings.vec_profile_id_blacklist.begin(),
                      Settings.vec_profile_id_blacklist.end(),
                      *id) != Settings.vec_profile_id_blacklist.end())
          blacklisted = true;
        if (!Settings.vec_profile_id_whitelist.empty()) {
          if (!id ||
              std::find(Settings.vec_profile_id_whitelist.begin(),
                        Settings.vec_profile_id_whitelist.end(),
                        *id) == Settings.vec_profile_id_whitelist.end())
            not_whitelisted = true;
        }
      }

      if (not_authenticated) {
        ServerLog(std::format(
            "Rejected connection from {}; Not authenticated.",
            up_new_conn->str_end_point));
        SendOrderTo(*up_new_conn, "ServerError", kRequiresAuthentication);
        DropClient(*up_new_conn);
      } else if (blacklisted || not_whitelisted) {
        if (blacklisted)
          ServerLog(std::format(
              "Rejected connection from {}; In server blacklist.",
              up_new_conn->str_end_point));
        else
          ServerLog(std::format(
              "Rejected connection from {}; Not in server whitelist.",
              up_new_conn->str_end_point));
        SendOrderTo(*up_new_conn, "ServerError", kNoPermission);
        DropClient(*up_new_conn);
      } else {
        CompleteConnection(std::move(up_new_conn), std::move(client));
      }
    });
    return;
  }

  if (Type == ServerType::Dedicated &&
      (Settings.RequireAuthentication ||
       !Settings.vec_profile_id_whitelist.empty())) {
    ServerLog(std::format(
        "Rejected connection from {}; Not authenticated.",
        new_conn.str_end_point));
    SendOrderTo(new_conn, "ServerError", kRequiresAuthentication);
    DropClient(new_conn);
    return;
  }

  CompleteConnection(std::move(up_new_conn), std::move(client));
}

void Server::CompleteConnection(
    std::shared_ptr<ServerConnection> up_new_conn, net::SessionClient client) {
  auto& new_conn = *up_new_conn;
  std::lock_guard lock{mutex_lobby};

  client.Slot = LobbyInfo.FirstEmptySlot();
  client.IsAdmin = true;
  for (const auto& c : LobbyInfo.vec_clients)
    if (c.IsAdmin) {
      client.IsAdmin = false;
      break;
    }

  if (client.IsObserver() && !LobbyInfo.global_settings.AllowSpectators) {
    SendOrderTo(new_conn, "ServerError", kFull);
    DropClient(new_conn);
    return;
  }

  if (!client.Slot.empty()) {
    if (deps_.fn_player_reference)
      SyncClientToPlayerReference(
          client, deps_.fn_player_reference(client.Slot));
    else if (Map != nullptr)
      SyncClientToPlayerReference(client, Map->Players().Find(client.Slot));
  } else {
    client.Color = core::Color::FromArgb(255, 255, 255, 255);
  }

  LobbyInfo.vec_clients.push_back(client);
  new_conn.b_validated.store(true);

  if (!client.IsAdmin && Settings.FloodLimitJoinCooldown > 0)
    up_player_message_tracker_->DisableChatUI(
        new_conn, Settings.FloodLimitJoinCooldown);

  ServerLog(std::format("Client {}: Accepted connection from {}.",
                        new_conn.int4_player_index, new_conn.str_end_point));

  if (!client.Fingerprint.empty())
    ServerLog(std::format("Client {}: Player fingerprint is {}.",
                          new_conn.int4_player_index, client.Fingerprint));

  for (auto* t : vec_iface_client_joined_)
    t->ClientJoined(*this, new_conn);

  SyncLobbyInfo();

  ServerLog(std::format("{} ({}) has joined the game.", client.Name,
                        new_conn.str_end_point));

  auto vec_other_conns = std::vector<std::shared_ptr<ServerConnection>>{};
  for (auto& c : vec_conns)
    if (c.get() != &new_conn)
      vec_other_conns.push_back(c);

  SendFluentMessage(vec_other_conns, kJoined,
                    std::vector<FluentArgument>{{"player", client.Name}});

  if (Type != ServerType::Local &&
      (LobbyInfo.global_settings.MapStatusFlags & 8) != 0)
    SendFluentMessageTo(new_conn, kCustomRules);
}

std::vector<std::uint8_t> Server::CreateFrame(
    int int4_client, int int4_frame,
    const std::vector<std::uint8_t>& vec_data) {
  auto w = net::ByteWriter{};
  w.Write(static_cast<std::int32_t>(vec_data.size() + 4));
  w.Write(int4_client);
  w.Write(int4_frame);
  w.WriteBytes(vec_data);
  return w.TakeBytes();
}

std::vector<std::uint8_t> Server::CreateAckFrame(int int4_frame,
                                                 std::uint8_t u1_count) {
  auto w = net::ByteWriter{};
  w.Write(std::int32_t{6});
  w.Write(std::int32_t{0});
  w.Write(int4_frame);
  w.WriteByte(static_cast<std::uint8_t>(net::OrderType::Ack));
  w.WriteByte(u1_count);
  return w.TakeBytes();
}

std::vector<std::uint8_t> Server::CreateTickScaleFrame(float fp_scale) {
  auto w = net::ByteWriter{};
  w.Write(std::int32_t{9});
  w.Write(std::int32_t{0});
  w.Write(std::int32_t{0});
  w.WriteByte(static_cast<std::uint8_t>(net::OrderType::TickScale));
  w.Write(fp_scale);
  return w.TakeBytes();
}

void Server::DispatchOrdersToClient(ServerConnection& c, int int4_client,
                                    int int4_frame,
                                    const std::vector<std::uint8_t>& vec_data) {
  DispatchFrameToClient(c, int4_client, CreateFrame(int4_client, int4_frame, vec_data));
}

void Server::DispatchFrameToClient(ServerConnection& c, int int4_client,
                                   std::vector<std::uint8_t> vec_frame_data) {
  if (!c.TrySendData(std::move(vec_frame_data))) {
    DropClient(c);
    ServerLog(std::format(
        "Dropping client {} because dispatching orders failed!", int4_client));
  }
}

bool Server::AnyUndefinedWinStates() {
  auto int4_last_team = -1;
  for (const auto& player : up_game_info_->vec_players) {
    if (player.Outcome != WinState::Undefined)
      continue;

    if (int4_last_team >= 0 &&
        (player.Team != int4_last_team || player.Team == 0))
      return true;

    int4_last_team = player.Team;
  }
  return false;
}

void Server::SetPlayerDefeat(int int4_player_index) {
  auto& defeated = vec_world_players_[static_cast<std::size_t>(int4_player_index)];
  if (!defeated || defeated->Outcome != WinState::Undefined)
    return;

  defeated->Outcome = WinState::Lost;
  defeated->OutcomeTimestampUtc = GameDateTime::UtcNow();

  if (!AnyUndefinedWinStates()) {
    const auto now = GameDateTime::UtcNow();
    for (auto& player : up_game_info_->vec_players) {
      if (player.Outcome != WinState::Undefined)
        continue;
      player.Outcome = WinState::Won;
      player.OutcomeTimestampUtc = now;
    }
  }
}

void Server::OutOfSync(int int4_frame) {
  ServerLog(std::format(
      "Out of sync detected at frame {}, cancel replay recording",
      int4_frame));

  if (up_recorder_) {
    up_replay_metadata_.reset();
    up_recorder_->Metadata = nullptr;
    up_recorder_->Dispose();
  }
  up_recorder_.reset();
}

void Server::HandleSyncOrder(int int4_frame,
                             const std::vector<std::uint8_t>& vec_packet) {
  const auto it = map_sync_for_frame_.find(int4_frame);
  if (it != map_sync_for_frame_.end()) {
    const auto& existing = it->second;
    if (vec_packet.size() != existing.size()) {
      OutOfSync(int4_frame);
      return;
    }

    for (std::size_t i = 0; i < vec_packet.size(); ++i)
      if (vec_packet[i] != existing[i]) {
        OutOfSync(int4_frame);
        return;
      }
  } else {
    auto uint8_defeat = std::uint64_t{0};
    for (auto i = 0; i < 8; ++i)
      uint8_defeat |= static_cast<std::uint64_t>(
                          vec_packet[1 + 4 + static_cast<std::size_t>(i)])
                      << (i * 8);

    if (int4_frame > int4_last_defeat_state_frame_ &&
        uint8_last_defeat_state_ != uint8_defeat) {
      const auto new_defeats = uint8_defeat & ~uint8_last_defeat_state_;
      for (std::size_t i = 0; i < vec_world_players_.size(); ++i)
        if ((new_defeats & (std::uint64_t{1} << i)) != 0)
          SetPlayerDefeat(static_cast<int>(i));

      uint8_last_defeat_state_ = uint8_defeat;
      int4_last_defeat_state_frame_ = int4_frame;
    }

    map_sync_for_frame_.emplace(int4_frame, vec_packet);
  }
}

void Server::DispatchOrdersToClients(ServerConnection& conn, int int4_frame,
                                     const std::vector<std::uint8_t>& vec_data) {
  const auto from = conn.int4_player_index;
  const auto vec_frame_data = CreateFrame(from, int4_frame, vec_data);

  auto snapshot = vec_conns;
  for (auto& c : snapshot)
    if (c.get() != &conn && c->b_validated.load())
      DispatchFrameToClient(*c, from, vec_frame_data);

  RecordOrder(int4_frame, vec_data, from);
}

void Server::RecordOrder(int int4_frame,
                         const std::vector<std::uint8_t>& vec_data, int from) {
  if (up_recorder_)
    up_recorder_->ReceiveFrame(from, int4_frame, vec_data);

  if (!vec_data.empty() &&
      vec_data[0] == static_cast<std::uint8_t>(net::OrderType::SyncHash)) {
    if (vec_data.size() == static_cast<std::size_t>(net::Order::SyncHashOrderLength))
      HandleSyncOrder(int4_frame, vec_data);
    else
      ServerLog(std::format(
          "Dropped sync order with length {} from client {}. Expected length {}.",
          vec_data.size(), from, net::Order::SyncHashOrderLength));
  }
}

void Server::DispatchServerOrdersToClients(
    const std::vector<std::uint8_t>& vec_data, int int4_frame) {
  constexpr int kFrom = 0;
  const auto vec_frame_data = CreateFrame(kFrom, int4_frame, vec_data);

  auto snapshot = vec_conns;
  for (auto& c : snapshot)
    if (c->b_validated.load())
      DispatchFrameToClient(*c, kFrom, vec_frame_data);

  RecordOrder(int4_frame, vec_data, kFrom);
}

void Server::DispatchServerOrdersToClients(
    std::span<const std::shared_ptr<ServerConnection>> vec_span_conns,
    const std::vector<std::uint8_t>& vec_data, int int4_frame) {
  constexpr int kFrom = 0;
  const auto vec_frame_data = CreateFrame(kFrom, int4_frame, vec_data);

  for (auto& c : vec_span_conns)
    if (c->b_validated.load())
      DispatchFrameToClient(*c, kFrom, vec_frame_data);

  RecordOrder(int4_frame, vec_data, kFrom);
}

void Server::ReceiveOrders(ServerConnection& conn, int int4_frame,
                           const std::vector<std::uint8_t>& vec_data) {
  bool present = false;
  for (auto& c : vec_conns)
    if (c.get() == &conn) {
      present = true;
      break;
    }
  if (!present)
    return;

  if (int4_frame != 0 && State() != ServerState::GameStarted) {
    ServerLog(std::format(
        "Dropping client {} for sending order frame {} before the game started.",
        conn.int4_player_index, int4_frame));
    DropClient(conn);
    return;
  }

  if (int4_frame == 0) {
    InterpretServerOrders(conn, vec_data);
  } else {
    if (vec_data.empty() ||
        vec_data[0] != static_cast<std::uint8_t>(net::OrderType::SyncHash)) {
      int4_frame += OrderLatency;
      DispatchFrameToClient(
          conn, conn.int4_player_index, CreateAckFrame(int4_frame, 1));

      order_buffer_.AddOrderTimestamp(conn.int4_player_index);

      conn.int4_last_orders_frame.store(int4_frame);
    }

    DispatchOrdersToClients(conn, int4_frame, vec_data);
  }
}

void Server::InterpretServerOrders(ServerConnection& conn,
                                   const std::vector<std::uint8_t>& vec_data) {
  auto reader = net::ByteReader{vec_data};
  while (reader.Position() < reader.Length()) {
    auto o = net::Order::Deserialize(nullptr, reader);
    if (!o)
      break;
    InterpretServerOrder(conn, *o);
  }
}

void Server::InterpretServerOrder(ServerConnection& conn, const net::Order& o) {
  std::lock_guard lock{mutex_lobby};

  if (!conn.b_validated.load()) {
    if (o.str_order_string == "HandshakeResponse") {
      ValidateClient(
          conn.shared_from_this(),
          o.str_target_string ? *o.str_target_string : std::string{});
    } else {
      ServerLog(std::format(
          "Rejected connection from {}; Order `{}` is not a `HandshakeResponse`.",
          conn.str_end_point, o.str_order_string));
      DropClient(conn);
    }
    return;
  }

  if (o.str_order_string == "Command") {
    const auto str_target =
        o.str_target_string ? *o.str_target_string : std::string{};
    if (!InterpretCommand(str_target, conn)) {
      ServerLog(std::format("Unknown server command: {}", str_target));
      SendFluentMessageTo(conn, kUnknownServerCommand,
                          std::vector<FluentArgument>{{"command", str_target}});
    }
  } else if (o.str_order_string == "Chat") {
    if (!IsMultiplayer() ||
        !up_player_message_tracker_->IsPlayerAtFloodLimit(conn))
      DispatchOrdersToClients(conn, 0, o.Serialize());
  }
}

void Server::SendOrderTo(ServerConnection& conn, std::string_view str_order,
                         std::string_view str_data) {
  DispatchOrdersToClient(conn, 0, 0,
                         SerializeImmediateTargetString(str_order, str_data));
}

void Server::SendFluentMessage(std::string_view str_key,
                               std::span<const FluentArgument> vec_args) {
  auto snapshot = vec_conns;
  SendFluentMessage(snapshot, str_key, vec_args);
}

void Server::SendFluentMessage(
    std::span<const std::shared_ptr<ServerConnection>> vec_span_conns,
    std::string_view str_key, std::span<const FluentArgument> vec_args) {
  const auto str_text = FluentMessage::Serialize(str_key, vec_args);
  DispatchServerOrdersToClients(
      vec_span_conns, SerializeImmediateTargetString("FluentMessage", str_text));

  if (Type == ServerType::Dedicated)
    WriteLineWithTimeStamp(deps_.fn_fluent_get(str_key));
}

void Server::SendFluentMessageTo(ServerConnection& conn,
                                 std::string_view str_key,
                                 std::span<const FluentArgument> vec_args) {
  const auto str_text = FluentMessage::Serialize(str_key, vec_args);
  DispatchOrdersToClient(
      conn, 0, 0, SerializeImmediateTargetString("FluentMessage", str_text));
}

void Server::WriteLineWithTimeStamp(std::string_view str_line) {
  std::println("[{}] {}", FormatLocalTimestamp(Settings.TimestampFormat),
               str_line);
}

void Server::ReceivePing(ServerConnection& conn,
                         const std::vector<int>& vec_ping_history) {
  auto i8_sum = std::int64_t{0};
  for (const auto ping : vec_ping_history)
    i8_sum += ping;
  const auto latency = vec_ping_history.empty()
                           ? 0
                           : i8_sum / static_cast<std::int64_t>(vec_ping_history.size());

  const auto quality = latency < 240 ? net::ConnectionQuality::Good
                      : latency < 360 ? net::ConnectionQuality::Moderate
                                      : net::ConnectionQuality::Poor;

  std::lock_guard lock{mutex_lobby};
  for (auto& c : LobbyInfo.vec_clients)
    if (c.Index == conn.int4_player_index ||
        (c.IsBot() && c.BotControllerClientIndex == conn.int4_player_index))
      c.ConnectionQualityKind = quality;

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - time_ping_updated_);
  if (elapsed.count() > 5000) {
    auto pool = yaml::StringPool{};
    auto vec_nodes = std::vector<yaml::MiniYamlNode>{};
    for (const auto& c : LobbyInfo.vec_clients)
      vec_nodes.push_back(yaml::MiniYamlNode{
          pool.Intern(std::format("ConnectionQuality@{}", c.Index)),
          yaml::MiniYaml{pool.Intern(FormatQuality(c.ConnectionQualityKind))}});

    DispatchServerOrdersToClients(SerializeImmediateTargetString(
        "SyncConnectionQuality", yaml::WriteToString(vec_nodes)));
    time_ping_updated_ = std::chrono::steady_clock::now();
  }
}

net::SessionClient* Server::GetClient(ServerConnection& conn) {
  return LobbyInfo.ClientWithIndex(conn.int4_player_index);
}

bool Server::HasClientWonOrLost(const net::SessionClient& client) {
  const GameInformationPlayer* found = nullptr;
  for (const auto& world : vec_world_players_)
    if (world && world->ClientIndex == client.Index) {
      found = &*world;
      break;
    }
  if (found == nullptr)
    return true;
  return found->Outcome != WinState::Undefined;
}

void Server::DropClient(ServerConnection& to_drop) {
  {
    std::lock_guard lock{mutex_lobby};
    order_buffer_.RemovePlayer(to_drop.int4_player_index);

    auto it = std::find_if(vec_conns.begin(), vec_conns.end(),
                           [&to_drop](const auto& c) {
                             return c.get() == &to_drop;
                           });
    if (it != vec_conns.end())
      vec_conns.erase(it);

    net::SessionClient* drop_client = nullptr;
    for (auto& c : LobbyInfo.vec_clients)
      if (c.Index == to_drop.int4_player_index) {
        drop_client = &c;
        break;
      }

    if (drop_client == nullptr) {
      to_drop.Dispose();
      return;
    }

    const bool b_drop_was_admin = drop_client->IsAdmin;

    if (State() == ServerState::GameStarted) {
      if (drop_client->IsObserver())
        SendFluentMessage(kObserverDisconnected,
                          std::vector<FluentArgument>{{"player", drop_client->Name}});
      else if (drop_client->Team > 0)
        SendFluentMessage(
            kPlayerTeamDisconnected,
            std::vector<FluentArgument>{{"player", drop_client->Name},
                                        {"team", std::to_string(drop_client->Team),
                                         FluentArgumentType::Number}});
      else
        SendFluentMessage(kPlayerDisconnected,
                          std::vector<FluentArgument>{{"player", drop_client->Name}});
    } else {
      SendFluentMessage(kLobbyDisconnected,
                        std::vector<FluentArgument>{{"player", drop_client->Name}});
    }

    LobbyInfo.vec_clients.erase(
        std::remove_if(LobbyInfo.vec_clients.begin(), LobbyInfo.vec_clients.end(),
                       [&to_drop](const auto& c) {
                         return c.Index == to_drop.int4_player_index;
                       }),
        LobbyInfo.vec_clients.end());

    if (Type == ServerType::Dedicated && drop_client->IsAdmin &&
        State() == ServerState::WaitingPlayers) {
      LobbyInfo.vec_clients.erase(
          std::remove_if(LobbyInfo.vec_clients.begin(),
                         LobbyInfo.vec_clients.end(),
                         [&to_drop](const auto& c) {
                           return c.IsBot() &&
                                  c.BotControllerClientIndex ==
                                      to_drop.int4_player_index;
                         }),
          LobbyInfo.vec_clients.end());

      net::SessionClient* next_admin = nullptr;
      for (auto& c : LobbyInfo.vec_clients)
        if (!c.IsBot() &&
            (next_admin == nullptr || c.Index < next_admin->Index))
          next_admin = &c;

      if (next_admin != nullptr) {
        next_admin->IsAdmin = true;
        SendFluentMessage(kNewAdmin,
                          std::vector<FluentArgument>{{"player", next_admin->Name}});
      }
    }

    auto disconnect_packet = std::vector<std::uint8_t>{};
    disconnect_packet.push_back(
        static_cast<std::uint8_t>(net::OrderType::Disconnect));
    for (auto i = 0; i < 4; ++i)
      disconnect_packet.push_back(static_cast<std::uint8_t>(
          (static_cast<std::uint32_t>(to_drop.int4_player_index) >> (i * 8)) &
          0xFF));
    DispatchServerOrdersToClients(
        disconnect_packet, to_drop.int4_last_orders_frame.load() + 1);

    if (up_game_info_)
      for (auto& player : up_game_info_->vec_players)
        if (player.ClientIndex == to_drop.int4_player_index)
          player.DisconnectFrame = to_drop.int4_last_orders_frame.load() + 1;

    bool any_validated = false;
    for (auto& c : vec_conns)
      if (c->b_validated.load()) {
        any_validated = true;
        break;
      }
    if (!any_validated)
      for (auto* t : vec_iface_server_empty_)
        t->ServerEmpty(*this);

    if (any_validated || Type == ServerType::Dedicated)
      SyncLobbyClients();

    if (Type != ServerType::Dedicated && b_drop_was_admin)
      Shutdown();
  }

  to_drop.Dispose();
}

void Server::SyncLobbyInfo() {
  std::lock_guard lock{mutex_lobby};
  if (State() == ServerState::WaitingPlayers)
    DispatchServerOrdersToClients(SerializeImmediateTargetString(
        "SyncInfo", LobbyInfo.Serialize()));

  for (auto* t : vec_iface_sync_lobby_info_)
    t->LobbyInfoSynced(*this);
}

void Server::SyncLobbyClients() {
  if (State() != ServerState::WaitingPlayers)
    return;

  std::lock_guard lock{mutex_lobby};

  auto pool = yaml::StringPool{};
  auto vec_nodes = std::vector<yaml::MiniYamlNode>{};
  for (const auto& client : LobbyInfo.vec_clients)
    vec_nodes.push_back(net::SerializeSessionClient(pool, client));

  DispatchServerOrdersToClients(SerializeImmediateTargetString(
      "SyncLobbyClients", yaml::WriteToString(vec_nodes)));

  for (auto* t : vec_iface_sync_lobby_info_)
    t->LobbyInfoSynced(*this);

  time_ping_updated_ = std::chrono::steady_clock::now();
}

void Server::SyncLobbySlots() {
  if (State() != ServerState::WaitingPlayers)
    return;

  std::lock_guard lock{mutex_lobby};

  auto pool = yaml::StringPool{};
  auto vec_nodes = std::vector<yaml::MiniYamlNode>{};
  for (const auto& [name, slot] : LobbyInfo.vec_slots) {
    static_cast<void>(name);
    vec_nodes.push_back(net::SerializeSessionSlot(pool, slot));
  }

  DispatchServerOrdersToClients(SerializeImmediateTargetString(
      "SyncLobbySlots", yaml::WriteToString(vec_nodes)));

  for (auto* t : vec_iface_sync_lobby_info_)
    t->LobbyInfoSynced(*this);
}

void Server::SyncLobbyGlobalSettings() {
  if (State() != ServerState::WaitingPlayers)
    return;

  std::lock_guard lock{mutex_lobby};

  auto pool = yaml::StringPool{};
  auto vec_nodes = std::vector<yaml::MiniYamlNode>{
      net::SerializeSessionGlobalSettings(pool, LobbyInfo.global_settings)};

  DispatchServerOrdersToClients(SerializeImmediateTargetString(
      "SyncLobbyGlobalSettings", yaml::WriteToString(vec_nodes)));
  for (auto* t : vec_iface_sync_lobby_info_)
    t->LobbyInfoSynced(*this);
}

void Server::StartGame() {
  std::lock_guard lock{mutex_lobby};

  WriteLineWithTimeStamp(deps_.fn_fluent_get(kGameStarted));

  auto snapshot = vec_conns;
  for (auto& c : snapshot) {
    const auto* p_client = GetClient(*c);
    if (!c->b_validated.load() || p_client == nullptr ||
        p_client->State == net::ClientState::Invalid) {
      SendOrderTo(*c, "ServerError", kYouWereKicked);
      DropClient(*c);
    }
  }

  LobbyInfo.global_settings.EnableGameSaves =
      Type != ServerType::Dedicated &&
      LobbyInfo.NonBotClients().size() == 1;

  auto player_random = MersenneTwister(LobbyInfo.global_settings.RandomSeed);
  vec_world_players_.clear();
  if (deps_.fn_create_server_players)
    deps_.fn_create_server_players(LobbyInfo, vec_world_players_, player_random);

  auto game_info = GameInformation{};
  game_info.Mod = deps_.str_mod_id;
  game_info.Version = deps_.str_mod_version;
  game_info.MapUid = str_map_uid;
  game_info.MapTitle = str_map_title;
  game_info.StartTimeUtc = GameDateTime::UtcNow();

  for (const auto& world : vec_world_players_)
    if (world)
      game_info.vec_players.push_back(*world);

  up_game_info_ = std::move(game_info);

  if (up_recorder_) {
    up_replay_metadata_ =
        std::make_unique<net::ReplayMetadata>(up_game_info_->Serialize());
    up_recorder_->Metadata = up_replay_metadata_.get();
  }

  SyncLobbyInfo();

  const auto* speeds = deps_.fn_game_speeds ? deps_.fn_game_speeds() : nullptr;
  auto fallback_speeds = game::GameSpeeds{};
  if (speeds == nullptr) {
    fallback_speeds.DefaultSpeed = "default";
    fallback_speeds.Speeds.push_back({"default", game::GameSpeed{"default", 40, 3}});
    speeds = &fallback_speeds;
  }

  const auto str_speed_name = LobbyInfo.global_settings.OptionOrDefault(
      "gamespeed", speeds->DefaultSpeed);
  const game::GameSpeed* speed = speeds->Find(str_speed_name);
  if (speed == nullptr)
    speed = speeds->Find(speeds->DefaultSpeed);
  if (speed == nullptr) {
    static const game::GameSpeed kFallbackSpeed{std::string{"default"}, 40, 3};
    speed = &kFallbackSpeed;
  }

  auto vec_player_indexes = std::vector<int>{};
  for (auto& c : vec_conns)
    if (c->b_validated.load())
      vec_player_indexes.push_back(c->int4_player_index);

  order_buffer_.Start(*speed, std::move(vec_player_indexes));

  SetState(ServerState::GameStarted);

  if (IsMultiplayer())
    OrderLatency = speed->OrderLatency;

  LobbyInfo.global_settings.GameTimestep = speed->Timestep;

  DispatchServerOrdersToClients(
      SerializeImmediateTargetString("StartGame", ""));

  for (auto* t : vec_iface_start_game_)
    t->GameStarted(*this);

  auto int4_first_frame = 1;

  auto conns = std::vector<std::shared_ptr<ServerConnection>>{};
  for (auto& c : vec_conns)
    if (c->b_validated.load())
      conns.push_back(c);

  for (auto& from : conns) {
    for (auto i = 0; i < OrderLatency; ++i) {
      from->int4_last_orders_frame.store(int4_first_frame + i);
      const auto vec_frame_data =
          CreateFrame(from->int4_player_index, from->int4_last_orders_frame.load(),
                      std::vector<std::uint8_t>{});
      for (auto& to : conns)
        DispatchFrameToClient(*to, from->int4_player_index, vec_frame_data);

      RecordOrder(from->int4_last_orders_frame.load(), std::vector<std::uint8_t>{},
                  from->int4_player_index);
    }
  }
}

bool Server::InterpretCommand(std::string_view str_command,
                              ServerConnection& conn) {
  for (auto* t : vec_iface_interpret_command_)
    if (t->InterpretCommand(*this, conn, GetClient(conn), str_command))
      return true;

  return false;
}

net::ConnectionTarget Server::GetEndpointForLocalConnection() {
  auto vec_endpoints = std::vector<net::DnsEndpoint>{};
  for (const auto& [str_host, int4_port] : vec_local_endpoints_) {
    if (str_host == "::" || str_host == "::0")
      vec_endpoints.push_back({"::1", int4_port});
    else if (str_host.empty() || str_host == "0.0.0.0")
      vec_endpoints.push_back({"127.0.0.1", int4_port});
    else
      vec_endpoints.push_back({str_host, int4_port});
  }
  return net::ConnectionTarget{std::move(vec_endpoints)};
}

void Server::ServerLog(std::string_view str_line) const {
  if (deps_.fn_server_log)
    deps_.fn_server_log(str_line);
}

PlayerMessageTracker::PlayerMessageTracker(
    Server& server,
    std::function<void(ServerConnection&, int, int,
                       const std::vector<std::uint8_t>&)>
        fn_dispatch,
    std::function<void(ServerConnection&, std::string_view,
                       std::span<const FluentArgument>)>
        fn_fluent)
    : p_server_(&server),
      fn_dispatch_orders_to_client_(std::move(fn_dispatch)),
      fn_send_fluent_message_to_(std::move(fn_fluent)) {}

void PlayerMessageTracker::DisableChatUI(ServerConnection& conn, int int4_time) {
  auto order = net::Order("DisableChatEntry", nullptr, false);
  order.uint4_extra_data = static_cast<std::uint32_t>(int4_time);
  fn_dispatch_orders_to_client_(conn, 0, 0, order.Serialize());
}

bool PlayerMessageTracker::IsPlayerAtFloodLimit(ServerConnection& conn) {
  if (!map_message_tracker_.contains(conn.int4_player_index))
    map_message_tracker_.emplace(conn.int4_player_index,
                                 std::vector<long>{});

  const auto* p_client = p_server_->GetClient(conn);
  const bool b_is_admin = p_client != nullptr && p_client->IsAdmin;
  const auto& settings = p_server_->Settings;
  const auto time = conn.ConnectionTimerMs();
  auto& tracker = map_message_tracker_[conn.int4_player_index];
  tracker.erase(std::remove_if(tracker.begin(), tracker.end(),
                               [&](long t) {
                                 return t + settings.FloodLimitInterval < time;
                               }),
                tracker.end());

  const auto CalculateRemaining = [](long cooldown, long now) {
    return (cooldown - now + 999) / 1000;
  };

  if (!b_is_admin && time < settings.FloodLimitJoinCooldown) {
    const auto remaining =
        CalculateRemaining(settings.FloodLimitJoinCooldown, time);
    fn_send_fluent_message_to_(
        conn, kChatTemporaryDisabled,
        std::vector<FluentArgument>{{"remaining", std::to_string(remaining),
                                     FluentArgumentType::Number}});
    return true;
  }

  if (static_cast<int>(tracker.size()) >= settings.FloodLimitMessageCount) {
    const auto remaining =
        CalculateRemaining(tracker.front() + settings.FloodLimitInterval, time);
    fn_send_fluent_message_to_(
        conn, kChatTemporaryDisabled,
        std::vector<FluentArgument>{{"remaining", std::to_string(remaining),
                                     FluentArgumentType::Number}});
    return true;
  }

  tracker.push_back(time);

  if (static_cast<int>(tracker.size()) >= settings.FloodLimitMessageCount) {
    const auto cooldown_delta = std::max(
        0, settings.FloodLimitCooldown - settings.FloodLimitInterval);
    for (auto& t : tracker)
      t = time + cooldown_delta;

    DisableChatUI(conn, settings.FloodLimitCooldown);
  }

  return false;
}

bool Server::MapIsUnknown(const std::string& str_uid) const {
  if (str_uid.empty())
    return true;

  const auto* p_map =
      deps_.fn_map_cache_at ? deps_.fn_map_cache_at(str_uid) : nullptr;
  const auto kind_status =
      p_map != nullptr ? p_map->Status() : map::MapStatus::Unavailable;
  return kind_status != map::MapStatus::Available &&
         kind_status != map::MapStatus::DownloadAvailable;
}

bool Server::MapIsKnown(const std::string& str_uid) const {
  if (str_uid.empty())
    return false;

  if (opt_map_pool.has_value() &&
      std::find(opt_map_pool->begin(), opt_map_pool->end(), str_uid) ==
          opt_map_pool->end())
    return false;

  const auto* p_map =
      deps_.fn_map_cache_at ? deps_.fn_map_cache_at(str_uid) : nullptr;
  const auto kind_status =
      p_map != nullptr ? p_map->Status() : map::MapStatus::Unavailable;
  return kind_status == map::MapStatus::Available ||
         kind_status == map::MapStatus::DownloadAvailable;
}

}
