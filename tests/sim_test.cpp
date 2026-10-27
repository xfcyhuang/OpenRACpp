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
#include <cstdio>

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

  void Tick(ora::sim::Actor& ) override { ++int4_tick_count; }

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

  void ResolveOrder(ora::sim::Actor& ,
                    const ora::net::Order& ) override {
    ++int4_orders_resolved;
  }

  void Tick(ora::sim::Actor& ) override { ++int4_tick2_count; }

  bool b_created = false;
  int initial_token = ora::sim::Actor::InvalidConditionToken;
  int int4_notify_count = 0;
  int int4_last_chaos = -100;
  int int4_orders_resolved = 0;
  int int4_tick2_count = 0;
};

void TestSyncHashPrimitives() {
  using namespace ora;
  using ora::sim::sync::CombineSyncHash;

  CheckEq(sim::sync::HashInt2(int2{3, 4}), ((3 * 5) ^ (4 * 3)) / 4,
          "HashInt2(3,4) formula");
  CheckEq(sim::sync::HashInt2(int2{7, 2}), ((7 * 5) ^ (2 * 3)) / 4,
          "HashInt2(7,2) formula");

  CheckEq(sim::sync::HashCPos(CPos{4, 5}), CPos{4, 5}.Bits,
          "HashCPos == Bits");

  CheckEq(sim::sync::HashCVec(CVec{4, 5}), ((4 * 5) ^ (5 * 3)) / 4,
          "HashCVec formula");

  CheckEq(sim::sync::HashWDist(WDist{1024}), 1024, "HashWDist");
  CheckEq(sim::sync::HashWAngle(WAngle{256}), 256, "HashWAngle");
  CheckEq(sim::sync::HashWPos(WPos{1, 2, 3}), 1 ^ 2 ^ 3, "HashWPos");
  CheckEq(sim::sync::HashWVec(WVec{10, 20, 30}), (10 ^ 20) ^ 30, "HashWVec");

  const WRot rot{WAngle{100}, WAngle{200}, WAngle{300}};
  CheckEq(sim::sync::HashWRot(rot), 100 ^ 200 ^ 300, "HashWRot");

  CheckEq(sim::sync::HashActor(nullptr), 0, "HashActor(null)==0");

  CheckEq(sim::sync::HashBool(true), 1, "HashBool(true)==1");
  CheckEq(sim::sync::HashBool(false), 0, "HashBool(false)==0");

  CheckEq(CombineSyncHash(CombineSyncHash(0, 5), 3), 5 ^ 3,
          "CombineSyncHash XOR chain");
}

void TestOrderSerialization() {
  using namespace ora::net;
  using ord = std::vector<std::uint8_t>;

  {
    Order o{"Attack", nullptr, false};
    const auto bytes = o.Serialize();
    const ord expected = {0xFF, 6, 'A', 't', 't', 'a', 'c', 'k',
                          0x00, 0x00};
    Check(bytes == expected, "minimal Fields order bytes");
  }

  {
    Order o{"Move", nullptr, true};
    o.str_target_string = std::string("waypoint");
    o.uint4_extra_data = 0x11223344;
    o.target = ora::sim::Target::FromPos(ora::WPos{1, 2, 3});
    const auto bytes = o.Serialize();

    const std::int16_t flags = 0x01 | 0x04 | 0x08 | 0x20;
    ord expected = {0xFF, 4, 'M', 'o', 'v', 'e'};
    const auto lo = static_cast<std::uint8_t>(flags & 0xFF);
    const auto hi = static_cast<std::uint8_t>((flags >> 8) & 0xFF);
    expected.push_back(lo);
    expected.push_back(hi);

    expected.push_back(2);
    for (auto v : {1, 2, 3})
      for (int b = 0; b < 4; ++b)
        expected.push_back(static_cast<std::uint8_t>((v >> (b * 8)) & 0xFF));
    expected.push_back(0xFF);
    expected.push_back(0xFF);

    expected.push_back(8);
    for (char c : std::string_view("waypoint"))
      expected.push_back(static_cast<std::uint8_t>(c));

    for (int b = 0; b < 4; ++b)
      expected.push_back(static_cast<std::uint8_t>((0x11223344 >> (b * 8)) & 0xFF));
    Check(bytes == expected, "full-fields Terrain order bytes");
  }

  {
    Order o = Order::FromTargetString("Handshake", "payload", true);
    o.type = OrderType::Handshake;
    const auto bytes = o.Serialize();
    const ord expected = {0xFE, 9,  'H', 'a', 'n', 'd', 's', 'h', 'a',
                          'k',  'e', 7,   'p', 'a', 'y', 'l', 'o', 'a', 'd'};
    Check(bytes == expected, "handshake order bytes");
  }

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

void TestOrderIO() {
  using namespace ora::net;

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

  {
    const auto bytes = OrderIO::SerializePingResponse(1234567890123, 7);
    CheckEq(static_cast<long long>(bytes.size()), 14, "ping length");
    CheckEq(bytes[4], 0x20, "ping magic 0x20");
  }

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

  {
    Packet p{0, {10, 0, 0, 0, static_cast<std::uint8_t>(0xBF), 3, 0, 0, 0}};
    DisconnectData d{};
    Check(OrderIO::TryParseDisconnect(p, d), "TryParseDisconnect ok");
    CheckEq(d.frame, 10, "disconnect frame");
    CheckEq(d.client_id, 3, "disconnect client");
  }
}

void RunLockstep(ora::net::OrderManager& om, ora::sim::World& world,
                 int int4_ticks, ora::net::OrderManager::ClockFn clock) {
  for (int i = 0; i < int4_ticks; ++i) {

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

  RunLockstep(om, world, 100, nullptr);

  CheckEq(om.NetFrameNumber(), 35, "100 lockstep ticks advance NetFrame (throttled)");
  Check(!om.IsOutOfSync(), "no desync over 100 ticks");

  om.IssueOrder(Order::Command("/test"));
  om.IssueOrder(Order{"Move", nullptr, false});
  RunLockstep(om, world, 10, nullptr);
  Check(!om.IsOutOfSync(), "no desync with orders");
  CheckEq(om.NetFrameNumber(), 38, "order ticks advance (3 more net frames)");
}

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

  for (long long i = 0; i < 1'000'000; ++i) {
    om.TickImmediate();
    if ((i & 1023) == 0)
      om.IssueOrder(Order::Chat("tick", static_cast<std::uint32_t>(i)));
    if (om.TryTick())
      world.Tick();
  }

  CheckEq(om.NetFrameNumber(), 333'335, "10^6 ticks advanced (throttled)");
  Check(!om.IsOutOfSync(), "no desync over 10^6 ticks");
}

void TestWorldSim() {
  using namespace ora::sim;

  World world(WorldSimParams{.int4_random_seed = 1, .int4_timestep = 40});

  world.SetTraitFactory([&world](const std::string& str_name,
                                 ActorInitializer&)
                            -> std::vector<TraitBase*> {
    if (str_name != "stub")
      return {};
    std::vector<TraitBase*> out;
    out.push_back(world.Arena().Create<StubTickTrait>());
    out.push_back(world.Arena().Create<StubNotifyTrait>());
    return out;
  });

  bool threw = false;
  try {
    TypeDictionary dict;
    world.CreateActor("nonexistent", dict);
  } catch (const std::runtime_error& e) {
    threw = std::string_view(e.what()) ==
            "No rules definition for unit nonexistent";
  }
  Check(threw, "No rules definition error text verbatim");

  TypeDictionary dict;
  Actor* actor = world.CreateActor("stub", dict);
  CheckEq(static_cast<long long>(world.Actors().size()), 1,
          "one actor in world");

  Check(actor != nullptr, "actor created");

  CheckEq(static_cast<long long>(actor->ActorID()), 1, "actor id 1");
  Check(actor->IsInWorld(), "actor IsInWorld");

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

  CheckEq(static_cast<long long>(actor->SyncHashes().size()), 1,
          "one ISync hash entry");
  ora::sim::RegisterSyncHashFunction(
      "(test-stub)", [](const ISync*) -> int { return 0xBEEF; });

  Check(FindSyncHashFunctionByName("(test-stub)") != nullptr,
        "sync hash fn registry by name");

  auto* notify = actor->TraitOrDefault<StubNotifyTrait>();
  Check(notify != nullptr, "StubNotifyTrait registered");
  Check(notify->b_created, "INotifyCreated.Created fired");
  Check(notify->initial_token != Actor::InvalidConditionToken,
        "grant inside Created returns token");

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

  CheckEq(actor->GrantCondition(""), Actor::InvalidConditionToken,
          "empty condition grants invalid token");

  world.Tick();
  CheckEq(stub->int4_tick_count, 1, "world tick drove ITick trait");
  Check(notify->int4_tick2_count == 1, "both ITick traits ticked");

  struct CountingActivity final : Activity {
    int int4_runs = 0;
    bool b_first_run = false;
    bool b_last_run = false;
    bool Tick(Actor&) override {
      ++int4_runs;
      return int4_runs >= 3;
    }
    void OnFirstRun(Actor&) override { b_first_run = true; }
    void OnLastRun(Actor&) override { b_last_run = true; }
  };

  auto* act = new CountingActivity();
  actor->QueueActivity(act);
  CheckEq(static_cast<long long>(act->State()), static_cast<long long>(ActivityState::Queued),
          "activity queued");
  world.Tick();
  CheckEq(static_cast<long long>(act->State()), static_cast<long long>(ActivityState::Active),
          "activity active");
  Check(act->b_first_run, "OnFirstRun fired");
  world.Tick();
  world.Tick();
  CheckEq(static_cast<long long>(act->State()), static_cast<long long>(ActivityState::Done),
          "activity done after 3 ticks");
  Check(act->b_last_run, "OnLastRun fired");
  Check(actor->IsIdle(), "actor idle after activity done");

  {
    World w2(WorldSimParams{});
    TypeDictionary d2;
    auto* a2 = w2.CreateActor("", d2);
    bool queue_threw = false;
    try {

      a2->QueueActivity(new CountingActivity());
    } catch (const std::runtime_error&) {
      queue_threw = true;
    }
    Check(!queue_threw, "QueueActivity after Initialize ok");
  }

  actor->Dispose();
  Check(actor->WillDispose(), "WillDispose set (L426)");
  world.Tick();
  Check(actor->Disposed(), "actor disposed at frame end");
  CheckEq(static_cast<long long>(world.Actors().size()), 0,
          "world empty after dispose");
  CheckEq(static_cast<long long>(world.ActorsWithTrait<ITick>().size()), 0,
          "trait dict purged after dispose");

  int counter = 0;
  world.Add(std::make_unique<DelayedAction>(2, [&counter] { ++counter; }));
  CheckEq(static_cast<long long>(world.Effects().size()), 1, "effect added");
  world.Tick();
  CheckEq(counter, 0, "delay not yet elapsed");
  world.Tick();
  CheckEq(counter, 1, "delayed action fired");
  CheckEq(static_cast<long long>(world.Effects().size()), 0,
          "effect removed after fire");

  const int expected_empty = world.SharedRandom().Last;
  CheckEq(world.SyncHash(), expected_empty, "empty world SyncHash == RNG.Last");
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
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
