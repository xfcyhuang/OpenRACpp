#pragma once

import std;

#include "game/network/order.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {

[[nodiscard]] std::vector<std::uint8_t> order_packet_from_orders(
    const std::vector<OrderModel>& vec_orders);

[[nodiscard]] std::vector<OrderModel> order_packet_get_orders(
    std::span<const std::uint8_t> span_data, const world::WorldModel* ptr_world);

[[nodiscard]] std::vector<std::uint8_t> order_packet_serialize_frame(
    std::span<const std::uint8_t> span_data, std::int32_t int4_frame);

[[nodiscard]] std::vector<std::uint8_t> serialize_sync_packet(std::int32_t int4_frame,
                                                              std::int32_t int4_sync_hash,
                                                              std::uint64_t u8_defeat_state);

struct ParsedOrderPacket {
  std::int32_t int4_frame{};
  std::vector<std::uint8_t> vec_order_bytes;
};

[[nodiscard]] std::optional<ParsedOrderPacket> try_parse_order_packet(
    std::span<const std::uint8_t> span_packet);

struct ParsedSyncPacket {
  std::int32_t int4_frame{};
  std::int32_t int4_sync_hash{};
  std::uint64_t u8_defeat_state{};
};

[[nodiscard]] std::optional<ParsedSyncPacket> try_parse_sync_packet(
    std::span<const std::uint8_t> span_packet);

}
