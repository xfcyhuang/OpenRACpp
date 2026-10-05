// UPSTREAM: OpenRA.Game/Network/OrderIO.cs @b6fc03f L18-212(逐字节重写)
//          Byte-exact rewrite.
//
// 机制对照 / Mechanism mapping:
//  - OrderPacket:MemoryStream 载荷 → vector<uint8_t>;GetOrders 延迟解析
//    (依赖世界状态)→ 惰性反序列化迭代;Serialize(frame) = frame LE + 载荷
//  - OrderPacket: the MemoryStream payload → vector<uint8_t>; the
//    deferred GetOrders parsing (world-state dependent) → lazy
//    deserialization; Serialize(frame) = frame LE + payload.
//  - TryParse* 家族:长度/首字节魔数判定 + FromClient==0(仅服务器)门槛,
//    全部逐字段对齐
//    The TryParse* family: length/first-byte magic checks plus the
//    FromClient==0 (server-only) gate, aligned field by field.
#pragma once
import std;

#include "net/byte_io.hpp"
#include "net/order.hpp"

namespace ora::net {

/// OrderPacket(OrderIO.cs L18-77)
class OrderPacket {
 public:
  OrderPacket() = default;

  /// OrderPacket(IEnumerable<Order>):立即序列化(上游注释:本地与远端
  /// 行为一致化的关键 —— order 可能引用即将销毁的 actor)
  /// OrderPacket(IEnumerable<Order>): serialize immediately (the upstream
  /// note: the key to consistent local/remote behaviour — orders may
  /// reference actors about to die).
  explicit OrderPacket(const std::vector<const Order*>& vec_orders);

  static OrderPacket FromBytes(std::vector<std::uint8_t> vec_data) {
    OrderPacket p;
    p.vec_data_ = std::move(vec_data);
    return p;
  }

  /// GetOrders(world):惰性解析(消耗后不可重放 —— 上游 Position=0 一次性)
  /// GetOrders(world): lazy parsing (single-shot — upstream rewinds
  /// Position once).
  std::vector<std::unique_ptr<Order>> GetOrders(sim::World* world) const;

  bool IsEmpty() const { return vec_data_.empty(); }
  const std::vector<std::uint8_t>& Data() const { return vec_data_; }
  std::size_t Size() const { return vec_data_.size(); }

  /// Serialize(frame)(L55-64):frame LE + 载荷
  std::vector<std::uint8_t> Serialize(int frame) const;

  /// Combine(L66-77)
  static OrderPacket Combine(const std::vector<const OrderPacket*>& packets);

 private:
  std::vector<std::uint8_t> vec_data_;
};

/// OrderIO(OrderIO.cs L79-212)
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
  /// SerializeSync(L83-91):frame + 0x65 + hash + defeat(13+4 字节)
  static std::vector<std::uint8_t> SerializeSync(const SyncPacketData& data);

  /// SerializePingResponse(L93-101):0 + 0x20 + timestamp(int64) + queueLen
  static std::vector<std::uint8_t> SerializePingResponse(std::int64_t timestamp,
                                                         std::uint8_t queue_length);

  /// TryParseDisconnect(L103-116;仅服务器 FromClient==0)
  static bool TryParseDisconnect(const Packet& packet, DisconnectData& out);

  /// TryParseSync(L118-131)
  static bool TryParseSync(const std::vector<std::uint8_t>& packet,
                           SyncPacketData& out);

  /// TryParseTickScale(L133-152;frame==0 门槛)
  static bool TryParseTickScale(const Packet& packet, float& out_scale);

  /// TryParsePingRequest(L154-173;frame==0 门槛)
  static bool TryParsePingRequest(const Packet& packet,
                                  std::int64_t& out_timestamp);

  /// TryParseAck(L175-187)
  static bool TryParseAck(const Packet& packet, int& out_frame,
                          std::uint8_t& out_count);

  /// TryParseOrderPacket(L189-211):packet[4] 非 Disconnect/SyncHash 即订单包
  static bool TryParseOrderPacket(const std::vector<std::uint8_t>& packet,
                                  int& out_frame, OrderPacket& out_orders);
};

}  // namespace ora::net
