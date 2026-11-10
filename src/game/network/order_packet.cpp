module ora.game.world;

import std;

namespace ora::game::network {

namespace {

constexpr std::uint8_t kTypeSyncHash{0x65};
constexpr std::uint8_t kTypeDisconnect{0xbf};

void append_i4le(std::vector<std::uint8_t>& vec_out, std::int32_t int4_value) {
  const std::uint32_t u4_value{static_cast<std::uint32_t>(int4_value)};
  vec_out.push_back(static_cast<std::uint8_t>(u4_value & 0xff));
  vec_out.push_back(static_cast<std::uint8_t>((u4_value >> 8) & 0xff));
  vec_out.push_back(static_cast<std::uint8_t>((u4_value >> 16) & 0xff));
  vec_out.push_back(static_cast<std::uint8_t>((u4_value >> 24) & 0xff));
}

void append_u8le(std::vector<std::uint8_t>& vec_out, std::uint64_t u8_value) {
  for (std::uint32_t idx{}; idx < 8; ++idx)
    vec_out.push_back(static_cast<std::uint8_t>((u8_value >> (8 * idx)) & 0xff));
}

std::int32_t read_i4le(std::span<const std::uint8_t> span_bytes, std::size_t idx_offset) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(span_bytes[idx_offset]) |
                                   (static_cast<std::uint32_t>(span_bytes[idx_offset + 1]) << 8) |
                                   (static_cast<std::uint32_t>(span_bytes[idx_offset + 2]) << 16) |
                                   (static_cast<std::uint32_t>(span_bytes[idx_offset + 3]) << 24));
}

}

std::vector<std::uint8_t> order_packet_from_orders(const std::vector<OrderModel>& vec_orders) {
  std::vector<std::uint8_t> vec_out;
  for (const OrderModel& order : vec_orders) {
    const std::vector<std::uint8_t> vec_bytes{serialize_order(order)};
    vec_out.insert(vec_out.end(), vec_bytes.begin(), vec_bytes.end());
  }
  return vec_out;
}

std::vector<OrderModel> order_packet_get_orders(std::span<const std::uint8_t> span_data,
                                                const world::WorldModel* ptr_world) {
  std::vector<OrderModel> vec_out;
  OrderReader reader{span_data};
  while (reader.idx_pos < span_data.size()) {
    const std::optional<OrderModel> opt_order{deserialize_order(reader, ptr_world)};
    if (opt_order.has_value()) vec_out.push_back(std::move(*opt_order));
  }
  return vec_out;
}

std::vector<std::uint8_t> order_packet_serialize_frame(std::span<const std::uint8_t> span_data,
                                                       std::int32_t int4_frame) {
  std::vector<std::uint8_t> vec_out;
  vec_out.reserve(span_data.size() + 4);
  append_i4le(vec_out, int4_frame);
  vec_out.insert(vec_out.end(), span_data.begin(), span_data.end());
  return vec_out;
}

std::vector<std::uint8_t> serialize_sync_packet(std::int32_t int4_frame,
                                                std::int32_t int4_sync_hash,
                                                std::uint64_t u8_defeat_state) {
  std::vector<std::uint8_t> vec_out;
  vec_out.reserve(17);
  append_i4le(vec_out, int4_frame);
  vec_out.push_back(kTypeSyncHash);
  append_i4le(vec_out, int4_sync_hash);
  append_u8le(vec_out, u8_defeat_state);
  return vec_out;
}

std::optional<ParsedOrderPacket> try_parse_order_packet(
    std::span<const std::uint8_t> span_packet) {
  if (span_packet.size() < 4) return std::nullopt;

  if (span_packet.size() >= 5 &&
      (span_packet[4] == kTypeDisconnect || span_packet[4] == kTypeSyncHash))
    return std::nullopt;

  ParsedOrderPacket parsed;
  parsed.int4_frame = read_i4le(span_packet, 0);
  if (span_packet.size() > 4)
    parsed.vec_order_bytes.assign(span_packet.begin() + 4, span_packet.end());
  return parsed;
}

std::optional<ParsedSyncPacket> try_parse_sync_packet(
    std::span<const std::uint8_t> span_packet) {
  if (span_packet.size() != 17 || span_packet[4] != kTypeSyncHash) return std::nullopt;

  ParsedSyncPacket parsed;
  parsed.int4_frame = read_i4le(span_packet, 0);
  parsed.int4_sync_hash = read_i4le(span_packet, 5);
  std::uint64_t u8_defeat{};
  for (std::uint32_t idx{}; idx < 8; ++idx)
    u8_defeat |= static_cast<std::uint64_t>(span_packet[9 + idx]) << (8 * idx);
  parsed.u8_defeat_state = u8_defeat;
  return parsed;
}

}
