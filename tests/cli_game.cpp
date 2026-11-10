import std;

#include <initializer_list>
import ora.game;
import ora.game.combat;
import ora.game.input;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;
import ora.launcher;
import ora.test.util;

using ora::test::check;

namespace {

namespace lch = ora::launcher;
namespace ogm = ora::game;
namespace net = ora::game::network;
namespace opro = ora::game::production;
namespace inp_view = ora::game::input;
namespace ww = ora::game::world;


std::vector<std::string_view> args(std::initializer_list<std::string_view> vec_parts) {
  return std::vector<std::string_view>{vec_parts};
}

void test_parse_defaults() {
  const lch::CliParseResult res{lch::cli_args_parse(args({}))};
  check(res.b_ok, "parse: empty argv is valid");
  check(res.vec_errors.empty(), "parse: empty argv yields no errors");
  check(res.args.str_mod == "ra", "parse: default mod is ra");
  check(res.args.str_map.empty(), "parse: default map is built-in sandbox");
  check(res.args.int4_seed == 0, "parse: default seed is 0");
  check(res.args.int4_tick_ms == 40, "parse: default tick is 40ms");
  check(!res.args.b_fullscreen, "parse: default is windowed");
  check(!res.args.b_headless, "parse: default is windowed session");
}

void test_parse_full() {
  const lch::CliParseResult res{lch::cli_args_parse(
      args({"--mod", "ra", "--map", "ore-lord.oramap", "--seed", "42", "--tick-ms", "80",
            "--fullscreen", "--headless", "--ticks", "600", "--content-dir", "C:/Content",
            "--log-file", "run.log"}))};
  check(res.b_ok, "parse: full argv is valid");
  if (!res.b_ok) {
    for (const std::string& str_err : res.vec_errors) std::println("  err: {}", str_err);
    return;
  }
  check(res.args.str_mod == "ra", "parse: mod ra");
  check(res.args.str_map == "ore-lord.oramap", "parse: map path");
  check(res.args.int4_seed == 42, "parse: seed 42");
  check(res.args.int4_tick_ms == 80, "parse: tick 80ms");
  check(res.args.b_fullscreen, "parse: fullscreen flag");
  check(res.args.b_headless, "parse: headless flag");
  check(res.args.int4_headless_ticks == 600, "parse: headless ticks");
  check(res.args.str_content_dir == "C:/Content", "parse: content dir");
  check(res.args.str_log_file == "run.log", "parse: log file");
}

void test_parse_errors() {
  check(!lch::cli_args_parse(args({"--wat"})).b_ok, "parse: unknown flag rejected");
  check(!lch::cli_args_parse(args({"--mod"})).b_ok, "parse: missing value rejected");
  check(!lch::cli_args_parse(args({"--seed", "abc"})).b_ok, "parse: bad seed rejected");
  check(!lch::cli_args_parse(args({"--seed", "99999999999999"})).b_ok,
        "parse: out-of-range seed rejected");
  check(!lch::cli_args_parse(args({"--tick-ms", "0"})).b_ok, "parse: zero tick rejected");
  check(!lch::cli_args_parse(args({"--ticks", "-5"})).b_ok, "parse: negative ticks rejected");
  check(!lch::cli_args_parse(args({"positional"})).b_ok, "parse: positional rejected");

  const lch::CliParseResult res_seed{lch::cli_args_parse(args({"--seed", "-3"}) )};
  check(res_seed.b_ok && res_seed.args.int4_seed == -3, "parse: negative seed allowed");
}

void test_parse_flags() {
  const lch::CliParseResult res_help{lch::cli_args_parse(args({"--help"}))};
  check(res_help.b_help && res_help.b_ok, "parse: --help flag");
  const lch::CliParseResult res_version{lch::cli_args_parse(args({"--version"}))};
  check(res_version.b_version && res_version.b_ok, "parse: --version flag");
  check(!lch::cli_args_parse(args({"--mod", "cnc"})).b_ok, "parse: non-ra mod rejected in M1");
}

void test_sandbox_recipe() {
  check(!lch::sandbox_rules_text().empty(), "recipe: rules text non-empty");
  check(!lch::sandbox_map_text().empty(), "recipe: map text non-empty");
  check(!lch::sandbox_weapons_text().empty(), "recipe: weapons text non-empty");

  const lch::SandboxMapData data{lch::sandbox_map_data()};
  check(data.bin_map.int4_width == 40 && data.bin_map.int4_height == 40,
        "recipe: sandbox is 40x40");
  check(!data.tileset.vec_templates.empty(), "recipe: synthetic tileset has a template");
  check(data.tileset.find_template(1) != nullptr, "recipe: template id 1 exists");

  std::int32_t int4_ore_cells{};
  for (const ogm::ResourceTile& tile : data.bin_map.vec_resources)
    if (tile.u1_type != 0 && tile.u1_density > 0) ++int4_ore_cells;
  check(int4_ore_cells >= 12, "recipe: sandbox has an ore field");
}

std::uint32_t actor_id_of_type(const lch::GameSession& s, std::string_view str_type,
                               std::size_t idx_skip) {
  std::size_t idx_seen{0};
  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    if (s.w.vec_actors_all[u4_id].str_type != str_type) continue;
    if (idx_seen == idx_skip) return u4_id;
    ++idx_seen;
  }
  return 0xffffffffu;
}

std::uint32_t actor_id_of_owner(const lch::GameSession& s, std::string_view str_type, bool b_own,
                                std::size_t idx_skip) {
  std::size_t idx_seen{0};
  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    const ww::ActorModel& actor{s.w.vec_actors_all[u4_id]};
    if (actor.str_type != str_type) continue;
    if ((actor.u4_owner == s.u4_viewer) != b_own) continue;
    if (idx_seen == idx_skip) return u4_id;
    ++idx_seen;
  }
  return 0xffffffffu;
}

void test_session_setup() {
  auto res_session{lch::session_setup(lch::SessionEnv{.int4_seed = 42, .str_map_path = {}, .opt_lobby = {}})};
  if (!res_session.has_value()) {
    check(false, std::format("setup: {}", res_session.error()));
    return;
  }
  lch::session_tick(*res_session);
  const lch::GameSession& s{*res_session};

  check(s.w.vec_players.size() == 4, "setup: Neutral+Creeps+Multi0+Everyone players");
  check(s.w.vec_players[0].str_internal_name == "Neutral", "setup: player 0 Neutral");
  check(s.w.vec_players[0].b_owns_world, "setup: Neutral owns world");
  check(s.w.vec_players[1].str_internal_name == "Creeps", "setup: player 1 Creeps");
  check(s.w.vec_players[1].b_non_combatant, "setup: Creeps non-combatant");
  check(s.w.vec_players[2].str_internal_name == "Multi0", "setup: player 2 Multi0");
  check(s.w.vec_players[2].b_playable, "setup: Multi0 playable");
  check(s.w.vec_players[3].str_internal_name == "Everyone", "setup: player 3 Everyone");
  check(s.w.vec_players[3].b_spectating, "setup: Everyone spectating");
  check(s.u4_viewer == 2, "setup: viewer is Multi0");

  const auto& res_multi0{s.w.prod.vec_players[2].res};
  check(res_multi0.int4_cash == 5000, "setup: starting cash 5000");
  check(s.w.prod.vec_players[2].power.int4_power_provided == 0,
        "setup: no power at start (fact 0)");

  const opro::ProductionQueueState* ptr_building{};
  const opro::ProductionQueueState* ptr_infantry{};
  const opro::ProductionQueueState* ptr_defense{};
  for (const auto& queue : s.w.prod.vec_players[2].vec_queues) {
    if (queue.str_type == "building") ptr_building = &queue;
    if (queue.str_type == "infantry") ptr_infantry = &queue;
    if (queue.str_type == "defense") ptr_defense = &queue;
  }
  check(ptr_building != nullptr && ptr_building->b_enabled,
        "setup: Building queue enabled (fact produces)");
  check(ptr_infantry != nullptr && !ptr_infantry->b_enabled,
        "setup: Infantry queue disabled (no barracks yet)");
  check(ptr_defense != nullptr && ptr_defense->b_enabled,
        "setup: Defense queue enabled (fact produces Building+Defense)");

  check(actor_id_of_type(s, "fact", 0) != 0xffffffffu, "setup: fact in world");
  check(actor_id_of_type(s, "proc", 0) != 0xffffffffu, "setup: proc in world");
  check(actor_id_of_type(s, "harv", 0) != 0xffffffffu, "setup: free harv spawned");
  const std::uint32_t u4_e1_own{actor_id_of_type(s, "e1", 0)};
  const std::uint32_t u4_e1_enemy{actor_id_of_type(s, "e1", 1)};
  check(u4_e1_own != 0xffffffffu, "setup: own starting e1 in world");
  check(u4_e1_enemy != 0xffffffffu, "setup: enemy e1 in world");
  check(u4_e1_own != u4_e1_enemy, "setup: two distinct e1 actors");
  check(s.w.vec_actors_all[u4_e1_enemy].u4_owner == 1, "setup: enemy e1 owned by Creeps");
  check(s.w.econ.layer.int4_res_cells >= 12, "setup: resource layer populated");
  check(s.w.econ.b_ready, "setup: economy ready");

  const std::uint32_t u4_harv{actor_id_of_type(s, "harv", 0)};
  bool b_harv_in_econ{};
  for (const auto& harv : s.w.econ.vec_harvs)
    if (harv.u4_actor == u4_harv) b_harv_in_econ = true;
  check(b_harv_in_econ, "setup: harv registered in economy");

  bool b_own_e1_mover{false};
  bool b_enemy_e1_mover{false};
  bool b_own_e1_fighter{false};
  for (const auto& mover : s.vec_movers) {
    if (mover.u4_actor == u4_e1_own) b_own_e1_mover = true;
    if (mover.u4_actor == u4_e1_enemy) b_enemy_e1_mover = true;
  }
  for (const auto& fighter : s.vec_fighters)
    if (fighter.unit.u4_id == u4_e1_own) b_own_e1_fighter = true;
  check(b_own_e1_mover && b_enemy_e1_mover, "setup: e1 units registered as movers");
  check(b_own_e1_fighter, "setup: own e1 registered as fighter");
  bool b_harv_mover{false};
  for (const auto& mover : s.vec_movers)
    if (mover.u4_actor == u4_harv) b_harv_mover = true;
  check(!b_harv_mover, "setup: harv not in mover bridge (economy-driven)");

  check(s.weapons.find("carbine") != nullptr, "setup: carbine weapon loaded");
  check(s.weapons.find("carbine")->int4_range == 5120, "setup: carbine range 5c0");
  check(s.om.b_game_started, "setup: order manager started");
  check(!s.vec_events.empty(), "setup: world ready event logged");
}

std::int32_t cell_bits(std::int32_t int4_x, std::int32_t int4_y) {
  return static_cast<std::int32_t>(((int4_x & 0xfff) << 20) | ((int4_y & 0xfff) << 8));
}

net::OrderModel order_start_production(std::uint32_t u4_player_actor, std::string_view str_item) {
  net::OrderModel order;
  order.str_order = "StartProduction";
  order.b_has_subject = true;
  order.u4_subject = u4_player_actor;
  order.opt_str_target = std::string{str_item};
  order.u4_extra_data = 1;
  return order;
}

net::OrderModel order_place_building(std::uint32_t u4_player_actor, std::string_view str_item,
                                     std::int32_t int4_x, std::int32_t int4_y) {
  net::OrderModel order;
  order.str_order = "PlaceBuilding";
  order.b_has_subject = true;
  order.u4_subject = u4_player_actor;
  order.target.kind = net::TargetKind::kTerrain;
  order.target.b_cell = true;
  order.target.int4_cell_bits = cell_bits(int4_x, int4_y);
  order.target.u1_subcell = 0;
  order.opt_str_target = std::string{str_item};
  order.u4_extra_data = u4_player_actor;
  return order;
}

net::OrderModel order_move(std::uint32_t u4_subject, std::int32_t int4_x, std::int32_t int4_y) {
  net::OrderModel order;
  order.str_order = "Move";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.target.kind = net::TargetKind::kTerrain;
  order.target.b_cell = true;
  order.target.int4_cell_bits = cell_bits(int4_x, int4_y);
  order.target.u1_subcell = 0;
  return order;
}

net::OrderModel order_attack(std::uint32_t u4_subject, std::uint32_t u4_target) {
  net::OrderModel order;
  order.str_order = "Attack";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.target.kind = net::TargetKind::kActor;
  order.target.u4_actor = u4_target;
  order.target.int4_generation = 0;
  return order;
}

bool queue_has_done(const lch::GameSession& s, std::string_view str_type,
                    std::string_view str_item) {
  for (const auto& queue : s.w.prod.vec_players[s.u4_viewer].vec_queues) {
    if (queue.str_type != str_type) continue;
    for (const auto& item : queue.vec_queue)
      if (item.str_item == str_item && item.b_done) return true;
  }
  return false;
}

void test_parse_replay_flags() {
  const lch::CliParseResult res{lch::cli_args_parse(
      args({"--headless", "--ticks", "300", "--record", "run.orarep"}))};
  check(res.b_ok, "parse: --record value accepted");
  check(res.args.str_record == "run.orarep", "parse: record path");
  check(res.args.str_replay.empty(), "parse: replay empty");

  const lch::CliParseResult res2{
      lch::cli_args_parse(args({"--replay", "run.orarep"}))};
  check(res2.b_ok, "parse: --replay value accepted");
  check(res2.args.str_replay == "run.orarep", "parse: replay path");

  check(!lch::cli_args_parse(args({"--record", "a", "--replay", "b"})).b_ok,
        "parse: --record + --replay mutually exclusive");
  check(!lch::cli_args_parse(args({"--record"})).b_ok, "parse: record missing value rejected");
}

void test_session_record_replay() {
  namespace rep = ora::game::replay;
  constexpr std::int32_t kSeed{20261103};
  constexpr std::int32_t kTicks{240};

  std::vector<std::uint8_t> vec_file;
  std::string str_digest_record;
  {
    lch::SessionEnv env;
    env.int4_seed = kSeed;
    auto res_session{lch::session_setup(env)};
    check(res_session.has_value(), "replay-cli: record session setup");
    if (!res_session.has_value()) return;
    lch::GameSession& s{*res_session};

    rep::ReplayRecorderModel recorder;
    s.om.ptr_recorder = &recorder;
    recorder_receive(recorder, 0,
                     rep::make_boot_packet("SyncInfo", rep::replay_session_yaml(
                         rep::ReplaySessionInfo{.int4_seed = kSeed,
                                                .str_map_uid = "ora-sandbox",
                                                .int4_net_frame_interval = net::kNetFrameInterval,
                                                .int4_game_timestep = 40,
                                                .vec_client_indices = {net::kLocalClientId}})));
    recorder_receive(recorder, 0, rep::make_boot_packet("StartGame", ""));

    recorder_receive(recorder, net::kLocalClientId,
                     net::order_packet_serialize_frame(std::vector<std::uint8_t>{}, 1));

    const std::uint32_t u4_player_actor{s.w.vec_players[s.u4_viewer].u4_actor_id};
    std::uint32_t u4_mover{};
    std::uint32_t u4_target{};
    for (const std::uint32_t u4_id : s.w.vec_in_world) {
      const ww::ActorModel& actor{s.w.vec_actors_all[u4_id]};
      if (actor.str_type == "harv" && u4_mover == 0) u4_mover = u4_id;
      if (actor.str_type == "e1" && u4_target == 0 && u4_id != u4_mover) u4_target = u4_id;
    }

    for (std::int32_t int4_tick{0}; int4_tick < kTicks; ++int4_tick) {
      if (int4_tick == 4) lch::session_issue_order(s, order_start_production(u4_player_actor, "powr"));
      if (int4_tick == 8 && u4_mover != 0) lch::session_issue_order(s, order_move(u4_mover, 30, 20));
      if (int4_tick == 12 && u4_mover != 0 && u4_target != 0)
        lch::session_issue_order(s, order_attack(u4_mover, u4_target));
      lch::session_tick(s);
    }
    str_digest_record = lch::session_digest(s);

    rep::ReplayGameInfo info;
    info.str_mod = "ra";
    info.str_version = "ora-m1";
    info.str_map_uid = "ora-sandbox";
    info.str_map_title = "ora sandbox";
    info.int4_final_game_tick = s.w.int4_world_tick;
    info.str_start_time_utc = "2026-01-01 00-00-00";
    info.vec_players.push_back(rep::ReplayGameInfo::PlayerInfo{
        .int4_client_index = net::kLocalClientId, .str_name = "cli-test", .b_is_human = true});
    recorder_attach_metadata(recorder, rep::replay_metadata_nodes(info));
    auto res_file{recorder_dispose(recorder)};
    check(res_file.has_value(), "replay-cli: recorder dispose");
    if (!res_file.has_value()) return;
    vec_file = std::move(*res_file);
  }

  {
    auto res_read{rep::replay_read(vec_file)};
    check(res_read.has_value(), "replay-cli: file parses");
    if (res_read.has_value())
      check(rep::replay_write(*res_read) == vec_file, "replay-cli: read->write identity");
  }

  {
    auto res_read{rep::replay_read(vec_file)};
    check(res_read.has_value(), "replay-cli: replay parse");
    if (!res_read.has_value()) return;
    auto res_feed{rep::replay_feed_build(*res_read)};
    check(res_feed.has_value(), "replay-cli: feed build");
    if (!res_feed.has_value()) return;

    lch::SessionEnv env;
    env.int4_seed = res_feed->session.int4_seed;
    auto res_session{lch::session_setup(env)};
    check(res_session.has_value(), "replay-cli: replay session setup");
    if (!res_session.has_value()) return;
    lch::GameSession& s{*res_session};
    s.om.ptr_replay_feed = &*res_feed;

    const std::int32_t int4_ticks{res_feed->int4_tick_count - 1};
    std::size_t sz_orders_fed{};
    for (std::int32_t int4_tick{0}; int4_tick < int4_ticks; ++int4_tick) {
      while (sz_orders_fed < res_feed->vec_replayed_orders.size()) {
        const auto& [int4_frame, vec_bytes]{res_feed->vec_replayed_orders[sz_orders_fed++]};
        for (const net::OrderModel& order : net::order_packet_get_orders(vec_bytes, &s.w))
          s.vec_bridge_pending.emplace_back(static_cast<std::uint32_t>(int4_frame), order);
      }
      lch::session_tick(s);
    }

    check(res_feed->vec_desync_reports.empty(), "replay-cli: no desync reports");
    check(!s.om.b_out_of_sync, "replay-cli: om not out of sync");
    check(!res_feed->vec_replayed_orders.empty(),
          "replay-cli: orders replayed through bridge path");
    check(lch::session_digest(s) == str_digest_record,
          "replay-cli: final digest equals record (bridge replay included)");
  }
}

bool events_contain(const lch::GameSession& s, std::string_view str_prefix) {
  for (const std::string& str_event : s.vec_events)
    if (str_event.starts_with(str_prefix)) return true;
  return false;
}

void advance(lch::GameSession& s, int int4_ticks, const std::function<bool()>& fn_stop,
             std::vector<std::size_t>* ptr_trace) {
  for (int int4_i{0}; int4_i < int4_ticks; ++int4_i) {
    lch::session_tick(s);
    if (ptr_trace != nullptr)
      ptr_trace->push_back(std::hash<std::string>{}(lch::session_digest(s)));
    if (fn_stop != nullptr && fn_stop()) return;
  }
}

void run_script(lch::GameSession& s, std::vector<std::size_t>* ptr_trace,
                std::string* ptr_final_digest) {
  const std::uint32_t u4_player_actor{s.w.vec_players[s.u4_viewer].u4_actor_id};

  lch::session_issue_order(s, order_start_production(u4_player_actor, "powr"));
  advance(s, 240, [&] { return queue_has_done(s, "building", "powr"); }, ptr_trace);
  check(queue_has_done(s, "building", "powr"), "loop: powr build completes (~180 ticks)");

  lch::session_issue_order(s, order_place_building(u4_player_actor, "powr", 15, 23));
  advance(s, 4, nullptr, ptr_trace);
  check(events_contain(s, "placed powr id="), "loop: powr placed event");
  check(s.w.prod.vec_players[s.u4_viewer].power.int4_power_provided == 100,
        "loop: power provided 100 after powr");

  lch::session_issue_order(s, order_start_production(u4_player_actor, "barr"));
  advance(s, 360, [&] { return queue_has_done(s, "building", "barr"); }, ptr_trace);
  check(queue_has_done(s, "building", "barr"), "loop: barr build completes (~300 ticks)");

  lch::session_issue_order(s, order_place_building(u4_player_actor, "barr", 15, 27));
  advance(s, 4, nullptr, ptr_trace);
  check(events_contain(s, "placed barr id="), "loop: barr placed event");

  lch::session_issue_order(s, order_start_production(u4_player_actor, "e1"));
  advance(s, 140, [&] { return events_contain(s, "produced e1 id="); }, ptr_trace);
  check(events_contain(s, "produced e1 id="), "loop: e1 produced from barracks");
  bool b_infantry_enabled{false};
  for (const auto& queue : s.w.prod.vec_players[s.u4_viewer].vec_queues)
    if (queue.str_type == "infantry") b_infantry_enabled = queue.b_enabled;
  check(b_infantry_enabled, "loop: Infantry queue enabled after barracks");

  const std::uint32_t u4_e1_new{actor_id_of_owner(s, "e1", true, 1)};
  check(u4_e1_new != 0xffffffffu, "loop: produced e1 is own second e1");
  const std::uint32_t u4_e1_foe{actor_id_of_owner(s, "e1", false, 0)};
  check(u4_e1_foe != 0xffffffffu, "loop: enemy e1 present");
  const std::int32_t int4_pos_x_start{s.w.vec_actors_all[u4_e1_new].int4_pos_x};

  lch::session_issue_order(s, order_move(u4_e1_new, 10, 30));
  advance(s, 60, nullptr, ptr_trace);
  check(s.w.vec_actors_all[u4_e1_new].int4_pos_x != int4_pos_x_start ||
            s.w.vec_actors_all[u4_e1_new].int4_pos_y != 24,
        "loop: e1 moves after Move order");

  lch::session_issue_order(s, order_attack(u4_e1_new, u4_e1_foe));
  advance(s, 300, [&] { return events_contain(s, "killed "); }, ptr_trace);
  check(events_contain(s, "hit "), "loop: shots land (hp drops)");
  check(events_contain(s, "killed "), "loop: enemy e1 dies");
  check(s.w.vec_actors_all[u4_e1_foe].b_disposed, "loop: enemy e1 disposed in world");
  bool b_foe_still_in_world{false};
  for (const std::uint32_t u4_id : s.w.vec_in_world)
    if (u4_id == u4_e1_foe) b_foe_still_in_world = true;
  check(!b_foe_still_in_world, "loop: enemy e1 removed from in_world");

  const auto& res_final{s.w.prod.vec_players[s.u4_viewer].res};
  check(res_final.int4_resources > 0 || res_final.int4_cash > 4200,
        "loop: harvester delivered ore (economy loop)");

  if (ptr_final_digest != nullptr) *ptr_final_digest = lch::session_digest(s);
}

void test_gameplay_loops() {
  auto res_session{lch::session_setup(lch::SessionEnv{.int4_seed = 7, .str_map_path = {}, .opt_lobby = {}})};
  if (!res_session.has_value()) {
    check(false, std::format("loops: setup {}", res_session.error()));
    return;
  }
  lch::session_tick(*res_session);
  run_script(*res_session, nullptr, nullptr);
}

void test_determinism_double_run() {
  std::vector<std::size_t> vec_first;
  std::string str_first;
  {
    auto res_session{lch::session_setup(lch::SessionEnv{.int4_seed = 7, .str_map_path = {}, .opt_lobby = {}})};
    if (!res_session.has_value()) {
      check(false, "determinism: first setup failed");
      return;
    }
    lch::session_tick(*res_session);
    run_script(*res_session, &vec_first, &str_first);
  }

  std::vector<std::size_t> vec_second;
  std::string str_second;
  {
    auto res_session{lch::session_setup(lch::SessionEnv{.int4_seed = 7, .str_map_path = {}, .opt_lobby = {}})};
    if (!res_session.has_value()) {
      check(false, "determinism: second setup failed");
      return;
    }
    lch::session_tick(*res_session);
    run_script(*res_session, &vec_second, &str_second);
  }

  check(vec_first.size() == vec_second.size() && vec_first.size() > 600,
        "determinism: per-tick digest trace captured");
  bool b_trace_equal{vec_first.size() == vec_second.size()};
  if (b_trace_equal)
    for (std::size_t idx{0}; idx < vec_first.size(); ++idx)
      if (vec_first[idx] != vec_second[idx]) {
        b_trace_equal = false;
        std::println("  digest diverges at tick {}", idx);
        break;
      }
  check(b_trace_equal, "determinism: same-seed per-tick digest trace identical");
  check(str_first == str_second, "determinism: final full digest identical");
}

void test_parse_demo_flag() {
  const lch::CliParseResult res{lch::cli_args_parse(args({"--demo"}))};
  check(res.b_ok && res.args.b_demo, "parse: --demo flag accepted");
}

void test_low_power_negative() {
  auto res_session{lch::session_setup(lch::SessionEnv{.int4_seed = 11, .str_map_path = {}, .opt_lobby = {}})};
  if (!res_session.has_value()) {
    check(false, std::format("lowpower: setup {}", res_session.error()));
    return;
  }
  lch::GameSession& s{*res_session};
  lch::session_tick(s);

  {
    const std::vector<inp_view::ProductionQueueView> vec_views{lch::session_queue_views(s)};
    bool b_building_view{false};
    for (const auto& view : vec_views) {
      if (view.str_type != "Building") continue;
      b_building_view = view.b_enabled && view.vec_items.size() == 1 &&
                        view.vec_items[0] == "powr";
    }
    check(b_building_view, "lowpower: sidebar view shows only powr before anypower");
  }

  const std::uint32_t u4_player_actor{s.w.vec_players[s.u4_viewer].u4_actor_id};
  const auto& state{s.w.prod.vec_players[s.u4_viewer]};

  struct BuildStep {
    std::string_view str_item;
    std::string_view str_queue;
    std::int32_t int4_x;
    std::int32_t int4_y;
  };
  const std::vector<BuildStep> vec_steps{
      BuildStep{"powr", "building", 15, 23}, BuildStep{"barr", "building", 15, 27},
      BuildStep{"ftur", "defense", 14, 31},  BuildStep{"ftur", "defense", 17, 31},
      BuildStep{"ftur", "defense", 20, 31},  BuildStep{"ftur", "defense", 23, 31},
      BuildStep{"ftur", "defense", 26, 31}};
  for (const BuildStep& step : vec_steps) {
    lch::session_issue_order(s, order_start_production(u4_player_actor, step.str_item));
    for (int int4_i{0}; int4_i < 1100 && !queue_has_done(s, step.str_queue, step.str_item);
         ++int4_i)
      lch::session_tick(s);
    check(queue_has_done(s, step.str_queue, step.str_item),
          std::format("lowpower: {} build completes", step.str_item));
    if (!queue_has_done(s, step.str_queue, step.str_item)) break;
    lch::session_issue_order(
        s, order_place_building(u4_player_actor, step.str_item, step.int4_x, step.int4_y));
    for (int int4_i{0}; int4_i < 4; ++int4_i) lch::session_tick(s);
  }

  check(state.power.int4_power_provided == 100 && state.power.int4_power_drained == 120,
        "lowpower: 100/120 after powr+barr+5x ftur");
  check(std::string_view{opro::power_state_name(opro::power_state(
                             state.power.int4_power_provided,
                             state.power.int4_power_drained))} == "low",
        "lowpower: low state reached (100 > 120/2)");

  bool b_slowdown_seen{false};
  lch::session_issue_order(s, order_start_production(u4_player_actor, "ftur"));
  for (int int4_i{0}; int4_i < 40; ++int4_i) {
    lch::session_tick(s);
    for (const auto& queue : s.w.prod.vec_players[s.u4_viewer].vec_queues) {
      if (queue.str_type != "building" && queue.str_type != "defense") continue;
      for (const auto& item : queue.vec_queue)
        if (item.int4_slowdown > 0) b_slowdown_seen = true;
    }
  }
  check(b_slowdown_seen, "lowpower: token-bucket slowdown observed while low");
}

}

int main() {
  test_parse_defaults();
  test_parse_full();
  test_parse_errors();
  test_parse_flags();
  test_sandbox_recipe();
  test_session_setup();
  test_gameplay_loops();
  test_determinism_double_run();
  test_parse_demo_flag();
  test_low_power_negative();
  test_parse_replay_flags();
  test_session_record_replay();
  return ora::test::test_exit_code("cli_game");
}
