import std;

#include "game/network/order.hpp"
#include "game/network/order_manager.hpp"
#include "game/production/production.hpp"
#include "game/world/sync_hash.hpp"
#include "game/world/world.hpp"

// T10（issue #11）：建造/生产闭环含电力门槛。
// 口径修正（调研实证）：上游 ra 语义是电力不足生产 ×LowPowerModifier/100 减速
// （LowPowerModifier=300 → 每 3 tick 推进 1），非停摆；真停摆仅缺钱/出口堵/
// 生产建筑被禁用。ProductionItem.Tick 的令牌桶照上游实码（Slowdown-100，
// <0 时 +Modifier 并继续，>=0 跳过）。

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

constexpr std::int32_t kSeed{42};
constexpr std::uint32_t kPlayerActorId{2};

constexpr std::string_view kRulesCommon{
    "world:\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "player:\n"
    "\tClassicProductionQueue@Building:\n"
    "\t\tType: Building\n"
    "\t\tLowPowerModifier: 300\n"
    "\tClassicProductionQueue@Defense:\n"
    "\t\tType: Defense\n"
    "\t\tLowPowerModifier: 300\n"
    "\tClassicProductionQueue@Vehicle:\n"
    "\t\tType: Vehicle\n"
    "\t\tLowPowerModifier: 300\n"
    "\tClassicProductionQueue@Infantry:\n"
    "\t\tType: Infantry\n"
    "\t\tLowPowerModifier: 300\n"
    "\tClassicProductionQueue@Ship:\n"
    "\t\tType: Ship\n"
    "\t\tLowPowerModifier: 300\n"
    "\tClassicProductionQueue@Aircraft:\n"
    "\t\tType: Aircraft\n"
    "\t\tLowPowerModifier: 300\n"
    "\tPlaceBuilding:\n"
    "\tPlayerResources:\n"
    "\tPowerManager:\n"
    "\tProvidesTechPrerequisite@unrestricted:\n"
    "\t\tId: unrestricted\n"
    "\t\tPrerequisites: techlevel.infonly, techlevel.low, techlevel.medium, techlevel.high, techlevel.unrestricted\n"
    "mpspawn:\n"
    "mcv:\n"
    "fact:\n"
    "\tBuilding:\n"
    "\t\tFootprint: xxX xxx XxX ===\n"
    "\t\tDimensions: 3,4\n"
    "\t\tLocalCenterOffset: 0,-512,0\n"
    "\tBaseProvider:\n"
    "\t\tRange: 16c0\n"
    "\tProduction:\n"
    "\t\tProduces: Building, Defense\n"
    "\tGivesBuildableArea:\n"
    "\tPower:\n"
    "\t\tAmount: 0\n"
    "\tProvidesPrerequisite@soviet:\n"
    "\t\tFactions: soviet, russia, ukraine\n"
    "\t\tPrerequisite: structures.soviet\n"
    "powr:\n"
    "\tValued:\n"
    "\t\tCost: 300\n"
    "\tBuildable:\n"
    "\t\tQueue: Building\n"
    "\t\tPrerequisites: ~techlevel.infonly\n"
    "\tPower:\n"
    "\t\tAmount: 100\n"
    "\tBuilding:\n"
    "\t\tFootprint: xx xx ==\n"
    "\t\tDimensions: 2,3\n"
    "\t\tLocalCenterOffset: 0,-512,0\n"
    "\t\tRequiresBaseProvider: True\n"
    "\tGivesBuildableArea:\n"
    "\tProvidesPrerequisite:\n"
    "\t\tPrerequisite: anypower\n"
    "barr:\n"
    "\tValued:\n"
    "\t\tCost: 500\n"
    "\tBuildable:\n"
    "\t\tQueue: Building\n"
    "\t\tPrerequisites: anypower, ~structures.soviet, ~techlevel.infonly\n"
    "\tPower:\n"
    "\t\tAmount: -20\n"
    "\tBuilding:\n"
    "\t\tFootprint: xx xx ==\n"
    "\t\tDimensions: 2,3\n"
    "\t\tLocalCenterOffset: 0,-512,0\n"
    "\t\tRequiresBaseProvider: True\n"
    "\tGivesBuildableArea:\n"
    "\tProduction:\n"
    "\t\tProduces: Infantry, Soldier\n"
    "\tExit@1:\n"
    "\t\tExitCell: 1,2\n"
    "\t\tProductionTypes: Soldier, Infantry\n"
    "\tExit@2:\n"
    "\t\tExitCell: 0,2\n"
    "\t\tProductionTypes: Soldier, Infantry\n"
    "\tProvidesPrerequisite:\n"
    "\t\tPrerequisite: barracks\n"
    "\tProvidesPrerequisite@buildingname:\n"
    "silo:\n"
    "\tBuilding:\n"
    "\t\tFootprint: x\n"
    "\t\tDimensions: 1,1\n"
    "\tPower:\n"
    "\t\tAmount: -10\n"
    "e1:\n"
    "\tValued:\n"
    "\t\tCost: 100\n"
    "\tBuildable:\n"
    "\t\tQueue: Infantry\n"
    "\t\tBuildAtProductionType: Soldier\n"
    "\t\tPrerequisites: ~barracks, ~techlevel.infonly\n"
    "\tWithInfantryBody:\n"
    "\t\tIdleSequences: idle1, idle2\n"};

constexpr std::string_view kMapPositive{
    "Title: t10-positive\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Multi0:\n"
    "\t\tName: Multi0\n"
    "\t\tPlayable: True\n"
    "\t\tFaction: Random\n"
    "Actors:\n"
    "\tActor0: mpspawn\n"
    "\t\tLocation: 8,8\n"
    "\t\tOwner: Neutral\n"
    "\tActor1: fact\n"
    "\t\tLocation: 30,30\n"
    "\t\tOwner: Multi0\n"};

constexpr std::string_view kMapNegative{
    "Title: t10-negative\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Multi0:\n"
    "\t\tName: Multi0\n"
    "\t\tPlayable: True\n"
    "\t\tFaction: Random\n"
    "Actors:\n"
    "\tActor0: mpspawn\n"
    "\t\tLocation: 8,8\n"
    "\t\tOwner: Neutral\n"
    "\tActor1: fact\n"
    "\t\tLocation: 30,30\n"
    "\t\tOwner: Multi0\n"
    "\tActor2: barr\n"
    "\t\tLocation: 33,34\n"
    "\t\tOwner: Multi0\n"
    "\tActor3: silo\n"
    "\t\tLocation: 36,38\n"
    "\t\tOwner: Multi0\n"
    "\tActor4: silo\n"
    "\t\tLocation: 37,38\n"
    "\t\tOwner: Multi0\n"
    "\tActor5: silo\n"
    "\t\tLocation: 38,38\n"
    "\t\tOwner: Multi0\n"
    "\tActor6: silo\n"
    "\t\tLocation: 39,38\n"
    "\t\tOwner: Multi0\n"
    "\tActor7: silo\n"
    "\t\tLocation: 36,40\n"
    "\t\tOwner: Multi0\n"
    "\tActor8: silo\n"
    "\t\tLocation: 37,40\n"
    "\t\tOwner: Multi0\n"
    "\tActor9: silo\n"
    "\t\tLocation: 38,40\n"
    "\t\tOwner: Multi0\n"
    "\tActor10: silo\n"
    "\t\tLocation: 39,40\n"
    "\t\tOwner: Multi0\n"
    "\tActor11: silo\n"
    "\t\tLocation: 36,42\n"
    "\t\tOwner: Multi0\n"
    "\tActor12: silo\n"
    "\t\tLocation: 37,42\n"
    "\t\tOwner: Multi0\n"};

std::expected<ora::game::world::WorldModel, std::string> make_world(std::string_view str_map) {
  const auto res_map{ora::game::world::synthetic_map_from_yaml(kRulesCommon, str_map)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  ora::game::production::LobbyModel lobby;
  lobby.vec_playable_factions.emplace_back("Multi0", "russia");
  lobby.int4_starting_cash = 5000;
  lobby.str_techlevel = "unrestricted";
  return ora::game::world::world_create(*res_map, kSeed, lobby);
}

ora::game::network::OrderModel make_start_production(std::string_view str_item,
                                                     std::uint32_t u4_count) {
  ora::game::network::OrderModel order;
  order.str_order = "StartProduction";
  order.b_has_subject = true;
  order.u4_subject = kPlayerActorId;
  order.opt_str_target = std::string(str_item);
  order.u4_extra_data = u4_count;
  return order;
}

std::int32_t cell_bits(std::int32_t int4_x, std::int32_t int4_y) {
  return static_cast<std::int32_t>(((int4_x & 0xfff) << 20) | ((int4_y & 0xfff) << 8));
}

ora::game::network::OrderModel make_place_building(std::string_view str_item, std::int32_t int4_x,
                                                    std::int32_t int4_y) {
  ora::game::network::OrderModel order;
  order.str_order = "PlaceBuilding";
  order.b_has_subject = true;
  order.u4_subject = kPlayerActorId;
  order.target.kind = ora::game::network::TargetKind::kTerrain;
  order.target.b_cell = true;
  order.target.int4_cell_bits = cell_bits(int4_x, int4_y);
  order.target.u1_subcell = 0;
  order.opt_str_target = std::string(str_item);
  order.u4_extra_data = kPlayerActorId;
  return order;
}

struct ScenarioRun {
  std::string str_observations{};
  ora::game::world::WorldModel w;
};

ScenarioRun run_scenario(std::string_view str_map, std::int32_t int4_ticks,
                         const std::vector<std::pair<std::int32_t, ora::game::network::OrderModel>>& vec_orders) {
  auto res_world{make_world(str_map)};
  if (!res_world.has_value()) {
    check(false, std::format("scenario world creates: {}", res_world.error()));
    constexpr std::string_view kFallbackRules{"world:\n\tFactionInfo@allies:\n"};
    constexpr std::string_view kFallbackMap{
        "Players:\n\tPlayerReference@Neutral:\n\t\tName: Neutral\n\t\tOwnsWorld: True\n\t\tFaction: allies\n"};
    auto res_fallback{ora::game::world::synthetic_map_from_yaml(kFallbackRules, kFallbackMap)};
    ScenarioRun run{
        .w = res_fallback.has_value()
                 ? ora::game::world::world_create(*res_fallback, kSeed).value_or(
                       ora::game::world::WorldModel{})
                 : ora::game::world::WorldModel{}};
    return run;
  }
  ScenarioRun run{.w = std::move(*res_world)};
  ora::game::network::OrderManagerModel om;
  const std::vector<std::int32_t> vec_clients{1};
  ora::game::network::order_manager_start_game(om, run.w, vec_clients);
  for (std::int32_t int4_tick{0}; int4_tick < int4_ticks; ++int4_tick) {
    for (const auto& [int4_frame, order] : vec_orders)
      if (int4_frame == static_cast<std::int32_t>(run.w.u4_net_frame))
        ora::game::network::issue_order(om, order);
    ora::game::network::run_frame(om, run.w);
    run.str_observations += ora::game::production::production_observation(run.w, 1);
  }
  return run;
}

std::string read_golden(std::string_view str_name) {
  std::ifstream stream_file{std::string(ORA_PRODUCTION_GOLDEN_DIR) + "/" + std::string(str_name)};
  if (!stream_file) return {};
  std::string str_out;
  for (std::string str_line; std::getline(stream_file, str_line);) {
    if (str_line.empty() || str_line.starts_with("#")) continue;
    str_out += str_line;
    str_out += '\n';
  }
  return str_out;
}

void test_power_state() {
  using ora::game::production::PowerState;
  using ora::game::production::power_state;
  check(power_state(100, 100) == PowerState::kNormal, "power 100 vs 100 Normal (>=)");
  check(power_state(100, 99) == PowerState::kNormal, "power 100 vs 99 Normal");
  check(power_state(100, 140) == PowerState::kLow, "power 100 vs 140 Low (100 > 70)");
  check(power_state(100, 199) == PowerState::kLow, "power 100 vs 199 Low (100 > 99)");
  check(power_state(100, 200) == PowerState::kCritical, "power 100 vs 200 Critical (100 > 100 false, int div)");
  check(power_state(100, 201) == PowerState::kCritical, "power 100 vs 201 Critical");
  check(power_state(0, 120) == PowerState::kCritical, "power 0 vs 120 Critical");
  check(power_state(0, 0) == PowerState::kNormal, "power 0 vs 0 Normal");
  check(ora::game::production::power_state_name(power_state(100, 140)) == "low", "power state name low");
}

void test_apply_percentage_modifiers() {
  using ora::game::production::apply_percentage_modifiers;
  const std::array<std::int32_t, 2> vec4_build{60, 100};
  check(apply_percentage_modifiers(100, vec4_build) == 60, "E1 build time 100*60% = 60");
  check(apply_percentage_modifiers(300, vec4_build) == 180, "POWR build time 300 -> 180");
  check(apply_percentage_modifiers(500, vec4_build) == 300, "BARR build time 500 -> 300");
  check(apply_percentage_modifiers(600, vec4_build) == 360, "FTUR build time 600 -> 360");
  const std::array<std::int32_t, 2> vec4_half{50, 50};
  check(apply_percentage_modifiers(101, vec4_half) == 25, "single truncation 101*0.25 = 25.25 -> 25");
  const std::array<std::int32_t, 1> vec4_boost{150};
  check(apply_percentage_modifiers(7, vec4_boost) == 10, "150% of 7 truncates once to 10");
}

void test_take_cash() {
  using ora::game::production::PlayerResourcesState;
  using ora::game::production::take_cash;
  PlayerResourcesState res{.int4_cash = 5000, .int4_resources = 0};
  check(take_cash(res, 5), "take 5 succeeds");
  check(res.int4_cash == 4995 && res.int4_resources == 0, "take 5 from cash only");
  PlayerResourcesState ore{.int4_cash = 100, .int4_resources = 30};
  check(take_cash(ore, 50), "take 50 with ore succeeds");
  check(ore.int4_resources == 0 && ore.int4_cash == 80, "ore spent before cash (30 ore + 20 cash)");
  PlayerResourcesState poor{.int4_cash = 10, .int4_resources = 0};
  check(!take_cash(poor, 11), "take beyond funds fails");
  check(poor.int4_cash == 10, "failed take leaves cash untouched");
}

void test_shuffle_forward() {
  ora::game::support::MersenneTwister rng{ora::game::support::make_mersenne_twister(kSeed)};
  std::vector<std::int32_t> vec_items{0, 1};
  ora::game::production::shuffle_forward(rng, std::span<std::int32_t>{vec_items});
  check(rng.int4_total_count == 1, "two-element forward shuffle consumes exactly one draw");

  ora::game::support::MersenneTwister rng_ref{ora::game::support::make_mersenne_twister(kSeed)};
  const std::int32_t int4_j{ora::game::support::next(rng_ref, 2)};
  std::vector<std::int32_t> vec_expect{0, 1};
  std::swap(vec_expect[0], vec_expect[static_cast<std::size_t>(int4_j)]);
  check(vec_items == vec_expect, "forward shuffle matches Util.Shuffle swap(i, i+j)");

  std::vector<std::int32_t> vec_one{7};
  ora::game::support::MersenneTwister rng_one{ora::game::support::make_mersenne_twister(kSeed)};
  ora::game::production::shuffle_forward(rng_one, std::span<std::int32_t>{vec_one});
  check(rng_one.int4_total_count == 0 && vec_one[0] == 7, "single-element shuffle consumes nothing");
}

const ora::game::production::ProductionItemModel* queue_head_item(
    const ora::game::world::WorldModel& w, std::string_view str_type) {
  const auto& vec_queues{w.prod.vec_players[1].vec_queues};
  for (const auto& queue : vec_queues) {
    if (queue.str_type != str_type) continue;
    if (queue.vec_queue.empty()) return nullptr;
    return &queue.vec_queue[0];
  }
  return nullptr;
}

std::int32_t first_actor_tick(const std::string& str_observations, std::string_view str_type) {
  const std::string str_marker{":" + std::string(str_type) + ":"};
  std::size_t idx_pos{0};
  while (true) {
    idx_pos = str_observations.find("\nactors ", idx_pos);
    if (idx_pos == std::string::npos) return -1;
    const std::size_t idx_end{str_observations.find('\n', idx_pos + 1)};
    if (str_observations.substr(idx_pos, idx_end - idx_pos).contains(str_marker)) {
      const std::size_t idx_line{str_observations.rfind("tick ", idx_pos)};
      if (idx_line == std::string::npos) return -1;
      const std::string_view str_line{std::string_view{str_observations}.substr(
          idx_line, str_observations.find('\n', idx_line) - idx_line)};
      const std::size_t idx_space{str_line.find(' ')};
      std::int32_t int4_tick{-1};
      std::from_chars(str_line.data() + idx_space + 1, str_line.data() + str_line.size(), int4_tick);
      return int4_tick;
    }
    idx_pos = idx_end;
  }
}

void test_positive_scenario() {
  const std::vector<std::pair<std::int32_t, ora::game::network::OrderModel>> vec_orders{
      {10, make_start_production("powr", 1)},
      {200, make_place_building("powr", 33, 30)},
      {210, make_start_production("barr", 1)},
      {520, make_place_building("barr", 33, 34)},
      {540, make_start_production("e1", 1)},
  };
  // 剧本截断在 e1 出厂帧（601）：E1 带武装，出厂后 AutoTarget idle 扫描每 3-8 tick
  // 耗 SharedRandom（战斗面，T12）——生产链 rng 账本只覆盖到出厂 shuffle 为止。
  ScenarioRun run{run_scenario(kMapPositive, 601, vec_orders)};
  const auto& w{run.w};

  check(w.vec_players.size() == 3, "positive: three players");
  check(w.vec_players[0].str_internal_name == "Neutral" && w.vec_players[1].str_internal_name == "Multi0" &&
            w.vec_players[2].str_internal_name == "Everyone",
        "positive: player order Neutral, Multi0 (lobby slot), Everyone");
  check(w.vec_players[1].b_playable && w.vec_players[1].str_faction == "russia",
        "positive: Multi0 playable russia faction");
  check(w.vec_actors_all.size() == 10 && w.u4_next_aid == 10,
        "positive: 10 actors after scenario (7 initial + powr + barr + e1)");
  check(w.rng_shared.int4_total_count == 3,
        "positive: shared rng consumed 3x on exit frame (exit shuffle + infantry idle sequence + idle delay)");

  const auto& res{w.prod.vec_players[1].res};
  check(res.int4_cash == 4100 && res.int4_resources == 0,
        "positive: final cash 5000-300-500-100 = 4100");
  const auto& power{w.prod.vec_players[1].power};
  check(power.int4_power_provided == 100 && power.int4_power_drained == 20,
        "positive: final power 100 vs 20");
  check(ora::game::production::power_state(power.int4_power_provided, power.int4_power_drained) ==
            ora::game::production::PowerState::kNormal,
        "positive: final power state Normal");

  const auto* ptr_powr{queue_head_item(w, "building")};
  check(ptr_powr == nullptr, "positive: building queue empty after placements");

  bool b_powr_seen{false};
  bool b_barr_seen{false};
  bool b_e1_seen{false};
  for (const auto& id : w.vec_in_world) {
    const auto& actor{w.vec_actors_all[id]};
    b_powr_seen |= actor.str_type == "powr";
    b_barr_seen |= actor.str_type == "barr";
    b_e1_seen |= actor.str_type == "e1";
  }
  check(b_powr_seen && b_barr_seen && b_e1_seen, "positive: powr, barr, e1 all in world");

  const auto& actor_e1{w.vec_actors_all[9]};
  check(actor_e1.str_type == "e1" && actor_e1.u4_owner == 1,
        "positive: e1 actor id 9 owned by Multi0");
  check((actor_e1.int4_pos_x == 34 && actor_e1.int4_pos_y == 36) ||
            (actor_e1.int4_pos_x == 33 && actor_e1.int4_pos_y == 36),
        "positive: e1 spawned at a barracks exit cell");

  check(first_actor_tick(run.str_observations, "powr") == 201,
        "positive: powr placed and in world at tick 201");
  check(first_actor_tick(run.str_observations, "barr") == 521,
        "positive: barr placed and in world at tick 521");
  check(first_actor_tick(run.str_observations, "e1") == 601,
        "positive: e1 exits barracks at tick 601 (order at 541 + 60 ticks)");
}

void test_negative_scenario() {
  const std::vector<std::pair<std::int32_t, ora::game::network::OrderModel>> vec_orders{
      {20, make_start_production("powr", 1)},
      {570, make_place_building("powr", 33, 30)},
      {580, make_start_production("e1", 1)},
  };
  // 剧本截断在 e1 出厂帧（761）。开局 0 vs 120 Critical（BARR -20 + 10×SILO -100）；
  // POWR(+100) 放置 @571 后 100 vs 120 Low——两档电力下生产均为 ×3 减速（令牌桶）。
  ScenarioRun run{run_scenario(kMapNegative, 761, vec_orders)};
  const auto& w{run.w};

  {
    auto res_fresh{make_world(kMapNegative)};
    check(res_fresh.has_value(), "negative: fresh world creates");
    if (res_fresh.has_value()) {
      const auto& power_fresh{res_fresh->prod.vec_players[1].power};
      check(power_fresh.int4_power_provided == 0 && power_fresh.int4_power_drained == 120,
            "negative: initial power 0 vs 120");
    }
    check(ora::game::production::power_state(0, 120) == ora::game::production::PowerState::kCritical,
          "negative: initial state Critical");
  }

  std::size_t idx_slow_line_200{std::string::npos};
  std::size_t idx_slow_line_100{std::string::npos};
  std::size_t idx_slow_line_0{std::string::npos};
  std::size_t idx_pos{0};
  while (true) {
    idx_pos = run.str_observations.find("i ", idx_pos);
    if (idx_pos == std::string::npos) break;
    const std::size_t idx_end{run.str_observations.find('\n', idx_pos)};
    const std::string str_line{
        run.str_observations.substr(idx_pos, idx_end - idx_pos)};
    if (idx_slow_line_200 == std::string::npos && str_line.contains("slow=200")) idx_slow_line_200 = idx_pos;
    if (idx_slow_line_100 == std::string::npos && str_line.contains("slow=100")) idx_slow_line_100 = idx_pos;
    if (idx_slow_line_0 == std::string::npos && str_line.contains("slow=0 ")) idx_slow_line_0 = idx_pos;
    idx_pos = idx_end;
  }
  check(idx_slow_line_200 != std::string::npos && idx_slow_line_100 != std::string::npos &&
            idx_slow_line_0 != std::string::npos,
        "negative: slowdown token bucket cycles 200, 100, 0 (x3 slowdown, not stall)");

  const auto& power_final{w.prod.vec_players[1].power};
  check(power_final.int4_power_provided == 100 && power_final.int4_power_drained == 120,
        "negative: final power 100 vs 120 after powr placement");
  check(ora::game::production::power_state(100, 120) == ora::game::production::PowerState::kLow,
        "negative: final state Low");

  check(w.rng_shared.int4_total_count == 3,
        "negative: shared rng consumed 3x on exit frame (exit shuffle + infantry idle sequence + idle delay)");
  check(w.vec_actors_all.size() == 20 && w.u4_next_aid == 20,
        "negative: 20 actors after scenario (18 initial + powr + e1)");
  check(w.vec_actors_all[17].str_type == "mcv" && w.vec_actors_all[18].str_type == "powr" &&
            w.vec_actors_all[19].str_type == "e1",
        "negative: runtime creation order mcv(17), powr(18), e1(19)");
  const auto& res{w.prod.vec_players[1].res};
  check(res.int4_cash == 5000 - 300 - 100,
        "negative: final cash 5000 - powr - e1");

  check(first_actor_tick(run.str_observations, "e1") == 759,
        "negative: e1 exits at tick 759 (60-tick build at x3 slowdown, Low power; golden-anchored)");
  check(first_actor_tick(run.str_observations, "powr") == 571,
        "negative: powr placed at tick 571 (180-tick build at x3 slowdown from 21, Critical power)");
  check(first_actor_tick(run.str_observations, "mcv") == 1,
        "negative: mcv present from first observation");
}

void test_determinism() {
  const std::vector<std::pair<std::int32_t, ora::game::network::OrderModel>> vec_orders{
      {20, make_start_production("powr", 1)},
      {570, make_place_building("powr", 33, 30)},
      {580, make_start_production("e1", 1)},
  };
  const ScenarioRun run_a{run_scenario(kMapNegative, 761, vec_orders)};
  const ScenarioRun run_b{run_scenario(kMapNegative, 761, vec_orders)};
  check(run_a.str_observations == run_b.str_observations,
        "determinism: same seed observation streams identical (761 ticks)");
  check(ora::game::world::state_digest(run_a.w) == ora::game::world::state_digest(run_b.w),
        "determinism: same seed state digests identical");
}

void test_golden_compare() {
  const std::vector<std::pair<std::int32_t, ora::game::network::OrderModel>> vec_orders_pos{
      {10, make_start_production("powr", 1)},
      {200, make_place_building("powr", 33, 30)},
      {210, make_start_production("barr", 1)},
      {520, make_place_building("barr", 33, 34)},
      {540, make_start_production("e1", 1)},
  };
  const std::vector<std::pair<std::int32_t, ora::game::network::OrderModel>> vec_orders_neg{
      {20, make_start_production("powr", 1)},
      {570, make_place_building("powr", 33, 30)},
      {580, make_start_production("e1", 1)},
  };

  const std::string str_golden_pos{read_golden("positive.txt")};
  check(!str_golden_pos.empty(), "golden: positive.txt present and non-empty");
  if (!str_golden_pos.empty()) {
    const ScenarioRun run{run_scenario(kMapPositive, 601, vec_orders_pos)};
    check(run.str_observations == str_golden_pos,
          "golden: positive scenario observation stream matches oracle");
    if (run.str_observations != str_golden_pos) {
      const std::size_t idx_max{std::min(run.str_observations.size(), str_golden_pos.size())};
      std::size_t idx_diff{0};
      while (idx_diff < idx_max && run.str_observations[idx_diff] == str_golden_pos[idx_diff])
        ++idx_diff;
      const std::size_t idx_line_start{run.str_observations.rfind('\n', idx_diff) + 1};
      std::println("  first diff near offset {}:\n  cpp  {}\n  gold {}",
                   idx_diff, run.str_observations.substr(idx_line_start, 160),
                   str_golden_pos.substr(std::min(idx_line_start, str_golden_pos.size()), 160));
    }
  }

  const std::string str_golden_neg{read_golden("negative.txt")};
  check(!str_golden_neg.empty(), "golden: negative.txt present and non-empty");
  if (!str_golden_neg.empty()) {
    const ScenarioRun run{run_scenario(kMapNegative, 761, vec_orders_neg)};
    check(run.str_observations == str_golden_neg,
          "golden: negative scenario observation stream matches oracle");
  }
}

}  // namespace

int main() {
  test_power_state();
  test_apply_percentage_modifiers();
  test_take_cash();
  test_shuffle_forward();
  test_positive_scenario();
  test_negative_scenario();
  test_determinism();
  test_golden_compare();

  std::println("production: {} checks, {} failed", g_total, g_failed);
  return g_failed == 0 ? 0 : 1;
}
