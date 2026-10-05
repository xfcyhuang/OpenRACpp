// UPSTREAM: OpenRA.Game/Network/Order.cs @b6fc03f L18-495(逐字节序列化协议
//          逐语义重写;BinaryWriter/Reader → byte_io 兼容层)
//          Verbatim-bytes rewrite of the serialization protocol via the
//          byte_io shim.
//
// 机制对照 / Mechanism mapping:
//  - C# null vs 空容器的 flags 位差(Subject/TargetString/ExtraActors/
//    Grouped/ExtraData!=0/ExtraLocation!=Zero)→ std::optional / 显式判定,
//    位掩码值与写入序逐字节一致(Order.cs L378-479)
//    The C# null-vs-empty flags distinction → std::optional / explicit
//    checks; bit values and write order are byte-identical (Order.cs
//    L378-479).
//  - Target 序列化:Actor(actorID+generation)/Terrain(cell 或 pos+位置数,
//    单位置短路 -1)/FrozenActor(Viewer.PlayerActor.ActorID+ID);反序列化经
//    当前世界解析(GetActorById;失败 → Invalid/null 语义保留)
//    Target serialization: Actor (actorID+generation) / Terrain (cell, or
//    pos+count with the single-position -1 shortcut) / FrozenActor
//    (Viewer.PlayerActor.ActorID+ID); deserialization resolves through the
//    live world (GetActorById; failures degrade to the upstream
//    Invalid/null semantics).
//  - Deserialize 的异常吞并(L235-245:catch → null)→ C++ try/catch(std::
//    exception)等价;TextNotificationsManager 通知 Phase 6 接线
//    Deserialize's exception swallowing (L235-245: catch → null) → the
//    equivalent C++ try/catch(std::exception); TextNotificationsManager
//    notifications wire up in Phase 6.
//  - FromCell 分支的 CenterOfSubCell(Map 依赖 Phase 5):经 World 注入的
//    子格中心换算器,未注入时按 square 网格公式(x*1024+512)桩换算 ——
//    COVERAGE 登记
//    The FromCell branch's CenterOfSubCell (Map dependency, Phase 5) goes
//    through a World-injected sub-cell center converter, falling back to
//    the square-grid formula (x*1024+512) — registered in COVERAGE.
#pragma once
import std;

#include "net/byte_io.hpp"
#include "sim/target.hpp"

namespace ora::net {

class OrderManager;

/// Order.cs L18-27
enum class OrderType : std::uint8_t {
  Ack = 0x10,
  Ping = 0x20,
  SyncHash = 0x65,
  TickScale = 0x76,
  Disconnect = 0xBF,
  Handshake = 0xFE,
  Fields = 0xFF,
};

/// Order.cs L30-42(internal flags)
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

/// Order(Order.cs L52-495)
struct Order {
  /// Order.cs L55
  static constexpr int SyncHashOrderLength = 13;
  /// Order.cs L58
  static constexpr int DisconnectOrderLength = 5;

  std::string str_order_string;
  sim::Actor* p_subject = nullptr;
  bool b_queued = false;
  sim::Target target;
  std::optional<std::vector<sim::Actor*>> vec_grouped_actors;

  std::optional<std::string> str_target_string;
  CPos extra_location;                        // 默认 CPos.Zero
  std::optional<std::vector<sim::Actor*>> vec_extra_actors;
  std::uint32_t uint4_extra_data = 0;
  bool b_is_immediate = false;
  OrderType type = OrderType::Fields;

  bool b_suppress_visual_feedback = false;
  sim::Target visual_feedback_target;

  sim::Player* Player() const;  // 实现于 order.cpp(需 Actor 完整类型)

  // ———— 构造族(Order.cs L81-93/330-343;optional = C# null 位)————
  // ———— The constructor family (Order.cs L81-93/330-343; optional =
  //      the C# null slot) ————
  Order() = default;
  Order(std::string str_order, sim::Actor* subject, bool queued,
        std::optional<std::vector<sim::Actor*>> vec_extra_actors = std::nullopt,
        std::optional<std::vector<sim::Actor*>> vec_grouped_actors =
            std::nullopt);
  Order(std::string str_order, sim::Actor* subject, const sim::Target& target,
        bool queued,
        std::optional<std::vector<sim::Actor*>> vec_extra_actors =
            std::nullopt);

  // ———— 反序列化(Order.cs L95-246;world 可为 null —— 服务器/replay 过滤路径)————
  static std::unique_ptr<Order> Deserialize(sim::World* world, ByteReader& r);

  // ———— 序列化(Order.cs L345-487;逐字节)————
  std::vector<std::uint8_t> Serialize() const;

  // ———— 命名构造(Order.cs L281-327)————
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

}  // namespace ora::net
