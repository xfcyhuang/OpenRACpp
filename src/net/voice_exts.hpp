#pragma once
import std;

namespace ora::net {
struct Order;
}

namespace ora {

void PlayVoiceForOrders(std::span<net::Order* const> vec_orders);

}  // namespace ora
