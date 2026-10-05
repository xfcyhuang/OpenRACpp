// UPSTREAM: OpenRA.Game/Network/OrderManager.cs @b6fc03f L22-334(逐语义重写;
//          SyncReport/TextNotificationsManager/Game 静态面以桩/注入承载 ——
//          Phase 5/6/7 接线)
//          Verbatim-semantics rewrite; the SyncReport/TextNotifications
//          Manager/Game statics ride stubs/injections — wired in Phase
//          5/6/7.
//
// 机制对照 / Mechanism mapping:
//  - pendingOrders: Dictionary<int, Queue<(int, OrderPacket)>> →
//    std::map<int, std::deque<FramePacket>>(遍历序 = 客户端 ID 升序,
//    上游 Dictionary 枚举序差异不影响:ProcessOrders 逐客户端独立处理)
//    pendingOrders → std::map<int, std::deque<FramePacket>> (iteration
//    order = ascending client id; upstream's Dictionary enumeration order
//    does not matter: ProcessOrders handles each client independently).
//  - Order 队列化:C# Order 对象 → OrderPacket 字节(与上游同策略:本地与
//    远端一致化,见 OrderPacket 注)
//  - SyncReport:desync 诊断面(Phase 7);generateSyncReport 恒 false 路径
//    保真(EnableSyncReports 默认 false)
//  - Game.RunTime:时钟注入(ClockFn;默认恒 0 —— 单机锁步不依赖墙钟)
#pragma once
import std;

#include "net/connection.hpp"
#include "net/order_io.hpp"
#include "net/session.hpp"
#include "net/tick_time.hpp"

namespace ora::sim {
class World;
}  // namespace ora::sim

namespace ora::net {

/// 一帧的订单包(frame + 字节载荷)
/// One frame's order packet (frame + byte payload).
struct FramePacket {
  int frame = 0;
  std::optional<OrderPacket> orders;  // nullopt = ClientDisconnected 标记
};

/// OrderManager(OrderManager.cs L22-334)
class OrderManager {
 public:
  /// ClientOrder(L75-84)
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

  /// LocalClient(L36):观战(replay)时为 null
  SessionClient* LocalClient() {
    return lobby_info_.ClientWithIndex(connection_->LocalClientId());
  }

  sim::World* World() { return p_world_; }
  void SetWorld(sim::World* w) { p_world_ = w; }

  /// OrderQueueLength(L38)
  int OrderQueueLength() const;

  int NetFrameNumber() const { return int4_net_frame_number_; }
  int LocalFrameNumber() const { return int4_local_frame_number_; }
  void SetLocalFrameNumber(int n) { int4_local_frame_number_ = n; }

  bool GameStarted() const { return int4_net_frame_number_ != 0; }  // L51
  IConnection& Connection() { return *connection_; }

  /// StartGame(L98-116)
  void StartGame();

  // ———— 订单收发(L126-232)————
  void IssueOrders(const std::vector<Order>& orders);
  void IssueOrder(const Order& order);
  void ReceiveDisconnect(int client_id, int frame);
  void ReceiveSync(const SyncPacketData& sync);
  void ReceiveTickScale(float scale) { fp4_tick_scale_ = scale; }
  void ReceiveImmediateOrders(int client_id, const OrderPacket& orders);
  void ReceiveOrders(int client_id, int frame, OrderPacket orders);

  /// TickImmediate(L293-298):主循环每渲染帧调用(Game.LogicTick 序)
  void TickImmediate();

  /// TryTick(L300-328):锁步放行判定
  bool TryTick();

  int GameSaveLastFrame() const { return int4_game_save_last_frame_; }
  void SetGameSaveLastFrame(int f) { int4_game_save_last_frame_ = f; }
  int GameSaveLastSyncFrame() const { return int4_game_save_last_sync_frame_; }
  void SetGameSaveLastSyncFrame(int f) { int4_game_save_last_sync_frame_ = f; }

  /// SuggestedTimestep(L204-222;Ui.Timestep/ReplayConnection 面以 Timestep
  /// 兜底 —— Phase 6/7 接线)
  int SuggestedTimestep() const;

  TickTime& LastTickTime() { return tick_time_; }

  bool IsOutOfSync() const { return b_is_out_of_sync_; }

  /// 时钟注入(Game.RunTime 等价;默认恒 0)
  void SetClock(ClockFn fn_clock) { fn_clock_ = std::move(fn_clock); }

  /// ProcessOrders 面的 order 分发钩子(UnitOrders::ProcessOrder)
  void SetOrderProcessor(
      std::function<void(OrderManager&, sim::World*, int, const Order&)>
          fn_process) {
    fn_process_order_ = std::move(fn_process);
  }

  /// desync 时的世界联动(World.OutOfSync —— Phase 5 Game 接线;默认 no-op)
  void SetOutOfSyncHandler(std::function<void(int)> fn) {
    fn_out_of_sync_ = std::move(fn);
  }

 private:
  void SendImmediateOrders();
  void SendOrders();
  void ProcessOrders();
  void OutOfSync(int frame);

  /// IsReadyForNextFrame(L202)
  bool IsReadyForNextFrame() const;

  /// IsNetFrame(L332):LocalFrameNumber % NetFrameInterval == 0
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

  std::vector<Order> vec_local_orders_;            // localOrders(值拷贝语义:
                                                   // C# 对象引用经队列序列化面)
  std::vector<const Order*> vec_local_order_ptrs_;  // Send 视图
  std::vector<Order> vec_local_immediate_orders_;
  std::vector<const Order*> vec_local_immediate_ptrs_;

  std::vector<ClientOrder> vec_process_client_orders_;
  std::vector<int> vec_process_clients_to_remove_;

  bool b_disposed_ = false;
  bool b_generate_sync_report_ = false;
  int int4_sent_orders_frame_ = 0;
  float fp4_tick_scale_ = 1.0f;
  bool b_is_out_of_sync_ = false;

  int int4_game_save_last_frame_ = -1;
  int int4_game_save_last_sync_frame_ = -1;

  std::function<void(OrderManager&, sim::World*, int, const Order&)>
      fn_process_order_;
  std::function<void(int)> fn_out_of_sync_;
};

}  // namespace ora::net
