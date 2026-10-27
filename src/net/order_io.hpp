#pragma once
import std;

#include "net/byte_io.hpp"
#include "net/order.hpp"

namespace ora::net {

class OrderPacket {
 public:
  OrderPacket() = default;

  explicit OrderPacket(const std::vector<const Order*>& vec_orders);

  static OrderPacket FromBytes(std::vector<std::uint8_t> vec_data) {
    OrderPacket p;
    p.vec_data_ = std::move(vec_data);
    return p;
  }

  std::vector<std::unique_ptr<Order>> GetOrders(sim::World* world) const;

  bool IsEmpty() const { return vec_data_.empty(); }
  const std::vector<std::uint8_t>& Data() const { return vec_data_; }
  std::size_t Size() const { return vec_data_.size(); }

  std::vector<std::uint8_t> Serialize(int frame) const;

  static OrderPacket Combine(const std::vector<const OrderPacket*>& packets);

 private:
  std::vector<std::uint8_t> vec_data_;
};

struct SyncPacketData {
  int frame = 0;
  int sync_hash = 0;
  std::uint64_t uint8_defeat_state = 0;
};

struct DisconnectData {
  int frame = 0;
  int client_id = 0;
};

struct Packet {
  int from_client = 0;
  std::vector<std::uint8_t> vec_data;
};

class OrderIO {
 public:

  static std::vector<std::uint8_t> SerializeSync(const SyncPacketData& data);

  static std::vector<std::uint8_t> SerializePingResponse(std::int64_t timestamp,
                                                         std::uint8_t queue_length);

  static bool TryParseDisconnect(const Packet& packet, DisconnectData& out);

  static bool TryParseSync(const std::vector<std::uint8_t>& packet,
                           SyncPacketData& out);

  static bool TryParseTickScale(const Packet& packet, float& out_scale);

  static bool TryParsePingRequest(const Packet& packet,
                                  std::int64_t& out_timestamp);

  static bool TryParseAck(const Packet& packet, int& out_frame,
                          std::uint8_t& out_count);

  static bool TryParseOrderPacket(const std::vector<std::uint8_t>& packet,
                                  int& out_frame, OrderPacket& out_orders);
};

}
