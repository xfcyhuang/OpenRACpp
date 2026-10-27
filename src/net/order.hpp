#pragma once
import std;

#include "net/byte_io.hpp"
#include "sim/target.hpp"

namespace ora::net {

class OrderManager;

enum class OrderType : std::uint8_t {
  Ack = 0x10,
  Ping = 0x20,
  SyncHash = 0x65,
  TickScale = 0x76,
  Disconnect = 0xBF,
  Handshake = 0xFE,
  Fields = 0xFF,
};

enum class OrderFields : std::int16_t {
  None = 0x0,
  Target = 0x01,
  ExtraActors = 0x02,
  TargetString = 0x04,
  Queued = 0x08,
  ExtraLocation = 0x10,
  ExtraData = 0x20,
  TargetIsCell = 0x40,
  Subject = 0x80,
  Grouped = 0x100,
};

inline OrderFields operator|(OrderFields a, OrderFields b) {
  return static_cast<OrderFields>(static_cast<std::int16_t>(a) |
                                  static_cast<std::int16_t>(b));
}
inline OrderFields& operator|=(OrderFields& a, OrderFields b) {
  a = a | b;
  return a;
}
inline bool HasField(OrderFields of, OrderFields f) {
  return (static_cast<std::int16_t>(of) & static_cast<std::int16_t>(f)) != 0;
}

struct Order {

  static constexpr int SyncHashOrderLength = 13;

  static constexpr int DisconnectOrderLength = 5;

  std::string str_order_string;
  sim::Actor* p_subject = nullptr;
  bool b_queued = false;
  sim::Target target;
  std::optional<std::vector<sim::Actor*>> vec_grouped_actors;

  std::optional<std::string> str_target_string;
  CPos extra_location;
  std::optional<std::vector<sim::Actor*>> vec_extra_actors;
  std::uint32_t uint4_extra_data = 0;
  bool b_is_immediate = false;
  OrderType type = OrderType::Fields;

  bool b_suppress_visual_feedback = false;
  sim::Target visual_feedback_target;

  sim::Player* Player() const;

  Order() = default;
  Order(std::string str_order, sim::Actor* subject, bool queued,
        std::optional<std::vector<sim::Actor*>> vec_extra_actors = std::nullopt,
        std::optional<std::vector<sim::Actor*>> vec_grouped_actors =
            std::nullopt);
  Order(std::string str_order, sim::Actor* subject, const sim::Target& target,
        bool queued,
        std::optional<std::vector<sim::Actor*>> vec_extra_actors =
            std::nullopt);
  Order(std::string str_order, sim::Actor* subject, const sim::Target& target,
        const sim::Target& target_visual_feedback, bool queued);

  static std::unique_ptr<Order> Deserialize(sim::World* world, ByteReader& r);

  std::vector<std::uint8_t> Serialize() const;

  static Order Chat(std::string_view text, std::uint32_t team_number = 0);
  static Order FromTargetString(std::string_view order,
                                std::string_view target_string,
                                bool is_immediate);
  static Order FromTargetString(std::string_view order,
                                std::string_view target_string,
                                bool is_immediate, std::uint32_t extra_data);
  static Order FromGroupedOrder(const Order& grouped, sim::Actor* subject);
  static Order Command(std::string_view text);
  static Order StartProduction(sim::Actor* subject, std::string_view item,
                               int count, bool queued = true);
  static Order PauseProduction(sim::Actor* subject, std::string_view item,
                               bool pause);
  static Order CancelProduction(sim::Actor* subject, std::string_view item,
                                int count);
};

}
