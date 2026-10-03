// UPSTREAM: 仿真核心单测(PORTING_PLAN §Phase 3 验收:
//          TraitDictionary/Activity/条件系统/Order 字节级序列化/OrderIO/
//          EchoConnection+OrderManager 锁步 10⁶ tick ASan 无泄漏)
//          The sim-core unit tests (the PORTING_PLAN §Phase 3 acceptance:
//          TraitDictionary/Activity/condition system/byte-level Order
//          serialization/OrderIO/EchoConnection+OrderManager lockstep with
//          10⁶ ticks, ASan-clean).
//
// 断言期望值对照 / Expected values verified against:
//  - 哈希函数族:Sync.cs L109-164 公式手算(含 bool 的 IL 可达语义
//    (b?1:0)^0xAAA)
//    The hash family: hand-computed from Sync.cs L109-164 (including the
//    bool IL-reachable semantics (b?1:0)^0xAAA).
//  - Order 字节:Order.cs L345-487 序列化序逐字段人工推导(flags 位值/
//    7-bit 前缀/小端)
//    Order bytes: hand-derived field-by-field from the Order.cs L345-487
//    serialization order (flag bit values / 7-bit prefixes / little-endian).
//  - 锁步:OrderManager.cs L98-328 状态机(EchoConnection 帧投影 +1/
//    IsNetFrame 节流/ProcessOrders 帧号校验)
//    Lockstep: the OrderManager.cs L98-328 state machine (EchoConnection
//    frame projection +1 / IsNetFrame throttling / ProcessOrders frame
//    validation).
import std;

#include "net/connection.hpp"
#include "net/order_io.hpp"
#include "net/order_manager.hpp"
#include "net/unit_orders.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/effects.hpp"
#include "sim/target.hpp"
#include "sim/trait_dictionary.hpp"
#include "sim/world.hpp"

int int4_failures = 0;

void Check(bool b_cond, std::string_view sv_what) {
  if (!b_cond) {
    std::println("FAIL: {}", sv_what);
    int4_failures++;
  }
}

void CheckEq(long long a, long long b, std::string_view sv_what) {
  if (a != b) {
    std::println("FAIL: {} ({} != {})", sv_what, a, b);
    int4_failures++;
  }
}

// ———— 测试桩 trait(ORA_TRAIT_INTERFACES 机制验证:预留 TypeId 段 60000+)————
// ———— Test stub traits (exercising the ORA_TRAIT_INTERFACES mechanism:
//      reserved TypeId range 60000+) ————

class StubTickTrait final : public ora::sim::TraitBase,
                            public ora::sim::ITick,
                            public ora::sim::ISync {
 public:
  static constexpr ora::gen::TypeId kTypeId = ora::gen::TypeId(60001);
  ora::gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<StubTickTrait, ora::sim::ITick,
                                 ora::sim::ISync>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  void Tick(ora::sim::Actor& /*self*/) override { ++int4_tick_count; }

  // ISync:hp(手动哈希接线,Phase 5 走 gen/sync_gen 表)
  int int4_hp = 100;
  int int4_tick_count = 0;
};

class StubNotifyTrait final : public ora::sim::TraitBase,
                              public ora::sim::INotifyCreated,
                              public ora::sim::IObservesVariables,
                              public ora::sim::IResolveOrder,
                              public ora::sim::ITick {
 public:
  static constexpr ora::gen::TypeId kTypeId = ora::gen::TypeId(60002);
  ora::gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<StubNotifyTrait, ora::sim::INotifyCreated,
                                 ora::sim::IObservesVariables,
                                 ora::sim::IResolveOrder, ora::sim::ITick>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  void Created(ora::sim::Actor& self) override {
    b_created = true;
    initial_token = self.GrantCondition("startup");
  }

  std::vector<ora::sim::VariableObserver> GetVariableObservers() override {
    return {ora::sim::VariableObserver{
        [this](ora::sim::Actor&, ora::sim::ConditionCacheView vars) {
          ++int4_notify_count;
          auto it = vars.find("chaos");
          int4_last_chaos = it != vars.end() ? it->second : -1;
        },
        {"chaos"}}};
  }

  void ResolveOrder(ora::sim::Actor& /*self*/,
                    const ora::net::Order& /*order*/) override {
    ++int4_orders_resolved;
  }

  void Tick(ora::sim::Actor& /*self*/) override { ++int4_tick2_count; }

  bool b_created = false;
  int initial_token = ora::sim::Actor::InvalidConditionToken;
  int int4_notify_count = 0;
  int int4_last_chaos = -100;
  int int4_orders_resolved = 0;
  int int4_tick2_count = 0;
};

// ———— A. Sync 哈希函数族(Sync.cs L109-164 公式手算)————
// ———— A. The hash function family (hand-computed from Sync.cs L109-164) ————

void TestSyncHashPrimitives() {
  using namespace ora;
  using ora::sim::sync::CombineSyncHash;

  // HashInt2((3,4)) = ((3*5)^(4*3))/4 = (15^12)/4
  CheckEq(sim::sync::HashInt2(int2{3, 4}), ((3 * 5) ^ (4 * 3)) / 4,
          "HashInt2(3,4) formula");
  CheckEq(sim::sync::HashInt2(int2{7, 2}), ((7 * 5) ^ (2 * 3)) / 4,
          "HashInt2(7,2) formula");

  // HashCPos = Bits
  CheckEq(sim::sync::HashCPos(CPos{4, 5}), CPos{4, 5}.Bits,
          "HashCPos == Bits");

  // HashCVec 同 int2 公式
  CheckEq(sim::sync::HashCVec(CVec{4, 5}), ((4 * 5) ^ (5 * 3)) / 4,
          "HashCVec formula");

  // HashWDist/WAngle/WPos/WVec/WRot = XOR 组合(int.GetHashCode = 值)
  CheckEq(sim::sync::HashWDist(WDist{1024}), 1024, "HashWDist");
  CheckEq(sim::sync::HashWAngle(WAngle{256}), 256, "HashWAngle");
  CheckEq(sim::sync::HashWPos(WPos{1, 2, 3}), 1 ^ 2 ^ 3, "HashWPos");
  CheckEq(sim::sync::HashWVec(WVec{10, 20, 30}), (10 ^ 20) ^ 30, "HashWVec");

  // WRot.GetHashCode = Roll ^ Pitch ^ Yaw
  const WRot rot{WAngle{100}, WAngle{200}, WAngle{300}};
  CheckEq(sim::sync::HashWRot(rot), 100 ^ 200 ^ 300, "HashWRot");

  // HashActor:ActorID<<16(null → 0);HashPlayer:((ID<<16)*0x567) 回绕
  CheckEq(sim::sync::HashActor(nullptr), 0, "HashActor(null)==0");

  // bool 字段哈希:IL 可达语义 (b?1:0)^0xAAA(EmitSyncOpcodes L63-71;
  // 0x555 分支恒不可达)
  CheckEq(sim::sync::HashBool(true), 1 ^ 0xAAA, "HashBool(true)==0xAAB");
  CheckEq(sim::sync::HashBool(false), 0 ^ 0xAAA, "HashBool(false)==0xAAA");

  // 组合协议:0 XOR 链(GenerateHashFunc)
  CheckEq(CombineSyncHash(CombineSyncHash(0, 5), 3), 5 ^ 3,
          "CombineSyncHash XOR chain");
}

// ———— B. Order 字节级序列化(Order.cs L345-487 推导)————
// ———— B. Byte-level Order serialization ————

void TestOrderSerialization() {
  using namespace ora::net;
  using ord = std::vector<std::uint8_t>;

  // B1. 最小 Fields 包:仅 orderstring。字节 = [0xFF] + str 前缀+字节。
  //     str="Attack":前缀 6 + "Attack"
  {
    Order o{"Attack", nullptr, false};
    const auto bytes = o.Serialize();
    const ord expected = {0xFF, 6, 'A', 't', 't', 'a', 'c', 'k',
                          0x00, 0x00};  // flags=None(int16 LE)
    Check(bytes == expected, "minimal Fields order bytes");
  }

  // B2. 全字段组合:Subject(uint.MaxValue=null)/Target=Terrain pos 单位置
  //     (短路 -1)/TargetString/Queued/ExtraData
  {
    Order o{"Move", nullptr, true};
    o.str_target_string = std::string("waypoint");
    o.uint4_extra_data = 0x11223344;
    o.target = ora::sim::Target::FromPos(ora::WPos{1, 2, 3});
    const auto bytes = o.Serialize();
    // flags = Target(1)|TargetString(4)|Queued(8)|ExtraData(0x20) = 0x2D
    const std::int16_t flags = 0x01 | 0x04 | 0x08 | 0x20;
    ord expected = {0xFF, 4, 'M', 'o', 'v', 'e'};
    const auto lo = static_cast<std::uint8_t>(flags & 0xFF);
    const auto hi = static_cast<std::uint8_t>((flags >> 8) & 0xFF);
    expected.push_back(lo);
    expected.push_back(hi);
    // Target: type=Terrain(2) + pos(1,2,3 LE) + (short)-1 短路
    expected.push_back(2);
    for (auto v : {1, 2, 3})
      for (int b = 0; b < 4; ++b)
        expected.push_back(static_cast<std::uint8_t>((v >> (b * 8)) & 0xFF));
    expected.push_back(0xFF);
    expected.push_back(0xFF);  // (short)-1 LE
    // TargetString
    expected.push_back(8);
    for (char c : std::string_view("waypoint"))
      expected.push_back(static_cast<std::uint8_t>(c));
    // ExtraData
    for (int b = 0; b < 4; ++b)
      expected.push_back(static_cast<std::uint8_t>((0x11223344 >> (b * 8)) & 0xFF));
    Check(bytes == expected, "full-fields Terrain order bytes");
  }

  // B3. Handshake:0xFE + name + targetstring
  {
    Order o = Order::FromTargetString("Handshake", "payload", true);
    o.type = OrderType::Handshake;
    const auto bytes = o.Serialize();
    const ord expected = {0xFE, 9,  'H', 'a', 'n', 'd', 's', 'h', 'a',
                          'k',  'e', 7,   'p', 'a', 'y', 'l', 'o', 'a', 'd'};
    Check(bytes == expected, "handshake order bytes");
  }

  // B4. 往返:serialize → deserialize(world=null) → serialize 字节一致
  {
    std::vector<Order> cases;
    cases.push_back(Order{"Attack", nullptr, false});
    Order full{"Attack", nullptr, true};
    full.str_target_string = std::string("xyz");
    full.uint4_extra_data = 7;
    full.target = ora::sim::Target::FromPos(ora::WPos{-5, 100, 2048});
    cases.push_back(std::move(full));
    Order multi_pos{"Move", nullptr, false};
    multi_pos.target = ora::sim::Target::FromSerializedTerrainPosition(
        ora::WPos{0, 0, 0},
        std::vector<ora::WPos>{ora::WPos{0, 0, 0}, ora::WPos{9, 9, 9}});
    cases.push_back(std::move(multi_pos));
    cases.push_back(Order::Chat("hello", 2));
    cases.push_back(Order::Command("/help"));
    cases.push_back(Order::StartProduction(nullptr, "e1", 5));

    for (auto& o : cases) {
      const auto first = o.Serialize();
      ByteReader r(first);
      auto back = Order::Deserialize(nullptr, r);
      Check(back != nullptr, "deserialize roundtrip non-null");
      if (back) {
        const auto second = back->Serialize();
        Check(first == second,
              "roundtrip bytes identical: " + o.str_order_string);
      }
    }
  }
}

// ———— C. OrderIO 线协议(OrderIO.cs L83-211)————
// ———— C. The OrderIO wire protocol ————

void TestOrderIO() {
  using namespace ora::net;

  // SerializeSync:frame + 0x65 + hash + defeat = 17 字节
  {
    const auto bytes = OrderIO::SerializeSync(SyncPacketData{100, -55, 0x1});
    CheckEq(static_cast<long long>(bytes.size()), 4 + Order::SyncHashOrderLength,
            "SerializeSync length");
    CheckEq(bytes[4], 0x65, "SerializeSync magic 0x65");

    SyncPacketData parsed{};
    Check(OrderIO::TryParseSync(bytes, parsed), "TryParseSync ok");
    CheckEq(parsed.frame, 100, "TryParseSync frame");
    CheckEq(parsed.sync_hash, -55, "TryParseSync hash");
    CheckEq(static_cast<long long>(parsed.uint8_defeat_state), 1,
            "TryParseSync defeat");
  }

  // SerializePingResponse:0 + 0x20 + int64 + byte = 14 字节
  {
    const auto bytes = OrderIO::SerializePingResponse(1234567890123, 7);
    CheckEq(static_cast<long long>(bytes.size()), 14, "ping length");
    CheckEq(bytes[4], 0x20, "ping magic 0x20");
  }

  // TryParseAck:6 字节 + 0x10;仅 FromClient==0
  {
    Packet p{0, {0x01, 0x00, 0x00, 0x00, 0x10, 0x03}};
    int frame = 0;
    std::uint8_t count = 0;
    Check(OrderIO::TryParseAck(p, frame, count), "TryParseAck ok");
    CheckEq(frame, 1, "ack frame");
    CheckEq(count, 3, "ack count");
    p.from_client = 5;
    Check(!OrderIO::TryParseAck(p, frame, count), "ack server-only");
  }

  // TryParseOrderPacket:空包(frame only)与载荷包
  {
    int frame = -1;
    OrderPacket orders;
    Check(OrderIO::TryParseOrderPacket({9, 0, 0, 0}, frame, orders),
          "empty packet parse");
    CheckEq(frame, 9, "empty packet frame");
    Check(orders.IsEmpty(), "empty packet no orders");

    std::vector<std::uint8_t> with_orders = {2, 0, 0, 0, 0xFF, 4, 'T', 'e', 's', 't', 0, 0};
    Check(OrderIO::TryParseOrderPacket(with_orders, frame, orders),
          "order packet parse");
    CheckEq(frame, 2, "order packet frame");
    auto parsed = orders.GetOrders(nullptr);
    CheckEq(static_cast<long long>(parsed.size()), 1, "one order parsed");
    Check(parsed[0] && parsed[0]->str_order_string == "Test",
          "order string roundtrip");
  }

  // TryParseDisconnect:9 字节 + 0xBF;server-only
  {
    Packet p{0, {10, 0, 0, 0, static_cast<std::uint8_t>(0xBF), 3, 0, 0, 0}};
    DisconnectData d{};
    Check(OrderIO::TryParseDisconnect(p, d), "TryParseDisconnect ok");
    CheckEq(d.frame, 10, "disconnect frame");
    CheckEq(d.client_id, 3, "disconnect client");
  }
}

// ———— D. EchoConnection + OrderManager 锁步(OrderManager.cs L98-328)————
// ———— D. The EchoConnection + OrderManager lockstep ————

void RunLockstep(ora::net::OrderManager& om, ora::sim::World& world,
                 int int4_ticks, ora::net::OrderManager::ClockFn clock) {
  for (int i = 0; i < int4_ticks; ++i) {
    // Game.InnerLogicTick 序(Game.cs L640-660):TickImmediate →
    // TryTick → world.Tick
    om.TickImmediate();
    if (om.TryTick())
      world.Tick();
    if (clock)
      (void)clock();
  }
}

void TestLockstep() {
  using namespace ora::net;
  using namespace ora::sim;

  auto echo = std::make_unique<EchoConnection>();
  OrderManager om(std::move(echo));
  om.LobbyInfo().vec_clients.push_back(SessionClient{1, "local"});
  om.LobbyInfo().global_settings.NetFrameInterval = 3;

  World world(WorldSimParams{.int4_random_seed = 42, .int4_timestep = 40});
  om.SetWorld(&world);
  world.SetOrderManager(&om);
  om.SetOrderProcessor(
      [](OrderManager& m, ora::sim::World* w, int client, const Order& o) {
        ProcessOrder(m, w, client, o);
      });

  om.StartGame();
  Check(om.GameStarted(), "GameStarted after StartGame");
  CheckEq(om.NetFrameNumber(), 1, "NetFrameNumber == 1 (L111)");
  CheckEq(om.LocalFrameNumber(), 0, "LocalFrameNumber == 0 (L112)");

  // 锁步推进(EchoConnection 空帧注入 + 投影 +1)
  RunLockstep(om, world, 100, nullptr);
  // NetFrameInterval=3:LocalFrame 0,3,...,99 共 34 次 IsNetFrame →
  // NetFrame = 1 + 34 = 35(TryTick L300-328 的节流语义)
  CheckEq(om.NetFrameNumber(), 35, "100 lockstep ticks advance NetFrame (throttled)");
  Check(!om.IsOutOfSync(), "no desync over 100 ticks");

  // 带订单:立即单 + 排队单
  om.IssueOrder(Order::Command("/test"));
  om.IssueOrder(Order{"Move", nullptr, false});
  RunLockstep(om, world, 10, nullptr);
  Check(!om.IsOutOfSync(), "no desync with orders");
  CheckEq(om.NetFrameNumber(), 38, "order ticks advance (3 more net frames)");
}

// ———— E. EchoConnection 10⁶ tick(Phase 3 验收:ASan 无泄漏)————
// ———— E. EchoConnection 10⁶ ticks (Phase 3 acceptance: ASan-clean) ————

void TestLongRun() {
  using namespace ora::net;
  using namespace ora::sim;

  auto echo = std::make_unique<EchoConnection>();
  OrderManager om(std::move(echo));
  om.LobbyInfo().vec_clients.push_back(SessionClient{1, "local"});
  om.LobbyInfo().global_settings.NetFrameInterval = 3;

  World world(WorldSimParams{.int4_random_seed = 7, .int4_timestep = 40});
  om.SetWorld(&world);
  world.SetOrderManager(&om);
  om.SetOrderProcessor(
      [](OrderManager& m, ora::sim::World* w, int client, const Order& o) {
        ProcessOrder(m, w, client, o);
      });
  om.StartGame();

  // 每 1024 tick 发一条轻量订单(聊天路径),覆盖序列化/反序列化热路径
  for (long long i = 0; i < 1'000'000; ++i) {
    om.TickImmediate();
    if ((i & 1023) == 0)
      om.IssueOrder(Order::Chat("tick", static_cast<std::uint32_t>(i)));
    if (om.TryTick())
      world.Tick();
  }
  // 10^6 迭代 → floor(999999/3)+1 = 333334 次 IsNetFrame → NetFrame = 333335
  CheckEq(om.NetFrameNumber(), 333'335, "10^6 ticks advanced (throttled)");
  Check(!om.IsOutOfSync(), "no desync over 10^6 ticks");
}

// ———— F. World/Actor/条件系统/TraitDictionary/Activity ————
// ———— F. World/Actor/condition system/TraitDictionary/Activity ————

void TestWorldSim() {
  using namespace ora::sim;

  World world(WorldSimParams{.int4_random_seed = 1, .int4_timestep = 40});

  // trait 工厂:为 "stub" actor 生成两个桩 trait
  world.SetTraitFactory([](const std::string& str_name)
                            -> std::vector<std::unique_ptr<TraitBase>> {
    if (str_name != "stub")
      return {};  // → "No rules definition for unit X"
    std::vector<std::unique_ptr<TraitBase>> out;
    out.push_back(std::make_unique<StubTickTrait>());
    out.push_back(std::make_unique<StubNotifyTrait>());
    return out;
  });

  // F1. 未知规则异常文本逐字
  bool threw = false;
  try {
    TypeDictionary dict;
    world.CreateActor("nonexistent", dict);
  } catch (const std::runtime_error& e) {
    threw = std::string_view(e.what()) ==
            "No rules definition for unit nonexistent";
  }
  Check(threw, "No rules definition error text verbatim");

  // F2. 创建 + trait 接线
  TypeDictionary dict;
  Actor* actor = world.CreateActor("stub", dict);
  CheckEq(static_cast<long long>(world.Actors().size()), 1,
          "one actor in world");

  Check(actor != nullptr, "actor created");
  // F1 的失败构造已消耗 ID 0(上游同序:NextAID 先于规则检查)
  // The F1 failed construction already consumed id 0 (upstream order:
  // NextAID runs before the rules check).
  CheckEq(static_cast<long long>(actor->ActorID()), 1, "actor id 1");
  Check(actor->IsInWorld(), "actor IsInWorld");

  // 两桩均实现 ITick —— GetOrDefault<ITick> 多实例异常(上游 L177-178 语义)
  bool multi_threw = false;
  try {
    (void)actor->TraitOrDefault<ITick>();
  } catch (const std::runtime_error& e) {
    multi_threw = std::string_view(e.what()).find("has multiple traits") !=
                  std::string_view::npos;
  }
  Check(multi_threw, "GetOrDefault<ITick> multiple-trait exception");

  auto* stub = actor->TraitOrDefault<StubTickTrait>();
  Check(stub != nullptr, "StubTickTrait via concrete-type query");
  CheckEq(static_cast<long long>(world.ActorsWithTrait<ITick>().size()), 2,
          "ActorsWithTrait<ITick> = All(): one pair per trait entry");
  CheckEq(static_cast<long long>(world.ActorsHavingTrait<ITick>().size()), 1,
          "ActorsHavingTrait<ITick> dedups to 1 actor");
  CheckEq(
      static_cast<long long>(actor->TraitsImplementing<ITick>().size()), 2,
      "WithInterface<ITick> returns both (registration order)");

  // F3. ISync 哈希收集(注册表接线)
  CheckEq(static_cast<long long>(actor->SyncHashes().size()), 1,
          "one ISync hash entry");
  ora::sim::RegisterSyncHashFunction(
      "(test-stub)", [](const ISync*) -> int { return 0xBEEF; });
  // 桩 TypeId(60001)不在 gen 表 → FindSyncHashFunction nullptr → 哈希 0;
  // 直接验证协议:注册后经全名查找生效
  Check(FindSyncHashFunctionByName("(test-stub)") != nullptr,
        "sync hash fn registry by name");

  // F4. 条件系统(Actor.cs L558-615)
  auto* notify = actor->TraitOrDefault<StubNotifyTrait>();
  Check(notify != nullptr, "StubNotifyTrait registered");
  Check(notify->b_created, "INotifyCreated.Created fired");
  Check(notify->initial_token != Actor::InvalidConditionToken,
        "grant inside Created returns token");

  // Created 内授权的条件在观察者注册时计数就位(L234 注释)
  auto it = actor->ConditionCache().find("startup");
  Check(it != actor->ConditionCache().end() && it->second == 1,
        "startup condition cached count 1");

  const int notify_count_before = notify->int4_notify_count;
  const int t2 = actor->GrantCondition("chaos");
  const int t3 = actor->GrantCondition("chaos");
  Check(t2 != t3 && t2 != Actor::InvalidConditionToken,
        "tokens unique per grant");
  CheckEq(notify->int4_last_chaos, 2, "chaos count 2 after two grants");
  Check(notify->int4_notify_count == notify_count_before + 2,
        "notifier called once per grant");

  CheckEq(actor->RevokeCondition(t2), Actor::InvalidConditionToken,
          "RevokeCondition returns invalid token (L606)");
  CheckEq(notify->int4_last_chaos, 1, "chaos count 1 after revoke");

  Check(actor->TokenValid(t3), "t3 still valid");
  Check(!actor->TokenValid(t2), "t2 invalid after revoke");

  bool revoke_threw = false;
  try {
    actor->RevokeCondition(t2);
  } catch (const std::runtime_error&) {
    revoke_threw = true;
  }
  Check(revoke_threw, "double revoke throws (L602)");

  // 空条件名 → InvalidConditionToken
  CheckEq(actor->GrantCondition(""), Actor::InvalidConditionToken,
          "empty condition grants invalid token");

  // F5. World.Tick 顺序:actor.Activity → ITick traits → effects → 帧末
  world.Tick();
  CheckEq(stub->int4_tick_count, 1, "world tick drove ITick trait");
  Check(notify->int4_tick2_count == 1, "both ITick traits ticked");

  // F6. Activity 状态机(Activity.cs L95-140)
  struct CountingActivity final : Activity {
    int int4_runs = 0;
    bool b_first_run = false;
    bool b_last_run = false;
    bool Tick(Actor&) override {
      ++int4_runs;
      return int4_runs >= 3;  // 3 tick 后完成
    }
    void OnFirstRun(Actor&) override { b_first_run = true; }
    void OnLastRun(Actor&) override { b_last_run = true; }
  };

  auto* act = new CountingActivity();
  actor->QueueActivity(act);
  CheckEq(static_cast<long long>(act->State()), static_cast<long long>(ActivityState::Queued),
          "activity queued");
  world.Tick();  // run 1
  CheckEq(static_cast<long long>(act->State()), static_cast<long long>(ActivityState::Active),
          "activity active");
  Check(act->b_first_run, "OnFirstRun fired");
  world.Tick();  // run 2
  world.Tick();  // run 3 → done
  CheckEq(static_cast<long long>(act->State()), static_cast<long long>(ActivityState::Done),
          "activity done after 3 ticks");
  Check(act->b_last_run, "OnLastRun fired");
  Check(actor->IsIdle(), "actor idle after activity done");

  // F7. QueueActivity 在 created 前(空 name actor 不受工厂影响,直接构造)
  {
    World w2(WorldSimParams{});
    TypeDictionary d2;
    auto* a2 = w2.CreateActor("", d2);
    bool queue_threw = false;
    try {
      // 手动置 created=false 的路径不可达(CreateActor 立即 Initialize);
      // 改验 Initialize 已发生:QueueActivity 应当成功
      a2->QueueActivity(new CountingActivity());
    } catch (const std::runtime_error&) {
      queue_threw = true;
    }
    Check(!queue_threw, "QueueActivity after Initialize ok");
  }

  // F8. Actor.Dispose → 帧末摘除(TraitDictionary/世界表)
  actor->Dispose();
  Check(actor->WillDispose(), "WillDispose set (L426)");
  world.Tick();  // 帧末任务执行
  Check(actor->Disposed(), "actor disposed at frame end");
  CheckEq(static_cast<long long>(world.Actors().size()), 0,
          "world empty after dispose");
  CheckEq(static_cast<long long>(world.ActorsWithTrait<ITick>().size()), 0,
          "trait dict purged after dispose");

  // F9. DelayedAction(DelayedAction.cs L29-33)
  int counter = 0;
  world.Add(std::make_unique<DelayedAction>(2, [&counter] { ++counter; }));
  CheckEq(static_cast<long long>(world.Effects().size()), 1, "effect added");
  world.Tick();  // delay 2→1
  CheckEq(counter, 0, "delay not yet elapsed");
  world.Tick();  // delay 1→0:帧末触发
  CheckEq(counter, 1, "delayed action fired");
  CheckEq(static_cast<long long>(world.Effects().size()), 0,
          "effect removed after fire");

  // F10. SyncHash 组合公式(World.cs L482-512):空世界 = SharedRandom.Last
  const int expected_empty = world.SharedRandom().Last;
  CheckEq(world.SyncHash(), expected_empty, "empty world SyncHash == RNG.Last");
}

int main() {
  TestSyncHashPrimitives();
  TestOrderSerialization();
  TestOrderIO();
  TestLockstep();
  TestWorldSim();
  TestLongRun();

  if (int4_failures == 0) {
    std::println("sim_test: all passed");
    return 0;
  }
  std::println("sim_test: {} FAILURES", int4_failures);
  return 1;
}
