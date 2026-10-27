#include "net/connection.hpp"

#include "net/order_manager.hpp"

namespace ora::net {

void EchoConnection::Receive(OrderManager& order_manager) {
  while (!queue_immediate_orders_.empty()) {
    auto i = std::move(queue_immediate_orders_.front());
    queue_immediate_orders_.pop();
    order_manager.ReceiveImmediateOrders(kLocalClientId, i);

    if (b_disposed_)
      break;
  }

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

}
