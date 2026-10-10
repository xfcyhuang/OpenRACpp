// UPSTREAM: OpenRA.Game/Network/Connection.cs @b6fc03f L24-97(IConnection +
//          EchoConnection 逐语义重写;NetworkConnection 随 Phase 7 网络批;
//          IsReplay/ReplayTickCount = 上游 `is ReplayConnection` 类型判与
//          TickCount 取值的虚面 —— World.IsReplay 与 OrderManager
//          .SuggestedTimestep 的消费点,第十一批随 ReplayConnection 全文化)
//          IConnection + EchoConnection, verbatim-semantics; Network
//          Connection rides the Phase 7 network batch; IsReplay/
//          ReplayTickCount are the virtual face of upstream's `is
//          ReplayConnection` test + TickCount read (consumed by
//          World.IsReplay and OrderManager.SuggestedTimestep, wired with
//          batch 11's full ReplayConnection).
#pragma once
import std;

#include "net/order_io.hpp"

namespace ora::net {

class OrderManager;

enum class ConnectionState { PreConnecting, NotConnected, Connecting, Connected };

/// IConnection(Connection.cs L32-40)
class IConnection {
 public:
  virtual ~IConnection() = default;

  virtual int LocalClientId() = 0;
  virtual void StartGame() = 0;
  virtual void Send(int frame, const std::vector<const Order*>& orders) = 0;
  virtual void SendImmediate(const std::vector<const Order*>& orders) = 0;
  virtual void SendSync(int frame, int sync_hash,
                        std::uint64_t uint8_defeat_state) = 0;
  virtual void Receive(OrderManager& order_manager) = 0;

  virtual bool IsReplay() const { return false; }
  virtual int ReplayTickCount() const { return -1; }
};

/// EchoConnection(Connection.cs L42-97):单机回环 —— 本地客户端即服务器
/// EchoConnection (Connection.cs L42-97): the single-player loopback — the
/// local client is the server.
class EchoConnection final : public IConnection {
 public:
  static constexpr int kLocalClientId = 1;

  int LocalClientId() override { return kLocalClientId; }

  void StartGame() override {
    // Inject an empty frame to fill the gap we are making by projecting
    // forward orders
    queue_orders_.emplace(0, OrderPacket{});
  }

  void Send(int frame, const std::vector<const Order*>& orders) override {
    queue_orders_.emplace(frame, OrderPacket{orders});
  }

  void SendImmediate(const std::vector<const Order*>& orders) override {
    queue_immediate_orders_.push(OrderPacket{orders});
  }

  void SendSync(int frame, int sync_hash,
                std::uint64_t uint8_defeat_state) override {
    queue_sync_.emplace(SyncPacketData{frame, sync_hash,
                                       uint8_defeat_state});
  }

  void Receive(OrderManager& order_manager) override;

  void Dispose() { b_disposed_ = true; }
  bool IsDisposed() const { return b_disposed_; }

 private:
  std::queue<SyncPacketData> queue_sync_;
  std::queue<std::pair<int, OrderPacket>> queue_orders_;
  std::queue<OrderPacket> queue_immediate_orders_;
  bool b_disposed_ = false;
};

}  // namespace ora::net
