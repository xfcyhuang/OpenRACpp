// UPSTREAM: OpenRA.Game/Network/Connection.cs @7d57605 L73-96(EchoConnection
//          .Receive)
//          EchoConnection.Receive.
#include "net/connection.hpp"

#include "net/order_manager.hpp"

namespace ora::net {

void EchoConnection::Receive(OrderManager& order_manager) {
  while (!queue_immediate_orders_.empty()) {
    auto i = std::move(queue_immediate_orders_.front());
    queue_immediate_orders_.pop();
    order_manager.ReceiveImmediateOrders(kLocalClientId, i);

    // An immediate order may trigger a chain of actions that disposes the
    // OrderManager and connection. Bail out to avoid potential problems
    // from acting on disposed objects.
    if (b_disposed_)
      break;
  }

  // Project orders forward to the next frame
  while (!queue_orders_.empty()) {
    auto o = std::move(queue_orders_.front());
    queue_orders_.pop();
    order_manager.ReceiveOrders(kLocalClientId, o.first + 1,
                                std::move(o.second));
  }

  while (!queue_sync_.empty()) {
    auto s = queue_sync_.front();
    queue_sync_.pop();
    order_manager.ReceiveSync(s);
  }
}

}  // namespace ora::net
