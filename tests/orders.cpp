import std;

#include "game/network/order.hpp"
#include "game/network/order_manager.hpp"
#include "game/network/order_packet.hpp"
#include "game/world/world.hpp"

namespace {

int g_failed{};
int g_total{};
constexpr int kMaxReports{20};

void check(bool b_ok, std::string_view str_what) {
  ++g_total;
  if (b_ok) return;
  ++g_failed;
  if (g_failed <= kMaxReports) std::println("FAIL: {}", str_what);
}

std::string to_hex(std::span<const std::uint8_t> span_bytes) {
  std::string str_out;
  str_out.reserve(span_bytes.size() * 2);
  for (const std::uint8_t u1_byte : span_bytes)
    str_out.append(std::format("{:02x}", u1_byte));
  return str_out;
}

bool roundtrip_identity(const ora::game::network::OrderModel& order, std::string_view str_name) {
  using namespace ora::game::network;
  const std::vector<std::uint8_t> vec_first{serialize_order(order)};
  OrderReader reader{vec_first};
  const std::optional<OrderModel> opt_parsed{deserialize_order(reader, nullptr)};
  if (!opt_parsed.has_value()) {
    check(false, std::format("{}: deserialize returned nullopt", str_name));
    return false;
  }
  check(reader.idx_pos == vec_first.size(),
        std::format("{}: consumed {} of {} bytes", str_name, reader.idx_pos, vec_first.size()));
  const std::vector<std::uint8_t> vec_second{serialize_order(*opt_parsed)};
  const bool b_equal{vec_first == vec_second};
  check(b_equal, std::format("{}: roundtrip bytes differ\n  first  {}\n  second {}",
                             str_name, to_hex(vec_first), to_hex(vec_second)));
  return b_equal;
}

void check_hex(const ora::game::network::OrderModel& order, std::string_view str_expect_hex,
               std::string_view str_name) {
  const std::string str_actual{to_hex(ora::game::network::serialize_order(order))};
  check(str_actual == str_expect_hex,
        std::format("{}: hex mismatch\n  expect {}\n  actual {}", str_name, str_expect_hex,
                    str_actual));
}

void test_roundtrip_tier_a() {
  using namespace ora::game::network;

  {
    const OrderModel order{};
    check_hex(order, "ff000000", "empty hex");
    roundtrip_identity(order, "empty");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    check_hex(order, "ff04546573740000", "unqueued hex");
    roundtrip_identity(order, "unqueued");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.b_queued = true;
    check_hex(order, "ff04546573740800", "queued hex");
    roundtrip_identity(order, "queued");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.target.kind = TargetKind::kTerrain;
    order.target.int4_pos_x = std::numeric_limits<std::int32_t>::min();
    order.target.int4_pos_z = std::numeric_limits<std::int32_t>::max();
    order.target.vec_terrain_positions.push_back(
        {order.target.int4_pos_x, order.target.int4_pos_y, order.target.int4_pos_z});
    check_hex(order, "ff04546573740100020000008000000000ffffff7fffff", "pos-target hex");
    roundtrip_identity(order, "pos-target");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    roundtrip_identity(order, "invalid-target");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.b_queued = true;
    order.opt_str_target = "TargetString";
    order.int4_extra_location_bits = 0x7ff7ff80;
    order.u4_extra_data = 0xffffffffu;
    check_hex(order, "ff04546573743c000c546172676574537472696e6780fff77fffffffff",
              "extra-fields hex");
    roundtrip_identity(order, "extra-fields");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.opt_str_target = "";
    check_hex(order, "ff0454657374040000", "target-string-empty hex");
    roundtrip_identity(order, "target-string-empty");
  }

  {
    OrderModel order;
    order.str_order = "Тест-日本語";
    order.opt_str_target = "τ=2π";
    roundtrip_identity(order, "utf8-strings");
  }

  {
    // 多字节 7-bit varint 长度（>=128）的 TargetString：LEB128 小端序读写对称
    // （T16 SyncInfo Session yaml 实证：read 端移位方向反了会解析失败/错长）
    OrderModel order;
    order.str_order = "SyncInfo";
    order.opt_str_target = std::string(235, 'x');  // 235 = 0xEB → varint 两字节 eb 01
    const std::vector<std::uint8_t> vec_first{serialize_order(order)};
    // varint 字节 eb 01 紧跟 flags(0x0004) 之后：ff 08 "SyncInfo" 04 00 eb 01 ...
    const std::string str_hex{to_hex(vec_first)};
    check(str_hex.starts_with("ff0853796e63496e666f0400eb01"),
          std::format("varint-235 prefix mismatch: {}", str_hex.substr(0, 24)));
    roundtrip_identity(order, "target-string-varint-multibyte");
  }

  {
    OrderModel order;
    order.str_order = "Move";
    order.b_has_subject = true;
    order.u4_subject = 7;
    roundtrip_identity(order, "subject-worldless");
  }

  {
    OrderModel order;
    order.str_order = "Attack";
    order.target.kind = TargetKind::kActor;
    order.target.u4_actor = 3;
    order.target.int4_generation = -2;
    roundtrip_identity(order, "actor-target-worldless");
  }

  {
    OrderModel order;
    order.str_order = "Move";
    order.target.kind = TargetKind::kTerrain;
    order.target.b_cell = true;
    order.target.int4_cell_bits = 0x7ff7ff80;
    order.target.u1_subcell = 2;
    roundtrip_identity(order, "cell-target-worldless");
  }

  {
    OrderModel order;
    order.str_order = "Attack";
    order.target.kind = TargetKind::kTerrain;
    order.target.int4_pos_x = 100;
    order.target.int4_pos_y = 200;
    order.target.int4_pos_z = 0;
    order.target.vec_terrain_positions.push_back({100, 200, 0});
    order.target.vec_terrain_positions.push_back({300, 400, 0});
    roundtrip_identity(order, "terrain-multi-pos");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.b_has_extra_actors = true;
    order.vec_extra_actors = {1, 2, 0xffffffffu};
    check_hex(order, "ff04546573740200030000000100000002000000ffffffff", "extra-actors hex");
    roundtrip_identity(order, "extra-actors");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.b_has_extra_actors = true;
    roundtrip_identity(order, "extra-actors-empty");
  }

  {
    OrderModel order;
    order.str_order = "CreateGroup";
    order.b_has_subject = true;
    order.u4_subject = 1;
    order.b_has_grouped = true;
    order.vec_grouped = {3, 4, 5};
    roundtrip_identity(order, "grouped");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.b_has_grouped = true;
    roundtrip_identity(order, "grouped-empty");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.b_handshake = true;
    order.opt_str_target = "payload";
    check_hex(order, "fe0454657374077061796c6f6164", "handshake hex");
    roundtrip_identity(order, "handshake");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.b_handshake = true;
    roundtrip_identity(order, "handshake-null-target");
  }

  {
    OrderModel order;
    order.str_order = "Test";
    order.target.kind = TargetKind::kFrozenActor;
    order.target.u4_viewer_actor = 1;
    order.target.u4_frozen_id = 9;
    roundtrip_identity(order, "frozen-target-worldless");
  }
}

namespace {

std::uint8_t hex_value(char ch) {
  if (ch >= '0' && ch <= '9') return static_cast<std::uint8_t>(ch - '0');
  if (ch >= 'a' && ch <= 'f') return static_cast<std::uint8_t>(ch - 'a' + 10);
  return static_cast<std::uint8_t>(ch - 'A' + 10);
}

std::vector<std::uint8_t> from_hex(std::string_view str_hex) {
  std::vector<std::uint8_t> vec_out;
  vec_out.reserve(str_hex.size() / 2);
  for (std::size_t idx{}; idx + 1 < str_hex.size(); idx += 2)
    vec_out.push_back(static_cast<std::uint8_t>((hex_value(str_hex[idx]) << 4) |
                                                hex_value(str_hex[idx + 1])));
  return vec_out;
}

std::optional<ora::game::network::OrderModel> parse_single(std::string_view str_hex) {
  const std::vector<std::uint8_t> vec_bytes{from_hex(str_hex)};
  ora::game::network::OrderReader reader{vec_bytes};
  return ora::game::network::deserialize_order(reader, nullptr);
}

}  // namespace

void test_deserialize_guards() {
  using namespace ora::game::network;

  check(!parse_single("").has_value(), "guard: empty input");
  check(!parse_single("ff").has_value(), "guard: truncated after type");
  check(!parse_single("ff045465").has_value(), "guard: truncated string");
  check(!parse_single("ff0454657374").has_value(), "guard: truncated flags");

  {
    const auto opt{parse_single("fe0454657374")};
    check(!opt.has_value(), "guard: handshake truncated target string");
  }

  {
    const auto opt{parse_single("00")};
    check(!opt.has_value(), "guard: unknown order type");
  }

  check(!parse_single("ff045465737402000500000000ffffffff").has_value(),
        "guard: extra-actor count exceeds payload");

  {
    const auto opt{parse_single("ff0454657374010099")};
    check(opt.has_value() && opt->str_order == "Test" && opt->target.kind == TargetKind::kInvalid,
          "guard: unknown target kind degrades to invalid and continues");
  }

  check(!parse_single("ff04546573740100020000008000000000ffffff7f").has_value(),
        "guard: terrain pos truncated before count");
}

void test_subject_world_semantics() {
  using namespace ora::game::network;
  using ora::game::world::WorldModel;

  WorldModel w;
  w.vec_actors_all.resize(10);
  w.vec_actors_all[7].b_in_world = true;
  w.vec_actors_all[3].b_in_world = true;
  w.vec_actors_all[3].b_disposed = true;
  w.vec_actors_all[3].b_in_world = false;

  {
    OrderModel order;
    order.str_order = "Move";
    order.b_has_subject = true;
    order.u4_subject = 7;
    const std::vector<std::uint8_t> vec_bytes{serialize_order(order)};
    OrderReader reader{vec_bytes};
    const auto opt{deserialize_order(reader, &w)};
    check(opt.has_value() && opt->b_has_subject && opt->u4_subject == 7,
          "world subject: valid id kept");
  }

  {
    OrderModel order;
    order.str_order = "Move";
    order.b_has_subject = true;
    order.u4_subject = 0xffffffffu;
    const std::vector<std::uint8_t> vec_bytes{serialize_order(order)};
    OrderReader reader{vec_bytes};
    const auto opt{deserialize_order(reader, &w)};
    check(!opt.has_value(), "world subject: MaxValue drops whole order");
  }

  {
    OrderModel order;
    order.str_order = "Move";
    order.b_has_subject = true;
    order.u4_subject = 999;
    const std::vector<std::uint8_t> vec_bytes{serialize_order(order)};
    OrderReader reader{vec_bytes};
    const auto opt{deserialize_order(reader, &w)};
    check(!opt.has_value(), "world subject: unknown id drops order");
  }

  {
    OrderModel order;
    order.str_order = "Attack";
    order.target.kind = TargetKind::kActor;
    order.target.u4_actor = 3;
    order.target.int4_generation = 1;
    const std::vector<std::uint8_t> vec_bytes{serialize_order(order)};
    OrderReader reader{vec_bytes};
    const auto opt{deserialize_order(reader, &w)};
    check(opt.has_value() && opt->target.kind == TargetKind::kInvalid,
          "world actor target: missing actor degrades to invalid");
  }

  {
    OrderModel order;
    order.str_order = "Attack";
    order.target.kind = TargetKind::kActor;
    order.target.u4_actor = 7;
    order.target.int4_generation = 4;
    const std::vector<std::uint8_t> vec_bytes{serialize_order(order)};
    OrderReader reader{vec_bytes};
    const auto opt{deserialize_order(reader, &w)};
    check(opt.has_value() && opt->target.kind == TargetKind::kActor &&
              opt->target.u4_actor == 7 && opt->target.int4_generation == 4,
          "world actor target: live actor kept with generation");
  }

  {
    OrderModel order;
    order.str_order = "Attack";
    order.target.kind = TargetKind::kFrozenActor;
    order.target.u4_viewer_actor = 7;
    order.target.u4_frozen_id = 2;
    const std::vector<std::uint8_t> vec_bytes{serialize_order(order)};
    OrderReader reader{vec_bytes};
    const auto opt{deserialize_order(reader, &w)};
    check(opt.has_value() && opt->target.kind == TargetKind::kInvalid,
          "world frozen target: minimal world degrades to invalid");
  }
}

}  // namespace

void test_order_packet() {
  using namespace ora::game::network;

  {
    OrderModel order_a;
    order_a.str_order = "Move";
    order_a.b_queued = true;
    OrderModel order_b;
    order_b.str_order = "Sell";
    const std::vector<std::uint8_t> vec_packet{
        order_packet_from_orders({order_a, order_b})};
    check(to_hex(vec_packet) ==
              "ff044d6f76650800" + std::string("ff0453656c6c0000"),
          "packet: two orders concatenated without length prefix");

    const std::vector<OrderModel> vec_parsed{order_packet_get_orders(vec_packet, nullptr)};
    check(vec_parsed.size() == 2 && vec_parsed[0].str_order == "Move" &&
              vec_parsed[0].b_queued && vec_parsed[1].str_order == "Sell",
          "packet: get_orders restores both");
  }

  {
    const std::vector<std::uint8_t> vec_empty{order_packet_from_orders({})};
    check(vec_empty.empty(), "packet: no orders yields empty bytes");
    check(order_packet_get_orders(vec_empty, nullptr).empty(), "packet: empty packet no orders");
  }

  {
    const std::vector<std::uint8_t> vec_body{from_hex("ff04546573740800")};
    const std::vector<std::uint8_t> vec_frame{
        order_packet_serialize_frame(vec_body, 258)};
    check(to_hex(vec_frame) == "02010000ff04546573740800", "packet: frame prefix little endian");
  }

  {
    const std::vector<std::uint8_t> vec_sync{serialize_sync_packet(7, -1, 0x1122334455667788ull)};
    check(vec_sync.size() == 17 && vec_sync[4] == 0x65,
          "packet: sync packet 17 bytes with 0x65 tag");
    check(to_hex(vec_sync) == "0700000065ffffffff8877665544332211", "packet: sync packet layout");
  }

  {
    const auto opt_ok{try_parse_order_packet(from_hex("03000000ff04546573740800"))};
    check(opt_ok.has_value() && opt_ok->int4_frame == 3 &&
              to_hex(opt_ok->vec_order_bytes) == "ff04546573740800",
          "parse: valid packet");

    const auto opt_empty{try_parse_order_packet(from_hex("03000000"))};
    check(opt_empty.has_value() && opt_empty->int4_frame == 3 &&
              opt_empty->vec_order_bytes.empty(),
          "parse: header-only packet yields empty orders");

    check(!try_parse_order_packet(from_hex("0300")).has_value(), "parse: len<4 rejected");

    check(!try_parse_order_packet(from_hex("03000000bf05")).has_value(),
          "parse: disconnect tag rejected");
    check(!try_parse_order_packet(from_hex("0300000065010203")).has_value(),
          "parse: sync tag rejected");
  }

  {
    ora::game::world::WorldModel w;
    w.vec_actors_all.resize(2);
    w.vec_actors_all[1].b_in_world = true;

    OrderModel order_ok;
    order_ok.str_order = "Move";
    order_ok.b_has_subject = true;
    order_ok.u4_subject = 1;

    OrderModel order_dropped;
    order_dropped.str_order = "Move";
    order_dropped.b_has_subject = true;
    order_dropped.u4_subject = 0xffffffffu;

    const std::vector<std::uint8_t> vec_packet{
        order_packet_from_orders({order_ok, order_dropped})};
    const std::vector<OrderModel> vec_parsed{order_packet_get_orders(vec_packet, &w)};
    check(vec_parsed.size() == 1 && vec_parsed[0].str_order == "Move" &&
              vec_parsed[0].b_has_subject,
          "packet: dropped order silently skipped, following orders intact");
  }

  {
    const std::vector<std::uint8_t> vec_packet{
        from_hex("ff0454657374040000ff045465")};
    const std::vector<OrderModel> vec_parsed{order_packet_get_orders(vec_packet, nullptr)};
    check(vec_parsed.size() == 1,
          "packet: truncated trailing order stops iteration without crash");
  }
}

void test_order_manager_pipeline() {
  using namespace ora::game::network;
  namespace ws = ora::game::world;

  constexpr std::string_view kRules{
      "world:\n"
      "\tFactionInfo@allies:\n"
      "player:\n"
      "\tOrderCounterProbe:\n"
      "probe.unit:\n"
      "\tOrderCounterProbe:\n"
      "\tOrderCounterProbe@second:\n"};

  constexpr std::string_view kMap{
      "Title: orders-map\n"
      "Players:\n"
      "\tPlayerReference@Neutral:\n"
      "\t\tName: Neutral\n"
      "\t\tOwnsWorld: True\n"
      "\t\tNonCombatant: True\n"
      "\t\tFaction: allies\n"
      "Actors:\n"
      "\tu0: probe.unit\n"
      "\t\tLocation: 1,2\n"
      "\t\tOwner: Neutral\n"
      "\tu1: probe.unit\n"
      "\t\tLocation: 3,4\n"
      "\t\tOwner: Neutral\n"};

  auto res_map{ws::synthetic_map_from_yaml(kRules, kMap)};
  check(res_map.has_value(), "pipeline: map parses");
  if (!res_map.has_value()) {
    std::println("map error: {}", res_map.error());
    return;
  }
  auto res_world{ws::world_create(*res_map, 0)};
  check(res_world.has_value(), "pipeline: world creates");
  if (!res_world.has_value()) {
    std::println("world error: {}", res_world.error());
    return;
  }
  ws::WorldModel w{std::move(*res_world)};

  const std::uint32_t u4_world_actor{0};
  const std::uint32_t u4_player_actor{1};
  const std::uint32_t u4_unit0{3};
  const std::uint32_t u4_unit1{4};
  check(w.vec_actors_all.size() == 5 && w.vec_actors_all[u4_unit0].str_type == "probe.unit",
        "pipeline: actor ids laid out as expected");

  auto move_order = [&](std::uint32_t u4_subject, bool b_queued) {
    OrderModel order;
    order.str_order = "Move";
    order.b_has_subject = true;
    order.u4_subject = u4_subject;
    order.target.kind = TargetKind::kTerrain;
    order.target.b_cell = true;
    order.target.int4_cell_bits = (5 << 20) | (6 << 8);
    order.target.u1_subcell = 1;
    order.b_queued = b_queued;
    return order;
  };
  auto trait_counter = [&](std::uint32_t u4_actor, std::size_t idx_trait) -> std::int32_t& {
    return w.vec_traits[w.vec_actors_all[u4_actor].vec_trait_handles[idx_trait]].int4_counter;
  };

  OrderManagerModel om;
  const std::int32_t arr_clients[]{1};
  order_manager_start_game(om, w, arr_clients);
  check(om.vec_pending.size() == 1 && om.vec_pending[0].int4_client == 1,
        "pipeline: single client pending queue");
  check(w.u4_net_frame == 1, "pipeline: net frame starts at 1");

  check(run_frame(om, w), "pipeline: frame 1 ticks");
  check(w.int4_world_tick == 1 && w.u4_net_frame == 2,
        "pipeline: frame 1 advances world tick and net frame");
  check(trait_counter(u4_unit0, 0) == 0, "pipeline: no orders applied yet");

  issue_order(om, move_order(u4_unit0, false));
  issue_order(om, move_order(u4_unit1, true));
  check(om.vec_local.size() == 2, "pipeline: two local orders queued");
  check(run_frame(om, w), "pipeline: frame 2 ticks");
  check(trait_counter(u4_unit0, 0) == 0 && trait_counter(u4_unit1, 0) == 0,
        "pipeline: orders projected to next frame not yet applied");
  check(run_frame(om, w), "pipeline: frame 3 ticks");
  check(trait_counter(u4_unit0, 0) == 1 && trait_counter(u4_unit1, 0) == 1,
        "pipeline: both move orders applied on next frame");
  check(trait_counter(u4_unit0, 1) == 1,
        "pipeline: every resolve trait on subject receives order in construct order");

  {
    OrderModel order_stance;
    order_stance.str_order = "SetUnitStance";
    order_stance.b_has_subject = true;
    order_stance.u4_subject = u4_unit0;
    order_stance.u4_extra_data = 2;
    issue_order(om, order_stance);
    run_frame(om, w);
    run_frame(om, w);
    check(trait_counter(u4_unit0, 0) == 1 && trait_counter(u4_unit0, 1) == 1,
        "pipeline: traits receive every order but self-filter by OrderString");
  }

  {
    OrderModel order_grouped;
    order_grouped.str_order = "Move";
    order_grouped.b_has_subject = true;
    order_grouped.u4_subject = u4_player_actor;
    order_grouped.b_has_grouped = true;
    order_grouped.vec_grouped = {u4_unit0, u4_unit1};
    issue_order(om, order_grouped);
    const std::int32_t int4_before0{trait_counter(u4_unit0, 0)};
    const std::int32_t int4_before1{trait_counter(u4_unit1, 0)};
    run_frame(om, w);
    run_frame(om, w);
    check(trait_counter(u4_unit0, 0) == int4_before0 + 1 &&
              trait_counter(u4_unit1, 0) == int4_before1 + 1,
        "pipeline: grouped order fans out to each grouped actor");
  }

  {
    ws::dispose_actor(w, u4_unit1);
    run_frame(om, w);
    const std::int32_t int4_before{trait_counter(u4_unit0, 0)};
    issue_order(om, move_order(u4_unit1, false));
    run_frame(om, w);
    run_frame(om, w);
    check(trait_counter(u4_unit0, 0) == int4_before,
        "pipeline: order for dead subject silently dropped");
  }

  {
    OrderModel order_chat;
    order_chat.str_order = "Chat";
    order_chat.b_is_immediate = true;
    order_chat.opt_str_target = "hello world";
    issue_order(om, order_chat);
    run_frame(om, w);
    check(om.vec_system_log.size() == 1 && om.vec_system_log[0].starts_with("chat 1 0 ") &&
              om.vec_system_log[0].ends_with("hello world"),
        "pipeline: immediate chat order applied same frame");
    check(om.vec_local.empty(), "pipeline: immediate order bypasses frame queue");
  }

  {
    OrderModel order_pause;
    order_pause.str_order = "PauseGame";
    order_pause.opt_str_target = "Pause";
    issue_order(om, order_pause);
    run_frame(om, w);
    run_frame(om, w);
    check(w.b_paused, "pipeline: PauseGame order pauses world");

    OrderModel order_unpause;
    order_unpause.str_order = "PauseGame";
    order_unpause.opt_str_target = "UnPause";
    issue_order(om, order_unpause);
    run_frame(om, w);
    run_frame(om, w);
    check(!w.b_paused, "pipeline: PauseGame UnPause resumes world");
  }

  {
    const std::size_t idx_sync_before{om.vec_sync_received.size()};
    run_frame(om, w);
    check(om.vec_sync_received.size() == idx_sync_before + 1,
        "pipeline: one sync sample per processed frame");
    check(!om.b_out_of_sync, "pipeline: echo sync never out of sync");
  }

  {
    OrderManagerModel om2;
    const std::int32_t arr_clients2[]{1, 2};
    order_manager_start_game(om2, w, arr_clients2);
    issue_order(om2, move_order(u4_unit0, false));
    check(!try_tick(om2, w), "pipeline: try_tick stalls while other client silent");
    tick_immediate(om2, w);
    check(!try_tick(om2, w), "pipeline: still stalled, other client has no packet");
    om2.vec_pending[1].deque_pending.emplace_back(1, std::vector<std::uint8_t>{});
    check(try_tick(om2, w), "pipeline: frame proceeds once every client has a packet");
  }
}

std::optional<ora::game::network::OrderModel> build_golden_case(std::string_view str_name) {
  using namespace ora::game::network;
  using TK = TargetKind;

  auto make_cell = [](std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_layer,
                      std::uint8_t u1_sub) {
    TargetModel target;
    target.kind = TK::kTerrain;
    target.b_cell = true;
    target.int4_cell_bits =
        ((int4_x & 0xfff) << 20) | ((int4_y & 0xfff) << 8) | int4_layer;
    target.u1_subcell = u1_sub;
    return target;
  };

  if (str_name == "empty") return OrderModel{};
  if (str_name == "unqueued") return OrderModel{.str_order = "Test"};
  if (str_name == "queued")
    return OrderModel{.str_order = "Test", .b_queued = true};
  if (str_name == "pos-target") {
    OrderModel order{.str_order = "Test"};
    order.target.kind = TK::kTerrain;
    order.target.int4_pos_x = std::numeric_limits<std::int32_t>::min();
    order.target.int4_pos_z = std::numeric_limits<std::int32_t>::max();
    order.target.vec_terrain_positions.push_back(
        {order.target.int4_pos_x, order.target.int4_pos_y, order.target.int4_pos_z});
    return order;
  }
  if (str_name == "invalid-target") return OrderModel{.str_order = "Test"};
  if (str_name == "extra-fields") {
    OrderModel order{.str_order = "Test", .b_queued = true};
    order.opt_str_target = "TargetString";
    order.int4_extra_location_bits = 0x7ff7ff80;
    order.u4_extra_data = 0xffffffffu;
    return order;
  }
  if (str_name == "target-string-empty") {
    OrderModel order{.str_order = "Test"};
    order.opt_str_target = "";
    return order;
  }
  if (str_name == "utf8-strings") {
    OrderModel order{.str_order = "Тест-日本語"};
    order.opt_str_target = "τ=2π";
    return order;
  }
  if (str_name == "handshake") {
    OrderModel order{.str_order = "Test", .b_handshake = true};
    order.opt_str_target = "payload";
    return order;
  }
  if (str_name == "handshake-empty") {
    OrderModel order{.str_order = "Test", .b_handshake = true};
    order.opt_str_target = "";
    return order;
  }
  if (str_name == "terrain-multi-pos") {
    OrderModel order{.str_order = "Attack"};
    order.target.kind = TK::kTerrain;
    order.target.int4_pos_x = 100;
    order.target.int4_pos_y = 200;
    order.target.vec_terrain_positions.push_back({100, 200, 0});
    order.target.vec_terrain_positions.push_back({300, 400, 0});
    return order;
  }
  if (str_name == "extra-actors-empty") {
    OrderModel order{.str_order = "Test"};
    order.b_has_extra_actors = true;
    return order;
  }
  if (str_name == "grouped-empty") {
    OrderModel order{.str_order = "CreateGroup"};
    order.b_has_grouped = true;
    return order;
  }
  if (str_name == "move-cell") {
    OrderModel order{.str_order = "Move", .b_queued = true};
    order.b_has_subject = true;
    order.u4_subject = 7;
    order.target = make_cell(10, 5, 0, 2);
    return order;
  }
  if (str_name == "actor-target") {
    OrderModel order{.str_order = "Attack", .b_queued = true};
    order.b_has_subject = true;
    order.u4_subject = 8;
    order.target.kind = TK::kActor;
    order.target.u4_actor = 9;
    order.target.int4_generation = -2;
    return order;
  }
  if (str_name == "start-production") {
    OrderModel order{.str_order = "StartProduction", .b_queued = true};
    order.b_has_subject = true;
    order.u4_subject = 2;
    order.opt_str_target = "e1";
    order.u4_extra_data = 5;
    return order;
  }
  if (str_name == "pause-production") {
    OrderModel order{.str_order = "PauseProduction"};
    order.b_has_subject = true;
    order.u4_subject = 2;
    order.opt_str_target = "e1";
    order.u4_extra_data = 1;
    return order;
  }
  if (str_name == "cancel-production") {
    OrderModel order{.str_order = "CancelProduction"};
    order.b_has_subject = true;
    order.u4_subject = 2;
    order.opt_str_target = "e1";
    order.u4_extra_data = 3;
    return order;
  }
  if (str_name == "place-building") {
    OrderModel order{.str_order = "PlaceBuilding"};
    order.b_has_subject = true;
    order.u4_subject = 1;
    order.target = make_cell(20, 10, 0, 0);
    order.opt_str_target = "fact";
    order.u4_extra_data = 42;
    order.int4_extra_location_bits = 1 << 8;
    return order;
  }
  if (str_name == "line-build") {
    OrderModel order{.str_order = "LineBuild"};
    order.b_has_subject = true;
    order.u4_subject = 1;
    order.target = make_cell(30, 15, 0, 0);
    order.opt_str_target = "pbox";
    order.u4_extra_data = 43;
    order.int4_extra_location_bits = 1 << 20;
    return order;
  }
  if (str_name == "create-group") {
    OrderModel order{.str_order = "CreateGroup"};
    order.b_has_subject = true;
    order.u4_subject = 1;
    order.b_has_grouped = true;
    order.vec_grouped = {3, 4, 5};
    return order;
  }
  if (str_name == "set-unit-stance") {
    OrderModel order{.str_order = "SetUnitStance"};
    order.b_has_subject = true;
    order.u4_subject = 6;
    order.u4_extra_data = 2;
    return order;
  }
  if (str_name == "extra-actors") {
    OrderModel order{.str_order = "Test"};
    order.b_has_extra_actors = true;
    order.vec_extra_actors = {3, 4};
    return order;
  }
  if (str_name == "chat") {
    OrderModel order{.str_order = "Chat", .b_is_immediate = true};
    order.opt_str_target = "Hello world";
    return order;
  }
  if (str_name == "chat-team") {
    OrderModel order{.str_order = "Chat", .b_is_immediate = true};
    order.opt_str_target = "team only";
    order.u4_extra_data = 2;
    return order;
  }
  if (str_name == "chat-spectators") {
    OrderModel order{.str_order = "Chat", .b_is_immediate = true};
    order.opt_str_target = "spec";
    order.u4_extra_data = 0xffffffffu;
    return order;
  }
  if (str_name == "command") {
    OrderModel order{.str_order = "Command", .b_is_immediate = true};
    order.opt_str_target = "/help";
    return order;
  }
  if (str_name == "pause-game") {
    OrderModel order{.str_order = "PauseGame"};
    order.opt_str_target = "Pause";
    return order;
  }
  return std::nullopt;
}

void test_golden_orders() {
  using namespace ora::game::network;
  using ora::game::world::WorldModel;

  std::ifstream stream_file{std::string(ORA_ORDERS_GOLDEN_DIR) + "/representative.txt"};
  check(bool(stream_file), "golden: representative.txt opens");
  if (!stream_file) return;

  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> vec_cases;
  std::string str_line;
  while (std::getline(stream_file, str_line)) {
    if (str_line.starts_with("case ")) {
      vec_cases.emplace_back(str_line.substr(5), std::vector<std::uint8_t>{});
    } else if (str_line.starts_with("hex ") && !vec_cases.empty()) {
      vec_cases.back().second = from_hex(std::string_view{str_line}.substr(4));
    }
  }
  check(vec_cases.size() == 28,
        std::format("golden: expected 28 cases, parsed {}", vec_cases.size()));

  WorldModel w;
  w.vec_actors_all.resize(10);
  for (std::uint32_t u4_id{1}; u4_id <= 9; ++u4_id) w.vec_actors_all[u4_id].b_in_world = true;

  for (const auto& [str_name, vec_hex] : vec_cases) {
    const auto opt_build{build_golden_case(str_name)};
    if (!opt_build.has_value()) {
      check(false, std::format("golden {}: no C++ builder for case", str_name));
      continue;
    }

    check(to_hex(serialize_order(*opt_build)) == to_hex(vec_hex),
          std::format("golden {}: C++ serialize == oracle hex", str_name));

    OrderReader reader_worldless{vec_hex};
    const auto opt_worldless{deserialize_order(reader_worldless, nullptr)};
    check(opt_worldless.has_value() && reader_worldless.idx_pos == vec_hex.size() &&
              serialize_order(*opt_worldless) == vec_hex,
          std::format("golden {}: worldless parse/re-serialize identity", str_name));

    OrderReader reader_world{vec_hex};
    const auto opt_world{deserialize_order(reader_world, &w)};
    check(opt_world.has_value() && reader_world.idx_pos == vec_hex.size() &&
              serialize_order(*opt_world) == vec_hex,
          std::format("golden {}: stub-world parse/re-serialize identity", str_name));
  }
}

int main() {
  test_roundtrip_tier_a();
  test_deserialize_guards();
  test_subject_world_semantics();
  test_order_packet();
  test_order_manager_pipeline();
  test_golden_orders();

  std::println("orders: {} checks, {} failed", g_total, g_failed);
  return g_failed == 0 ? 0 : 1;
}
