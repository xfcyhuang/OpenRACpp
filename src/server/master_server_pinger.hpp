#pragma once
import std;

#include "platform/udp_socket.hpp"
#include "server/server_traits.hpp"

namespace ora::server {

struct MasterServerPingerDeps {

  std::function<std::string()> fn_advertise_endpoint;

  std::function<void(std::string str_endpoint, std::string str_data,
                     std::function<void(bool b_ok, std::string str_response)>
                         fn_done)>
      fn_post;

  std::function<void(std::string_view)> fn_server_log;
};

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
  static constexpr int kMasterPingInterval = 60 * 3 * 1000;
  static constexpr int kRateLimitInterval = 1000;

  explicit MasterServerPinger(MasterServerPingerDeps deps = {});

  void Tick(Server& server) override;
  void ServerStarted(Server& server) override;
  void ServerShutdown(Server& server) override;
  void LobbyInfoSynced(Server& server) override;
  void GameStarted(Server& server) override;
  void GameEnded(Server& server) override;

 private:

  void UpdateMasterServer(const Server& server, std::string str_post_data);

  MasterServerPingerDeps deps_;

  struct SharedState {
    std::mutex mutex_messages;
    std::deque<std::string> deque_master_server_messages;
    std::atomic<bool> b_is_busy{false};
    std::atomic<bool> b_is_initial_ping{true};
  };
  std::shared_ptr<SharedState> ptr_state_ = std::make_shared<SharedState>();

  static int LanAdvertisePort();

  std::unique_ptr<LanBeacon> up_lan_beacon_;
  std::int64_t i8_last_ping_ = 0;
  std::int64_t i8_last_changed_ = 0;
};

}
