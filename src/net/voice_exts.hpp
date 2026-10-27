#pragma once
import std;

namespace ora::net {
struct Order;
}

namespace ora {

void PlayVoiceForOrders(
    std::span<const std::unique_ptr<net::Order>> vec_orders);

}
