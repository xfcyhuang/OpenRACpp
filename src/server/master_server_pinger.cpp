// UPSTREAM: OpenRA.Mods.Common/ServerTraits/MasterServerPinger.cs
//          @e180e3225 L27-217 + BeaconLib 1.0.2(rix0rrr/beacon)的
//          Beacon.cs(实现体;Task.Run 的闭包态 → shared_ptr 共享态,HTTP
//          线程不触达 trait 生命周期 —— 上游 GC 常青语义的确定性等价)
//          The implementation body (MasterServerPinger.cs + BeaconLib
//          1.0.2's Beacon.cs; Task.Run's closure state → a shared_ptr state
//          block, so the HTTP thread never touches the trait's lifetime —
//          the deterministic equivalent of upstream's GC evergreen liveness).
#include "server/master_server_pinger.hpp"

#include "core/mersenne_twister.hpp"
#include "net/fluent_message.hpp"
#include "server/game_server.hpp"
#include "server/server.hpp"

namespace ora::server {

using ora::net::FluentArgument;
using ora::net::FluentArgumentType;
using ora::net::FluentMessage;

namespace {

constexpr std::string_view kNoPortForward = "notification-no-port-forward";
constexpr std::string_view kBlacklistedTitle =
    "notification-blacklisted-server-name";
constexpr std::string_view kInvalidErrorCode =
    "notification-invalid-error-code";
constexpr std::string_view kConnected = "notification-master-server-connected";
constexpr std::string_view kError = "notification-master-server-error";
constexpr std::string_view kGameOffline = "notification-game-offline";

constexpr std::string_view kBeaconType = "OpenRALANGame";
constexpr int kDiscoveryPort = 35891;  // BeaconLib.DiscoveryPort

/// BeaconLib.Encode:2 字节小端长度前缀 + UTF-8
/// BeaconLib.Encode: a 2-byte little-endian length prefix + UTF-8.
std::vector<std::uint8_t> BeaconEncode(std::string_view sv_text) {
  auto vec_out = std::vector<std::uint8_t>{};
  const auto u2_len = static_cast<std::uint16_t>(sv_text.size());
  vec_out.push_back(static_cast<std::uint8_t>(u2_len & 0xFF));
  vec_out.push_back(static_cast<std::uint8_t>((u2_len >> 8) & 0xFF));
  vec_out.insert(vec_out.end(), sv_text.begin(), sv_text.end());
  return vec_out;
}

/// BeaconLib.HasPrefix:已编码前缀的字节比对
/// BeaconLib.HasPrefix: a byte compare against the encoded prefix.
bool BeaconHasPrefix(std::span<const std::uint8_t> span_packet,
                     std::span<const std::uint8_t> span_prefix) {
  return span_packet.size() >= span_prefix.size() &&
         std::equal(span_prefix.begin(), span_prefix.end(),
                    span_packet.begin());
}

/// 首投应答的 ^\[(?<code>-?\d+)\](?<message>.*) 解析;返回 false =
/// InvalidErrorCode 形态
/// The first ping's ^\[(?<code>-?\d+)\](?<message>.*) parse; false = the
/// InvalidErrorCode shape.
bool ParseMasterResponse(const std::string& str_response, int& int4_code_out,
                         std::string& str_message_out) {
  const auto sz_first = str_response.find_first_not_of(" \t\r\n");
  if (sz_first == std::string::npos)
    return false;
  if (str_response[sz_first] != '[')
    return false;

  auto sz_pos = sz_first + 1;
  const auto sz_code_begin = sz_pos;
  if (sz_pos < str_response.size() && str_response[sz_pos] == '-')
    sz_pos++;
  while (sz_pos < str_response.size() && str_response[sz_pos] >= '0' &&
         str_response[sz_pos] <= '9')
    sz_pos++;

  const auto str_code =
      str_response.substr(sz_code_begin, sz_pos - sz_code_begin);
  auto int4_code = int{};
  const auto [ptr_end, ec] = std::from_chars(
      str_code.data(), str_code.data() + str_code.size(), int4_code);
  (void)ptr_end;
  if (ec != std::errc{})
    return false;

  if (sz_pos >= str_response.size() || str_response[sz_pos] != ']')
    return false;

  auto str_message = str_response.substr(sz_pos + 1);
  const auto sz_trim_begin = str_message.find_first_not_of(" \t\r\n");
  const auto sz_trim_end = str_message.find_last_not_of(" \t\r\n");
  int4_code_out = int4_code;
  str_message_out = sz_trim_begin == std::string::npos
                        ? std::string{}
                        : str_message.substr(
                              sz_trim_begin, sz_trim_end - sz_trim_begin + 1);
  return true;
}

}  // namespace

// ———— LanBeacon(BeaconLib.Beacon 的直连形态)————
// ———— LanBeacon (BeaconLib.Beacon's direct form) ——

LanBeacon::LanBeacon(std::string str_beacon_type, int int4_advertised_port,
                     std::function<void(std::string_view)> fn_log)
    : str_beacon_type_{std::move(str_beacon_type)},
      int4_advertised_port_{int4_advertised_port},
      fn_log_{std::move(fn_log)} {
  udp_ = platform::UdpSocket::BindAny(kDiscoveryPort);
  b_valid_ = udp_.IsValid();
  if (!b_valid_ && fn_log_)
    fn_log_("BeaconLib.Beacon: beacon bind failed");
}

LanBeacon::~LanBeacon() {
  Stop();
}

void LanBeacon::SetBeaconData(std::string str_data) {
  std::lock_guard lock{mutex_data_};
  str_beacon_data_ = std::move(str_data);
}

void LanBeacon::Start() {
  std::call_once(flag_start_, [this]() {
    b_stopped_.store(false);
    thread_ = std::thread{[this]() { ReceiveLoop(); }};
  });
}

void LanBeacon::Stop() {
  b_stopped_.store(true);
  udp_.Close();
  if (thread_.joinable())
    thread_.join();
}

void LanBeacon::ReceiveLoop() {
  const auto vec_prefix = BeaconEncode(str_beacon_type_);
  auto arr_buf = std::array<std::uint8_t, 65536>{};
  while (!b_stopped_.load()) {
    auto addr_peer = platform::SocketAddress{};
    const int int4_n =
        udp_.ReceiveFrom(arr_buf.data(), arr_buf.size(), addr_peer);
    if (int4_n <= 0)
      break;

    if (!BeaconHasPrefix(std::span{arr_buf.data(), arr_buf.data() + int4_n},
                         vec_prefix))
      continue;

    auto vec_reply = std::vector<std::uint8_t>{};
    vec_reply.insert(vec_reply.end(), vec_prefix.begin(), vec_prefix.end());

    // AdvertisedPort 的 2 字节大端序(HostToNetworkOrder)
    // The AdvertisedPort's 2 big-endian bytes (HostToNetworkOrder).
    const auto u2_port = static_cast<std::uint16_t>(int4_advertised_port_);
    vec_reply.push_back(static_cast<std::uint8_t>((u2_port >> 8) & 0xFF));
    vec_reply.push_back(static_cast<std::uint8_t>(u2_port & 0xFF));

    std::string str_data;
    {
      std::lock_guard lock{mutex_data_};
      str_data = str_beacon_data_;
    }
    const auto vec_data = BeaconEncode(str_data);
    vec_reply.insert(vec_reply.end(), vec_data.begin(), vec_data.end());

    udp_.SendTo(addr_peer, vec_reply);
  }
}

// ———— MasterServerPinger ——

int MasterServerPinger::LanAdvertisePort() {
  // System.Random(DateTime.Now.Millisecond).Next(2048, 60000) 的值域等
  // 价面(端口号值不影响协议语义;D225)
  // The value-domain equivalent of System.Random(DateTime.Now.Millisecond)
  // .Next(2048, 60000) (the port value carries no protocol semantics; D225).
  static const int int4_port = [] {
    const auto ms_now =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() %
        1000;
    return MersenneTwister(static_cast<std::int32_t>(ms_now)).Next(2048, 60000);
  }();
  return int4_port;
}

MasterServerPinger::MasterServerPinger(MasterServerPingerDeps deps)
    : deps_{std::move(deps)} {}

void MasterServerPinger::Tick(Server& server) {
  if (!server.IsMultiplayer())
    return;

  const auto i8_now = server.RunTimeMs();

  if (i8_now - i8_last_changed_ > kMasterPingInterval)
    i8_last_changed_ = i8_now;

  if ((server.Settings.AdvertiseOnline ||
       server.Settings.AdvertiseOnLocalNetwork) &&
      !ptr_state_->b_is_busy.load() &&
      ((i8_last_changed_ > i8_last_ping_ &&
        i8_now - i8_last_ping_ > kRateLimitInterval) ||
       ptr_state_->b_is_initial_ping.load())) {
    const auto gs = GameServerData{server};
    if (server.Settings.AdvertiseOnline)
      UpdateMasterServer(server, gs.ToPOSTData(false));

    if (server.Settings.AdvertiseOnLocalNetwork && up_lan_beacon_)
      up_lan_beacon_->SetBeaconData(gs.ToPOSTData(true));

    i8_last_ping_ = i8_now;
  }

  if (server.Settings.AdvertiseOnline) {
    std::deque<std::string> deque_drain;
    {
      std::lock_guard lock{ptr_state_->mutex_messages};
      deque_drain.swap(ptr_state_->deque_master_server_messages);
    }
    while (!deque_drain.empty()) {
      server.SendFluentMessage(deque_drain.front());
      deque_drain.pop_front();
    }
  }
}

void MasterServerPinger::ServerStarted(Server& server) {
  if (server.IsMultiplayer() && server.Settings.AdvertiseOnLocalNetwork) {
    // CreateLanGameBeacon(L205-217):旧 beacon 停止后重建;绑定失败 =
    // null(上游 catch 形态)
    // CreateLanGameBeacon (L205-217): the old beacon is stopped and
    // recreated; a bind failure = null (upstream's catch shape).
    if (up_lan_beacon_) {
      up_lan_beacon_->Stop();
      up_lan_beacon_.reset();
    }

    up_lan_beacon_ = std::make_unique<LanBeacon>(
        std::string{kBeaconType}, LanAdvertisePort(), deps_.fn_server_log);
    if (!up_lan_beacon_->IsValid())
      up_lan_beacon_.reset();

    if (up_lan_beacon_)
      up_lan_beacon_->Start();
  }
}

void MasterServerPinger::ServerShutdown(Server& server) {
  if (!server.IsMultiplayer())
    return;

  if (server.Settings.AdvertiseOnline) {
    const auto game_server = GameServerData{server};
    UpdateMasterServer(server, game_server.ToPOSTData(false));
  }

  if (up_lan_beacon_) {
    up_lan_beacon_->Stop();
    up_lan_beacon_.reset();
  }
}

void MasterServerPinger::LobbyInfoSynced(Server& server) {
  i8_last_changed_ = server.RunTimeMs();
}

void MasterServerPinger::GameStarted(Server& server) {
  i8_last_changed_ = server.RunTimeMs();
}

void MasterServerPinger::GameEnded(Server& server) {
  if (up_lan_beacon_) {
    up_lan_beacon_->Stop();
    up_lan_beacon_.reset();
  }

  i8_last_changed_ = server.RunTimeMs();
}

void MasterServerPinger::UpdateMasterServer(const Server&,
                                            std::string str_post_data) {
  ptr_state_->b_is_busy.store(true);

  const bool b_initial = ptr_state_->b_is_initial_ping.load();
  const auto fn_log = deps_.fn_server_log;

  const auto ptr_state = ptr_state_;
  const auto fn_done = [ptr_state, b_initial,
                        fn_log](bool b_ok, const std::string& str_response) {
    if (!b_ok) {
      if (fn_log)
        fn_log("Master server post failed");
      std::lock_guard lock{ptr_state->mutex_messages};
      ptr_state->deque_master_server_messages.emplace_back(kError);
      ptr_state->b_is_busy.store(false);
      return;
    }

    if (b_initial) {
      if (fn_log)
        fn_log(std::format("Master server: {}", str_response));

      auto int4_error_code = 0;
      auto str_error_message = std::string{};
      const auto b_blank =
          str_response.find_first_not_of(" \t\r\n") == std::string::npos;
      if (!b_blank) {
        int int4_code = 0;
        std::string str_message;
        if (ParseMasterResponse(str_response, int4_code, str_message)) {
          int4_error_code = int4_code;
          str_error_message = std::move(str_message);
        } else {
          str_error_message = std::string{kInvalidErrorCode};
        }
      }

      std::lock_guard lock{ptr_state->mutex_messages};
      ptr_state->deque_master_server_messages.emplace_back(kConnected);
      if (int4_error_code != 0) {
        std::string str_message;
        if (int4_error_code == 1)
          str_message = std::string{kNoPortForward};
        else if (int4_error_code == 2)
          str_message = std::string{kBlacklistedTitle};
        else
          str_message = str_error_message;

        ptr_state->deque_master_server_messages.emplace_back(std::move(str_message));

        if (int4_error_code > 0)
          ptr_state->deque_master_server_messages.emplace_back(kGameOffline);
      }

      ptr_state->b_is_initial_ping.store(false);
    }

    ptr_state->b_is_busy.store(false);
  };

  const auto str_endpoint =
      deps_.fn_advertise_endpoint ? deps_.fn_advertise_endpoint()
                                  : std::string{};
  if (!deps_.fn_post) {
    // HttpClient 构建失败的等价面:异常路径 → Error 通知
    // The equivalent of HttpClient construction failing: the exception
    // path → the Error notification.
    fn_done(false, "");
    return;
  }

  try {
    deps_.fn_post(str_endpoint, str_post_data, fn_done);
  } catch (const std::exception& ex) {
    if (fn_log)
      fn_log(ex.what());
    std::lock_guard lock{ptr_state_->mutex_messages};
    ptr_state_->deque_master_server_messages.emplace_back(kError);
    ptr_state_->b_is_busy.store(false);
  }
}

}  // namespace ora::server
