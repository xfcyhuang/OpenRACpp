import std;

import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;
import ora.test.util;
using ora::test::check;

#ifndef ORA_REPLAY_GOLDEN_DIR
#define ORA_REPLAY_GOLDEN_DIR "tests/golden/replay"
#endif

namespace {



void append_i4le(std::vector<std::uint8_t>& vec_out, std::int32_t int4_value) {
  const std::uint32_t u4{static_cast<std::uint32_t>(int4_value)};
  for (std::uint32_t idx{}; idx < 4; ++idx)
    vec_out.push_back(static_cast<std::uint8_t>((u4 >> (8 * idx)) & 0xff));
}

void append_record(std::vector<std::uint8_t>& vec_out, std::int32_t int4_client,
                   std::span<const std::uint8_t> span_data) {
  append_i4le(vec_out, int4_client);
  append_i4le(vec_out, static_cast<std::int32_t>(span_data.size()));
  vec_out.insert(vec_out.end(), span_data.begin(), span_data.end());
}

std::string to_hex(std::span<const std::uint8_t> span_bytes) {
  std::string str_out;
  for (const std::uint8_t u1 : span_bytes) str_out += std::format("{:02x}", u1);
  return str_out;
}

std::vector<std::uint8_t> read_file_bytes(std::string_view str_path) {
  std::ifstream fs{std::string{str_path}, std::ios::binary};
  if (!fs) return {};
  fs.seekg(0, std::ios::end);
  std::vector<std::uint8_t> vec_out(static_cast<std::size_t>(fs.tellg()));
  fs.seekg(0, std::ios::beg);
  fs.read(reinterpret_cast<char*>(vec_out.data()),
          static_cast<std::streamsize>(vec_out.size()));
  return vec_out;
}

std::vector<std::uint8_t> make_minimal_orarep() {
  using ora::game::replay::kMetaEndMarker;
  using ora::game::replay::kMetaStartMarker;
  using ora::game::replay::kMetaVersion;
  std::vector<std::uint8_t> vec_bytes;

  const std::vector<std::uint8_t> vec_boot{0x00, 0x00, 0x00, 0x00, 0xff, 0x04, 'T', 'e',
                                           's',  't',  0x00, 0x00};
  append_record(vec_bytes, 0, vec_boot);

  const std::vector<std::uint8_t> vec_pacing{0x03, 0x00, 0x00, 0x00};
  append_record(vec_bytes, 1, vec_pacing);

  const std::string str_yaml{"Root:\n\tMod: ra\n\tFinalGameTick: 580\n"};
  const std::vector<std::uint8_t> vec_yaml{str_yaml.begin(), str_yaml.end()};
  append_i4le(vec_bytes, kMetaStartMarker);
  append_i4le(vec_bytes, kMetaVersion);
  append_i4le(vec_bytes, static_cast<std::int32_t>(vec_yaml.size()));
  vec_bytes.insert(vec_bytes.end(), vec_yaml.begin(), vec_yaml.end());
  append_i4le(vec_bytes, static_cast<std::int32_t>(4 + vec_yaml.size()));
  append_i4le(vec_bytes, kMetaEndMarker);
  return vec_bytes;
}

void test_format_roundtrip_minimal() {
  using ora::game::replay::ReplayFile;
  using ora::game::replay::replay_read;
  const std::vector<std::uint8_t> vec_bytes{make_minimal_orarep()};

  const auto res_read{replay_read(vec_bytes)};
  check(res_read.has_value(), "minimal: replay_read succeeds");
  if (!res_read.has_value()) {
    std::println("  error: {}", res_read.error());
    return;
  }
  const ReplayFile& file{*res_read};

  check(file.vec_records.size() == 2, std::format("minimal: 2 records (got {})",
                                                  file.vec_records.size()));
  if (file.vec_records.size() == 2) {
    check(file.vec_records[0].int4_client == 0 && file.vec_records[0].vec_data.size() == 12,
          "minimal: record 0 client/data");
    check(file.vec_records[1].int4_client == 1 && file.vec_records[1].vec_data.size() == 4,
          "minimal: record 1 client/data");
    check(file.vec_records[0].idx_offset == 0, "minimal: record 0 offset 0");
    check(file.vec_records[1].idx_offset == 8 + 12, "minimal: record 1 offset 20");
  }
  check(file.b_has_metadata, "minimal: metadata present");
  check(file.int4_final_game_tick == 580,
        std::format("minimal: FinalGameTick 580 (got {})", file.int4_final_game_tick));

  const std::vector<std::uint8_t> vec_written{replay_write(file)};
  check(vec_written == vec_bytes,
        std::format("minimal: read->write byte identity\n  want {}\n  got  {}",
                    to_hex(vec_bytes), to_hex(vec_written)));
}

void test_format_corrupt() {
  using ora::game::replay::replay_read;

  std::vector<std::uint8_t> vec_no_meta;
  const std::vector<std::uint8_t> vec_frame1{0x01, 0x00, 0x00, 0x00};
  append_record(vec_no_meta, 1, vec_frame1);
  check(!replay_read(vec_no_meta).has_value(), "corrupt: no-metadata block rejected");

  std::vector<std::uint8_t> vec_trunc{0x01, 0x00, 0x00};
  check(!replay_read(vec_trunc).has_value(), "corrupt: truncated header rejected");

  std::vector<std::uint8_t> vec_bad_tail{make_minimal_orarep()};
  vec_bad_tail.back() = 0xfd;
  check(!replay_read(vec_bad_tail).has_value(), "corrupt: bad end marker rejected");

  std::vector<std::uint8_t> vec_bad_len{make_minimal_orarep()};
  vec_bad_len[vec_bad_len.size() - 8] = 0x99;
  check(!replay_read(vec_bad_len).has_value(), "corrupt: bad dataLength rejected");
}

void test_format_golden() {
  using ora::game::replay::ReplayRecord;
  using ora::game::replay::replay_read;
  const std::vector<std::uint8_t> vec_bytes{
      read_file_bytes(std::format("{}/ore-lord.orarep", ORA_REPLAY_GOLDEN_DIR))};
  check(!vec_bytes.empty(), "golden: file readable");
  if (vec_bytes.empty()) return;

  const auto res_read{replay_read(vec_bytes)};
  check(res_read.has_value(), "golden: replay_read succeeds");
  if (!res_read.has_value()) {
    std::println("  error: {}", res_read.error());
    return;
  }

  check(res_read->vec_records.size() == 1163,
        std::format("golden: 1163 records (got {})", res_read->vec_records.size()));
  check(res_read->int4_final_game_tick == 580,
        std::format("golden: FinalGameTick 580 (got {})", res_read->int4_final_game_tick));

  const std::vector<std::uint8_t>& vec_boot0{res_read->vec_records[0].vec_data};
  const std::vector<std::uint8_t>& vec_boot1{res_read->vec_records[1].vec_data};
  check(vec_boot0.size() > 14 && vec_boot0[0] == 0 && vec_boot0[4] == 0xff &&
            vec_boot0[5] == 8 &&
            std::string_view{reinterpret_cast<const char*>(vec_boot0.data() + 6), 8} == "SyncInfo",
        "golden: boot 0 is SyncInfo");
  check(vec_boot1.size() > 14 && vec_boot1[0] == 0 && vec_boot1[4] == 0xff &&
            vec_boot1[5] == 9 &&
            std::string_view{reinterpret_cast<const char*>(vec_boot1.data() + 6), 9} == "StartGame",
        "golden: boot 1 is StartGame");

  const ReplayRecord& rec_first{res_read->vec_records[2]};
  check(rec_first.int4_client == 1 && rec_first.vec_data.size() == 4 &&
            rec_first.vec_data[0] == 1,
        "golden: first pacing packet [1][]");

  const ReplayRecord& rec_sync{res_read->vec_records[4]};
  check(rec_sync.vec_data.size() == 17 && rec_sync.vec_data[4] == 0x65,
        "golden: sync packet 17B/0x65");

  const std::vector<std::uint8_t> vec_written{replay_write(*res_read)};
  check(vec_written == vec_bytes,
        std::format("golden: read->write byte identity ({} vs {} bytes)", vec_bytes.size(),
                    vec_written.size()));

  const std::vector<std::uint8_t> vec_stream{
      read_file_bytes(std::format("{}/ore-lord.record-stream.bin", ORA_REPLAY_GOLDEN_DIR))};
  check(vec_stream.size() == 22533, std::format("golden: record stream 22533B (got {})",
                                                vec_stream.size()));
  std::size_t idx_meta{};
  for (const ReplayRecord& record : res_read->vec_records) idx_meta += 8 + record.vec_data.size();
  check(idx_meta == vec_stream.size() && vec_stream.size() <= vec_bytes.size() &&
            vec_stream == std::vector<std::uint8_t>{vec_bytes.begin(),
                                                    vec_bytes.begin() + idx_meta},
        "golden: record stream prefix matches");
}

namespace scenario {

namespace ww = ora::game::world;
namespace net = ora::game::network;
namespace rep = ora::game::replay;

constexpr std::int32_t kSeed{20261103};
constexpr std::int32_t kFrames{60};

constexpr std::string_view kRules{
    "world:\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "\tRngProbe:\n"
    "player:\n"
    "\tClassicProductionQueue@Building:\n"
    "\t\tType: Building\n"
    "\t\tLowPowerModifier: 300\n"
    "\tClassicProductionQueue@Infantry:\n"
    "\t\tType: Infantry\n"
    "\t\tLowPowerModifier: 300\n"
    "\tPlaceBuilding:\n"
    "\tPlayerResources:\n"
    "\tPowerManager:\n"
    "\tProvidesTechPrerequisite@unrestricted:\n"
    "\t\tId: unrestricted\n"
    "\t\tPrerequisites: techlevel.infonly, techlevel.low, techlevel.medium, techlevel.high, techlevel.unrestricted\n"
    "mpspawn:\n"
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
    "\tProvidesPrerequisite:\n"
    "\t\tPrerequisite: barracks\n"
    "\tProvidesPrerequisite@buildingname:\n"
    "e1:\n"
    "\tValued:\n"
    "\t\tCost: 100\n"
    "\tBuildable:\n"
    "\t\tQueue: Infantry\n"
    "\t\tBuildAtProductionType: Soldier\n"
    "\t\tPrerequisites: ~barracks, ~techlevel.infonly\n"
    "probe.rng:\n"
    "\tRngProbe:\n"
    "probe.sync:\n"
    "\tSyncProbe:\n"};

constexpr std::string_view kMap{
    "Title: t16-replay-scenario\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Creeps:\n"
    "\t\tName: Creeps\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: soviet\n"
    "\tPlayerReference@Multi0:\n"
    "\t\tName: Multi0\n"
    "\t\tPlayable: True\n"
    "\t\tFaction: Random\n"
    "Actors:\n"
    "\trng0: probe.rng\n"
    "\t\tLocation: 50,60\n"
    "\t\tOwner: Neutral\n"
    "\tsync0: probe.sync\n"
    "\t\tLocation: 60,70\n"
    "\t\tOwner: Creeps\n"
    "\tfact0: fact\n"
    "\t\tLocation: 30,30\n"
    "\t\tOwner: Multi0\n"
    "\tbarr0: barr\n"
    "\t\tLocation: 33,34\n"
    "\t\tOwner: Multi0\n"
    "\tatk0: e1\n"
    "\t\tLocation: 25,10\n"
    "\t\tOwner: Multi0\n"
    "\tvic0: e1\n"
    "\t\tLocation: 28,12\n"
    "\t\tOwner: Creeps\n"};

std::int32_t cell_bits(std::int32_t int4_x, std::int32_t int4_y) {
  return static_cast<std::int32_t>(((int4_x & 0xfff) << 20) | ((int4_y & 0xfff) << 8));
}

std::expected<ww::WorldModel, std::string> make_world(std::int32_t int4_seed) {
  const auto res_map{ww::synthetic_map_from_yaml(kRules, kMap)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  ora::game::production::LobbyModel lobby;
  lobby.vec_playable_factions.emplace_back("Multi0", "russia");
  lobby.int4_starting_cash = 5000;
  lobby.str_techlevel = "unrestricted";
  return ww::world_create(*res_map, int4_seed, lobby);
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
  return order;
}

net::OrderModel order_start_production(std::uint32_t u4_subject, std::string_view str_item,
                                       std::uint32_t u4_count) {
  net::OrderModel order;
  order.str_order = "StartProduction";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.opt_str_target = std::string{str_item};
  order.u4_extra_data = u4_count;
  return order;
}

net::OrderModel order_place_building(std::uint32_t u4_subject, std::string_view str_item,
                                     std::int32_t int4_x, std::int32_t int4_y) {
  net::OrderModel order;
  order.str_order = "PlaceBuilding";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.opt_str_target = std::string{str_item};
  order.target.kind = net::TargetKind::kTerrain;
  order.target.b_cell = true;
  order.target.int4_cell_bits = cell_bits(int4_x, int4_y);
  order.target.u1_subcell = 0;
  return order;
}

net::OrderModel order_chat(std::int32_t int4_client) {
  net::OrderModel order;
  order.str_order = "Chat";
  order.b_is_immediate = true;
  order.opt_str_target = "hello replay";
  order.u4_extra_data = static_cast<std::uint32_t>(int4_client);
  return order;
}

std::string actor_dump_line(const ww::WorldModel& w) {
  std::string str_line;
  for (const std::uint32_t u4_id : w.vec_in_world)
    str_line += std::format("{}:{},", u4_id, w.vec_actors_all[u4_id].str_type);
  return str_line;
}

std::uint32_t u4_owner_of(const ww::WorldModel& w, std::string_view str_name) {
  for (std::size_t idx{}; idx < w.vec_players.size(); ++idx)
    if (w.vec_players[idx].str_internal_name == str_name)
      return static_cast<std::uint32_t>(idx);
  return 0;
}

struct ScenarioResult {
  std::vector<std::uint8_t> vec_file;
  std::string str_dump;
  ora::game::replay::ReplayFeedModel feed;
  bool b_out_of_sync{};
};

std::expected<ScenarioResult, std::string> record_run() {
  auto res_world{make_world(kSeed)};
  if (!res_world.has_value()) return std::unexpected(res_world.error());
  ww::WorldModel w{std::move(*res_world)};
  net::OrderManagerModel om;

  rep::ReplayRecorderModel recorder;
  om.ptr_recorder = &recorder;
  const std::vector<std::int32_t> vec_clients{net::kLocalClientId};

  const std::string str_session{rep::replay_session_yaml(
      rep::ReplaySessionInfo{.int4_seed = kSeed,
                             .str_map_uid = "t16-replay-scenario",
                             .int4_net_frame_interval = 1,
                             .int4_game_timestep = 40,
                             .vec_client_indices = {1}})};
  recorder_receive(recorder, 0, rep::make_boot_packet("SyncInfo", str_session));
  recorder_receive(recorder, 0, rep::make_boot_packet("StartGame", ""));

  net::order_manager_start_game(om, w, vec_clients);

  std::uint32_t u4_atk{};
  std::uint32_t u4_vic{};
  std::uint32_t u4_player_actor{};
  const std::uint32_t u4_multi0{u4_owner_of(w, "Multi0")};
  const std::uint32_t u4_creeps{u4_owner_of(w, "Creeps")};
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const ww::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.str_type == "e1" && actor.u4_owner == u4_multi0) u4_atk = u4_id;
    if (actor.str_type == "e1" && actor.u4_owner == u4_creeps) u4_vic = u4_id;
  }
  for (const ww::PlayerModel& player : w.vec_players)
    if (player.b_playable) u4_player_actor = player.u4_actor_id;

  std::string str_dump;
  str_dump += std::format("# run frames {} actors {}\n", kFrames, w.vec_in_world.size());
  for (std::int32_t int4_frame{1}; int4_frame <= kFrames; ++int4_frame) {
    switch (int4_frame) {
      case 3:
        net::issue_order(om, order_move(u4_atk, 27, 9));
        break;
      case 5:
        net::issue_order(om, order_start_production(u4_player_actor, "powr", 1));
        break;
      case 8:
        net::issue_order(om, order_start_production(u4_player_actor, "e1", 1));
        break;
      case 12:
        net::issue_order(om, order_attack(u4_atk, u4_vic));
        break;
      case 20:
        net::issue_order(om, order_chat(1));
        break;
      case 30:
        net::issue_order(om, order_place_building(u4_player_actor, "powr", 35, 40));
        break;
      default:
        break;
    }
    net::run_frame(om, w);

    check(!om.vec_echo_sync.empty(),
          std::format("record: frame {} produced sync (fatal='{}')", int4_frame, om.str_fatal));
    const auto opt_sync{net::try_parse_sync_packet(om.vec_echo_sync.back())};
    check(opt_sync.has_value() && opt_sync->int4_frame == int4_frame,
          std::format("record: frame {} sync frame tag", int4_frame));
    const std::int32_t int4_hash{opt_sync.has_value() ? opt_sync->int4_sync_hash : -1};
    str_dump += std::format("f {} sync {} rng {} {} actors {} | {}\n", int4_frame, int4_hash,
                            w.rng_shared.int4_last, w.rng_shared.int4_total_count,
                            w.vec_in_world.size(), actor_dump_line(w));
  }

  rep::ReplayGameInfo info;
  info.str_mod = "ra";
  info.str_version = "t16-test";
  info.str_map_uid = "t16-replay-scenario";
  info.str_map_title = "T16 Replay Scenario";
  info.int4_final_game_tick = w.int4_world_tick;
  info.str_start_time_utc = "2026-01-01 00-00-00";
  info.vec_players.push_back(rep::ReplayGameInfo::PlayerInfo{.int4_client_index = 1,
                                                             .str_name = "T16Host",
                                                             .b_is_human = true});
  recorder_attach_metadata(recorder, rep::replay_metadata_nodes(info));

  auto res_file{recorder_dispose(recorder)};
  if (!res_file.has_value()) return std::unexpected(res_file.error());
  return ScenarioResult{*res_file, std::move(str_dump), {}, false};
}

std::expected<ScenarioResult, std::string> replay_run(std::span<const std::uint8_t> span_file) {
  auto res_read{rep::replay_read(span_file)};
  if (!res_read.has_value()) return std::unexpected(res_read.error());
  auto res_feed{rep::replay_feed_build(*res_read)};
  if (!res_feed.has_value()) return std::unexpected(res_feed.error());

  auto res_world{make_world(res_feed->session.int4_seed)};
  if (!res_world.has_value()) return std::unexpected(res_world.error());
  ww::WorldModel w{std::move(*res_world)};
  net::OrderManagerModel om;
  om.ptr_replay_feed = &*res_feed;
  const std::vector<std::int32_t> vec_clients{net::kLocalClientId};
  net::order_manager_start_game(om, w, vec_clients);

  std::string str_dump;
  str_dump += std::format("# run frames {} actors {}\n", kFrames, w.vec_in_world.size());
  for (std::int32_t int4_frame{1}; int4_frame <= kFrames; ++int4_frame) {
    net::run_frame(om, w);
    if (om.vec_echo_sync.empty()) {
      std::println("replay: frame {} stalled (no local sync), fatal='{}'", int4_frame,
                   om.str_fatal);
      break;
    }
    const auto opt_sync{net::try_parse_sync_packet(om.vec_echo_sync.back())};
    const std::int32_t int4_hash{opt_sync.has_value() ? opt_sync->int4_sync_hash : -1};
    str_dump += std::format("f {} sync {} rng {} {} actors {} | {}\n", int4_frame, int4_hash,
                            w.rng_shared.int4_last, w.rng_shared.int4_total_count,
                            w.vec_in_world.size(), actor_dump_line(w));
  }

  net::tick_immediate(om, w);

  ScenarioResult result;
  result.vec_file.assign(span_file.begin(), span_file.end());
  result.str_dump = std::move(str_dump);
  result.feed = std::move(*res_feed);
  result.b_out_of_sync = om.b_out_of_sync;
  return result;
}

}

void test_record_layout() {
  using scenario::kFrames;
  using scenario::record_run;
  const auto res_run{record_run()};
  check(res_run.has_value(), "record: run succeeds");
  if (!res_run.has_value()) {
    std::println("  error: {}", res_run.error());
    return;
  }

  const auto res_read{ora::game::replay::replay_read(res_run->vec_file)};
  check(res_read.has_value(), "record: file parses");
  if (!res_read.has_value()) return;

  check(res_read->vec_records.size() == 2 + 1 + kFrames + 1 + kFrames,
        std::format("record: 124 records (got {})", res_read->vec_records.size()));
  check(res_read->int4_final_game_tick == kFrames, "record: FinalGameTick 60");

  const auto& vec_records{res_read->vec_records};

  check(vec_records[0].int4_client == 0 && vec_records[1].int4_client == 0, "record: boot from=0");

  check(vec_records[2].int4_client == 1 && vec_records[2].vec_data.size() == 4 &&
            vec_records[2].vec_data[0] == 1,
        "record: [1] pacing packet");

  bool b_found_move{};
  for (const auto& record : vec_records) {
    if (record.int4_client != 1 || record.vec_data.size() < 6) continue;
    const std::span<const std::uint8_t> span_data{record.vec_data};
    if (span_data.size() > 4 && span_data[4] == 0xff && span_data[5] == 4 &&
        std::string_view{reinterpret_cast<const char*>(span_data.data() + 6), 4} == "Move") {
      const std::int32_t int4_frame{static_cast<std::int32_t>(static_cast<std::uint32_t>(
                                        span_data[0]) |
                                    (static_cast<std::uint32_t>(span_data[1]) << 8))};
      check(int4_frame == 4, std::format("record: Move applied at frame 4 (got {})", int4_frame));
      b_found_move = true;
      break;
    }
  }
  check(b_found_move, "record: Move order present");

  bool b_found_chat{};
  for (const auto& record : vec_records) {
    if (record.int4_client != 1 || record.vec_data.size() < 6) continue;
    const std::span<const std::uint8_t> span_data{record.vec_data};
    if (span_data[4] == 0xff && span_data[5] == 4 &&
        std::string_view{reinterpret_cast<const char*>(span_data.data() + 6), 4} == "Chat") {
      check(span_data[0] == 0 && span_data[1] == 0 && span_data[2] == 0 && span_data[3] == 0,
            "record: Chat immediate at [0]");
      b_found_chat = true;
      break;
    }
  }
  check(b_found_chat, "record: Chat immediate order present");

  std::int32_t int4_sync_count{};
  for (const auto& record : vec_records) {
    if (record.vec_data.size() == 17 && record.vec_data[4] == 0x65) ++int4_sync_count;
  }
  check(int4_sync_count == kFrames, std::format("record: 60 sync packets (got {})",
                                                int4_sync_count));

  const std::vector<std::uint8_t> vec_written{ora::game::replay::replay_write(*res_read)};
  check(vec_written == res_run->vec_file, "record: self-recorded read->write identity");
}

void test_replay_self_consistent() {
  using scenario::kFrames;
  using scenario::kSeed;
  using scenario::replay_run;
  using scenario::record_run;
  const auto res_a{record_run()};
  const auto res_b{record_run()};
  check(res_a.has_value() && res_b.has_value(), "replay: two record runs succeed");
  if (!res_a.has_value() || !res_b.has_value()) return;

  const auto res_read_a{ora::game::replay::replay_read(res_a->vec_file)};
  const auto res_read_b{ora::game::replay::replay_read(res_b->vec_file)};
  check(res_read_a.has_value() && res_read_b.has_value(), "replay: both files parse");
  if (!res_read_a.has_value() || !res_read_b.has_value()) return;
  check(res_read_a->vec_records.size() == res_read_b->vec_records.size(),
        "replay: record counts match");
  bool b_identical{res_read_a->vec_records.size() == res_read_b->vec_records.size()};
  for (std::size_t idx{}; b_identical && idx < res_read_a->vec_records.size(); ++idx)
    b_identical = res_read_a->vec_records[idx].int4_client ==
                      res_read_b->vec_records[idx].int4_client &&
                  res_read_a->vec_records[idx].vec_data == res_read_b->vec_records[idx].vec_data;
  check(b_identical, "record-stream-identical (double run)");
  check(res_a->str_dump == res_b->str_dump, "record-dump-identical (double run)");

  auto res_replay{replay_run(res_a->vec_file)};
  check(res_replay.has_value(), "replay: replay run succeeds");
  if (!res_replay.has_value()) {
    std::println("  error: {}", res_replay.error());
    return;
  }
  check(res_replay->str_dump == res_a->str_dump, "replay: per-frame dump equals record");
  check(!res_replay->b_out_of_sync, "replay: om out-of-sync flag false");
  check(res_replay->feed.vec_desync_reports.empty(), "replay: no desync reports");

  const ora::game::replay::ReplayFeedModel& feed{res_replay->feed};
  std::int32_t int4_mismatches{};
  std::string str_first;
  for (const auto& [int4_frame, pair_recorded] : feed.map_recorded_syncs) {
    const auto it_local{feed.map_local_syncs.find(int4_frame)};
    if (it_local == feed.map_local_syncs.end() || it_local->second != pair_recorded) {
      ++int4_mismatches;
      if (str_first.empty())
        str_first = std::format("first mismatch at frame {}: recorded ({},{})", int4_frame,
                                pair_recorded.first, pair_recorded.second);
    }
  }
  check(int4_mismatches == 0,
        std::format("replay: 0 sync mismatches (got {}, {})", int4_mismatches, str_first));
  check(static_cast<std::int32_t>(feed.map_local_syncs.size()) == kFrames,
        std::format("replay: 60 local syncs (got {})", feed.map_local_syncs.size()));
  check(feed.int4_tick_count == kFrames + 1,
        std::format("replay: TickCount 61 (got {})", feed.int4_tick_count));
  check(feed.b_valid, "replay: feed valid");
  check(feed.session.int4_seed == kSeed, "replay: seed from file session");
  check(feed.session.int4_net_frame_interval == 1,
        "replay: NetFrameInterval from file session");
}

std::vector<std::uint8_t> tamper_copy(std::span<const std::uint8_t> span_file,
                                      std::string_view str_name,
                                      void (*fn_mutate)(std::vector<std::uint8_t>&,
                                                        const ora::game::replay::ReplayRecord&)) {
  auto res_read{ora::game::replay::replay_read(span_file)};
  if (!res_read.has_value()) return {};
  for (const auto& record : res_read->vec_records) {
    if (record.int4_client != 1 || record.vec_data.size() < 6) continue;
    if (record.vec_data[4] != 0xff) continue;

    const std::size_t sz_len{record.vec_data[5]};
    if (record.vec_data.size() < 6 + sz_len) continue;
    if (std::string_view{reinterpret_cast<const char*>(record.vec_data.data() + 6), sz_len} !=
        str_name)
      continue;
    std::vector<std::uint8_t> vec_out{span_file.begin(), span_file.end()};
    fn_mutate(vec_out, record);
    return vec_out;
  }
  return {};
}

void test_tamper_detect() {
  using scenario::record_run;
  using scenario::replay_run;
  const auto res_record{record_run()};
  check(res_record.has_value(), "tamper: record run succeeds");
  if (!res_record.has_value()) return;
  const std::span<const std::uint8_t> span_file{res_record->vec_file};

  const auto vec_subject{tamper_copy(
      span_file, "StartProduction",
      [](std::vector<std::uint8_t>& vec_bytes, const ora::game::replay::ReplayRecord& record) {

        const std::size_t idx_subject{record.idx_offset + 8 + 6 + record.vec_data[5] + 2};
        vec_bytes[idx_subject] ^= 0x01;
      })};
  check(!vec_subject.empty(), "tamper: subject target found");

  const auto vec_prodname{tamper_copy(
      span_file, "StartProduction",
      [](std::vector<std::uint8_t>& vec_bytes, const ora::game::replay::ReplayRecord& record) {
        const std::span<const std::uint8_t> span_data{record.vec_data};
        for (std::size_t idx{6}; idx + 4 <= span_data.size(); ++idx)
          if (span_data[idx] == 'p' && span_data[idx + 1] == 'o' && span_data[idx + 2] == 'w' &&
              span_data[idx + 3] == 'r') {
            vec_bytes[record.idx_offset + 8 + idx] = 'q';
            return;
          }
      })};
  check(!vec_prodname.empty(), "tamper: prodname target found");

  std::vector<std::uint8_t> vec_sync{res_record->vec_file};
  {
    auto res_read{ora::game::replay::replay_read(span_file)};
    bool b_done{};
    for (const auto& record : res_read->vec_records) {
      if (b_done) break;
      if (record.vec_data.size() == 17 && record.vec_data[4] == 0x65 &&
          record.vec_data[0] == 20) {
        vec_sync[record.idx_offset + 8 + 6] ^= 0xff;
        b_done = true;
      }
    }
    check(b_done, "tamper: sync frame-20 packet found");
  }

  struct TamperCase {
    std::string_view str_name;
    std::vector<std::uint8_t> vec_bytes;
  };
  for (const TamperCase& cs : {TamperCase{"order-subject", vec_subject},
                               TamperCase{"prod-name", vec_prodname},
                               TamperCase{"sync-hash", vec_sync}}) {
    if (cs.vec_bytes.empty()) continue;
    auto res_replay{replay_run(cs.vec_bytes)};
    bool b_detected{false};
    std::string str_detail;
    if (!res_replay.has_value()) {
      b_detected = true;
      str_detail = res_replay.error();
    } else {

      b_detected = res_replay->b_out_of_sync || !res_replay->feed.vec_desync_reports.empty();
      if (!res_replay->feed.vec_desync_reports.empty())
        str_detail = res_replay->feed.vec_desync_reports.front();
      else if (res_replay->b_out_of_sync)
        str_detail = "om out-of-sync flag set";
    }
    check(b_detected, std::format("tamper {}: detected ({})", cs.str_name, str_detail));
  }
}

}

int main() {
  test_format_roundtrip_minimal();
  test_format_corrupt();
  test_format_golden();
  test_record_layout();
  test_replay_self_consistent();
  test_tamper_detect();

  return ora::test::test_exit_code("replay");
}
