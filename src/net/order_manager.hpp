#pragma once
import std;

#include "net/connection.hpp"
#include "net/order_io.hpp"
#include "net/session.hpp"
#include "net/tick_time.hpp"

namespace ora::sim {
class World;
}

namespace ora::net {

struct FramePacket {
  int frame = 0;
  std::optional<OrderPacket> orders;
};

class OrderManager {
 public:

  struct ClientOrder {
    int client = 0;
    std::unique_ptr<Order> order;
  };

  using ClockFn = std::function<std::int64_t()>;

  explicit OrderManager(std::unique_ptr<IConnection> conn);
  ~OrderManager();

  OrderManager(const OrderManager&) = delete;
  OrderManager& operator=(const OrderManager&) = delete;

  Session& LobbyInfo() { return lobby_info_; }
  const Session& LobbyInfo() const { return lobby_info_; }

  SessionClient* LocalClient() {
    return lobby_info_.ClientWithIndex(connection_->LocalClientId());
  }

  sim::World* World() { return p_world_; }
  void SetWorld(sim::World* w) { p_world_ = w; }

  int OrderQueueLength() const;

  int NetFrameNumber() const { return int4_net_frame_number_; }
  int LocalFrameNumber() const { return int4_local_frame_number_; }
  void SetLocalFrameNumber(int n) { int4_local_frame_number_ = n; }

  bool GameStarted() const { return int4_net_frame_number_ != 0; }
  IConnection& Connection() { return *connection_; }

  const std::string& ServerError() const { return str_server_error_; }
  void SetServerError(std::string str_error) {
    str_server_error_ = std::move(str_error);
  }
  bool AuthenticationFailed() const { return b_authentication_failed_; }
  void SetAuthenticationFailed(bool b_failed) {
    b_authentication_failed_ = b_failed;
  }

  const std::optional<std::set<std::string>>& ServerMapPool() const {
    return set_server_map_pool_;
  }
  void SetServerMapPool(std::optional<std::set<std::string>> set_pool) {
    set_server_map_pool_ = std::move(set_pool);
  }

  void StartGame();

  void IssueOrders(const std::vector<Order>& orders);
  void IssueOrder(const Order& order);
  void ReceiveDisconnect(int client_id, int frame);
  void ReceiveSync(const SyncPacketData& sync);
  void ReceiveTickScale(float scale) { fp4_tick_scale_ = scale; }
  void ReceiveImmediateOrders(int client_id, const OrderPacket& orders);
  void ReceiveOrders(int client_id, int frame, OrderPacket orders);

  void TickImmediate();

  bool TryTick();

  int GameSaveLastFrame() const { return int4_game_save_last_frame_; }
  void SetGameSaveLastFrame(int f) { int4_game_save_last_frame_ = f; }
  int GameSaveLastSyncFrame() const { return int4_game_save_last_sync_frame_; }
  void SetGameSaveLastSyncFrame(int f) { int4_game_save_last_sync_frame_ = f; }

  int SuggestedTimestep() const;

  TickTime& LastTickTime() { return tick_time_; }

  bool IsOutOfSync() const { return b_is_out_of_sync_; }

  void SetClock(ClockFn fn_clock) { fn_clock_ = std::move(fn_clock); }

  void SetOrderProcessor(
      std::function<void(OrderManager&, sim::World*, int, const Order&)>
          fn_process) {
    fn_process_order_ = std::move(fn_process);
  }

  void SetOutOfSyncHandler(std::function<void(int)> fn) {
    fn_out_of_sync_ = std::move(fn);
  }

 private:
  void SendImmediateOrders();
  void SendOrders();
  void ProcessOrders();
  void OutOfSync(int frame);

  bool IsReadyForNextFrame() const;

  bool IsNetFrame() const {
    return int4_local_frame_number_ %
               lobby_info_.global_settings.NetFrameInterval ==
           0;
  }

  std::unique_ptr<IConnection> connection_;
  Session lobby_info_;

  std::map<int, std::deque<FramePacket>> map_pending_orders_;
  std::map<int, SyncPacketData> map_sync_for_frame_;

  sim::World* p_world_ = nullptr;

  int int4_net_frame_number_ = 0;
  int int4_local_frame_number_ = 0;
  TickTime tick_time_;
  ClockFn fn_clock_ = [] { return std::int64_t{0}; };

  std::vector<Order> vec_local_orders_;

  std::vector<const Order*> vec_local_order_ptrs_;
  std::vector<Order> vec_local_immediate_orders_;
  std::vector<const Order*> vec_local_immediate_ptrs_;

  std::vector<ClientOrder> vec_process_client_orders_;
  std::vector<int> vec_process_clients_to_remove_;

  bool b_disposed_ = false;
  bool b_generate_sync_report_ = false;
  int int4_sent_orders_frame_ = 0;
  float fp4_tick_scale_ = 1.0f;
  bool b_is_out_of_sync_ = false;

  std::string str_server_error_;
  bool b_authentication_failed_ = false;
  std::optional<std::set<std::string>> set_server_map_pool_;

  int int4_game_save_last_frame_ = -1;
  int int4_game_save_last_sync_frame_ = -1;

  std::function<void(OrderManager&, sim::World*, int, const Order&)>
      fn_process_order_;
  std::function<void(int)> fn_out_of_sync_;
};

}
