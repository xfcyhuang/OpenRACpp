// UPSTREAM: OpenRA.Mods.Common/ServerTraits/MasterServerPinger.cs
//          @e180e3225 L27-217 全文(3min 广告刷新/1s 限速的 Tick 门 +
//          isBusy 双闸 + GameServer.ToPOSTData 的在线/LAN 双投递 +
//          首投的 [code]message 应答解析与错误码表 + 消息队列的服务器线
//          程汇流;BeaconLib.Beacon → 自持 LanBeacon(UDP 35891 发现应
//          答,RIX0RRR 编码:2 字节小端长度前缀 + UTF-8;LanAdvertisePort
//          的 System.Random(ms) → MersenneTwister(ms),值域面等价);
//          HttpClient → 注入面(缺省 = 立即失败 → Error 通知,与不可达
//          主服务器同 observable);Game.RunTime → Server::RunTimeMs)
//          The whole of MasterServerPinger.cs L27-217 (the 3min
//          advertisement refresh / the 1s rate-limited Tick gate + the
//          isBusy double gate + GameServer.ToPOSTData's online/LAN dual
//          delivery + the first ping's [code]message response parsing with
//          the error-code table + the message queue's server-thread drain;
//          BeaconLib.Beacon → a self-held LanBeacon (the UDP 35891
//          discovery responder, rix0rrr encoding: a 2-byte little-endian
//          length prefix + UTF-8; LanAdvertisePort's System.Random(ms) →
//          MersenneTwister(ms), value-domain equivalence); HttpClient → an
//          injection face (default = immediate failure → the Error
//          notification, the same observable as an unreachable master);
//          Game.RunTime → Server::RunTimeMs).
#pragma once
import std;

#include "platform/udp_socket.hpp"
#include "server/server_traits.hpp"


namespace ora::server {

struct MasterServerPingerDeps {
  /// WebServices.ServerAdvertise(主服务器公告 URL)
  /// WebServices.ServerAdvertise (the master-server advertisement URL).
  std::function<std::string()> fn_advertise_endpoint;

  /// HttpClient POST(endpoint, data → 应答文本;ok=false = 异常路径;
  ///      实现侧可同步或异步回调)
  /// The HttpClient POST (endpoint, data → the response text; ok=false =
  /// the exception path; the implementation may call back synchronously or
  /// asynchronously).
  std::function<void(std::string str_endpoint, std::string str_data,
                     std::function<void(bool b_ok, std::string str_response)>
                         fn_done)>
      fn_post;

  /// Log.Write("server", line)(缺省 = 丢弃)
  /// Log.Write("server", line) (default = dropped).
  std::function<void(std::string_view)> fn_server_log;
};

/// BeaconLib.Beacon("OpenRALANGame", port) 的发现应答器:35891 端口收
/// 探测包(前缀 = beaconType 编码),回 [type][advertisedPort 大端 2B]
/// [BeaconData 编码]
/// BeaconLib.Beacon("OpenRALANGame", port)'s discovery responder: the
/// probe packets (prefixed by the encoded beaconType) arrive on 35891 and
/// are answered with [type][advertisedPort, 2B big-endian][the encoded
/// BeaconData].
class LanBeacon final {
 public:
  LanBeacon(std::string str_beacon_type, int int4_advertised_port,
            std::function<void(std::string_view)> fn_log);
  ~LanBeacon();

  LanBeacon(const LanBeacon&) = delete;
  LanBeacon& operator=(const LanBeacon&) = delete;

  bool IsValid() const { return b_valid_; }
  void SetBeaconData(std::string str_data);
  void Start();
  void Stop();

 private:
  void ReceiveLoop();

  std::string str_beacon_type_;
  int int4_advertised_port_;
  std::function<void(std::string_view)> fn_log_;
  platform::UdpSocket udp_;
  std::atomic<bool> b_valid_{false};
  std::atomic<bool> b_stopped_{false};
  std::mutex mutex_data_;
  std::string str_beacon_data_;
  std::thread thread_;
  std::once_flag flag_start_;
};

class MasterServerPinger final : public ServerTrait,
                                 public ITick,
                                 public INotifyServerStart,
                                 public INotifyServerShutdown,
                                 public INotifySyncLobbyInfo,
                                 public IStartGame,
                                 public IEndGame {
 public:
  static constexpr int kMasterPingInterval = 60 * 3 * 1000;  // L30
  static constexpr int kRateLimitInterval = 1000;            // L33

  explicit MasterServerPinger(MasterServerPingerDeps deps = {});

  void Tick(Server& server) override;
  void ServerStarted(Server& server) override;
  void ServerShutdown(Server& server) override;
  void LobbyInfoSynced(Server& server) override;
  void GameStarted(Server& server) override;
  void GameEnded(Server& server) override;

 private:
  /// UpdateMasterServer(L145-203):后台投递 + 首投应答解析(HTTP 回调只触
  /// 达共享态,不触达 trait 生命周期)
  /// UpdateMasterServer (L145-203): the background delivery + the
  /// first-ping response parsing (the HTTP callback touches only the shared
  /// state, never the trait's lifetime).
  void UpdateMasterServer(const Server& server, std::string str_post_data);

  MasterServerPingerDeps deps_;

  /// 异步回调态(isBusy/masterServerMessages/isInitialPing 的归属块)
  /// The async-callback state (the home of isBusy/masterServerMessages/
  /// isInitialPing).
  struct SharedState {
    std::mutex mutex_messages;
    std::deque<std::string> deque_master_server_messages;
    std::atomic<bool> b_is_busy{false};
    std::atomic<bool> b_is_initial_ping{true};
  };
  std::shared_ptr<SharedState> ptr_state_ = std::make_shared<SharedState>();

  /// 进程级 LAN 广告端口(L53;System.Random(DateTime.Now.Millisecond).
  /// Next(2048, 60000) 的值域等价面)
  /// The process-wide LAN advertisement port (L53; the value-domain
  /// equivalent of System.Random(DateTime.Now.Millisecond).Next(2048,
  /// 60000)).
  static int LanAdvertisePort();

  std::unique_ptr<LanBeacon> up_lan_beacon_;
  std::int64_t i8_last_ping_ = 0;
  std::int64_t i8_last_changed_ = 0;
};

}  // namespace ora::server
