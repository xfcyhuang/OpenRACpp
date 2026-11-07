import std;
#include <cstdio>
#include <cstdlib>

#include "game/network/order.hpp"
#include "game/network/order_packet.hpp"
#include "game/replay/replay_format.hpp"
#include "game/replay/replay_player.hpp"
#include "game/replay/replay_world.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_REPLAY_GOLDEN_DIR
#define ORA_REPLAY_GOLDEN_DIR "tests/golden/replay"
#endif
#ifndef ORA_MAPS_GOLDEN_DIR
#define ORA_MAPS_GOLDEN_DIR "tests/golden/maps"
#endif

namespace {



std::vector<std::uint8_t> read_file_bytes(std::string_view str_path) {
  std::ifstream fs{std::string{str_path}, std::ios::binary};
  if (!fs) return {};
  fs.seekg(0, std::ios::end);
  std::vector<std::uint8_t> vec_out(static_cast<std::size_t>(fs.tellg()));
  fs.seekg(0, std::ios::beg);
  fs.read(reinterpret_cast<char*>(vec_out.data()), static_cast<std::streamsize>(vec_out.size()));
  return vec_out;
}

std::string golden_init_line() {
  std::ifstream fs{std::format("{}/ore-lord.frames.txt", ORA_REPLAY_GOLDEN_DIR)};
  std::string str_line;
  while (std::getline(fs, str_line))
    if (str_line.starts_with("init actors ")) return str_line;
  return {};
}

std::int32_t run_oracle_replay(const std::vector<std::uint8_t>& vec_orarep,
                               const std::vector<std::uint8_t>& vec_oramap,
                               const ora::game::replay::OracleReplayContext& ctx) {
  using ora::game::replay::ReplayOracleWorld;
  using ora::game::replay::ReplayRecord;
  using ora::game::replay::replay_oracle_create;
  using ora::game::replay::replay_read;

  const auto res_read{replay_read(vec_orarep)};
  if (!res_read.has_value()) return -1;
  const auto res_feed{replay_feed_build(*res_read)};
  if (!res_feed.has_value()) return -1;
  std::map<std::int32_t, std::vector<ora::game::network::OrderModel>> map_orders;
  for (const ReplayRecord& record : res_read->vec_records) {
    const auto opt_orders{ora::game::network::try_parse_order_packet(record.vec_data)};
    if (!opt_orders.has_value() || opt_orders->int4_frame == 0) continue;
    for (const auto& order :
         ora::game::network::order_packet_get_orders(opt_orders->vec_order_bytes, nullptr))
      map_orders[opt_orders->int4_frame].push_back(order);
  }
  const auto res_world{replay_oracle_create(res_feed->session.int4_seed, vec_oramap)};
  if (!res_world.has_value()) return -1;
  ReplayOracleWorld w{*res_world};
  for (std::int32_t int4_f{1}; int4_f <= 580; ++int4_f) {
    if (int4_f >= 2)
      for (const ora::game::network::OrderModel& order : map_orders[int4_f])
        replay_oracle_apply_order(w, order);
    if (int4_f == 1)
      replay_oracle_frame1(w);
    else
      replay_oracle_frame(w, ctx, int4_f);
    const auto it_sync{res_feed->map_recorded_syncs.find(int4_f + 1)};
    if (it_sync == res_feed->map_recorded_syncs.end()) continue;
    if (replay_oracle_sync_hash(w) != it_sync->second.first) return int4_f + 1;
  }
  return 0;
}

void test_assembly_and_frame1() {
  using ora::game::replay::ReplayOracleWorld;
  using ora::game::replay::ReplayRecord;
  using ora::game::replay::ReplaySessionInfo;
  using ora::game::replay::replay_oracle_context_parse;
  using ora::game::replay::replay_oracle_create;
  using ora::game::replay::replay_read;

  const std::vector<std::uint8_t> vec_orarep{
      read_file_bytes(std::format("{}/ore-lord.orarep", ORA_REPLAY_GOLDEN_DIR))};
  check(!vec_orarep.empty(), "golden orarep readable");
  if (vec_orarep.empty()) return;

  const auto res_read{replay_read(vec_orarep)};
  check(res_read.has_value(), "orarep parses");
  if (!res_read.has_value()) return;
  const auto res_feed{replay_feed_build(*res_read)};
  check(res_feed.has_value(), "feed builds");
  if (!res_feed.has_value()) return;
  const ReplaySessionInfo& session{res_feed->session};
  check(session.int4_seed == 20261103, "seed 20261103 from file session");
  check(session.int4_net_frame_interval == 1, "NetFrameInterval 1 from file session");

  const std::vector<std::uint8_t> vec_oramap{
      read_file_bytes(std::format("{}/ore-lord.oramap", ORA_MAPS_GOLDEN_DIR))};
  const auto res_world{replay_oracle_create(session.int4_seed, vec_oramap)};
  check(res_world.has_value(), "replay world creates");
  if (!res_world.has_value()) {
    std::println("  error: {}", res_world.error());
    return;
  }
  const ReplayOracleWorld& w{*res_world};

  const std::string str_golden_init{golden_init_line()};
  check(!str_golden_init.empty(), "golden init line present");
  bool b_order_ok{w.vec_actors.size() == 149};
  if (b_order_ok) {
    std::size_t idx_next{std::string{"init actors "}.size()};
    for (std::size_t idx{}; idx < 149; ++idx) {
      const std::size_t idx_space{str_golden_init.find(' ', idx_next)};
      const std::string str_want{str_golden_init.substr(idx_next, idx_space - idx_next)};
      idx_next = idx_space + 1;
      const std::string str_got{std::format(
          "{}:{}:{}", idx, w.vec_actors[idx].str_type,
          w.vec_actors[idx].str_type == "world"
              ? std::string{"Neutral"}
              : w.vec_players[w.vec_actors[idx].u4_owner].str_name)};
      if (str_got != str_want) {
        check(false, std::format("assembly order actor {}: got {} want {}", idx, str_got, str_want));
        b_order_ok = false;
        break;
      }
    }
  } else {
    check(false, std::format("assembly 149 actors (got {})", w.vec_actors.size()));
  }
  if (b_order_ok) check(true, "assembly order matches golden init");

  const std::int32_t int4_hash{replay_oracle_sync_hash(w)};

  const char* env_dump{std::getenv("ORA_PERTRAIT_DUMP_PREFIX")};
  const std::string str_dump_prefix{env_dump == nullptr ? std::string{} : std::string{env_dump}};
  {
    const std::string str_dump{replay_oracle_pertrait_dump(w)};
    if (!str_dump_prefix.empty()) {
      FILE* ptr_file{std::fopen(std::format("{}-init.txt", str_dump_prefix).c_str(), "wb")};
      if (ptr_file != nullptr) {
        std::fwrite(str_dump.data(), 1, str_dump.size(), ptr_file);
        std::fclose(ptr_file);
      }
    }
  }
  const auto it_sync{res_feed->map_recorded_syncs.find(1)};
  check(it_sync != res_feed->map_recorded_syncs.end(), "frame-1 sync packet present");
  if (it_sync != res_feed->map_recorded_syncs.end())
    check(int4_hash == it_sync->second.first,
          std::format("frame-1 sync hash: got {} want {}", int4_hash, it_sync->second.first));

  std::map<std::int32_t, std::vector<ora::game::network::OrderModel>> map_script_orders;
  for (const ReplayRecord& record : res_read->vec_records) {
    const auto opt_orders{ora::game::network::try_parse_order_packet(record.vec_data)};
    if (!opt_orders.has_value() || opt_orders->int4_frame == 0) continue;
    for (const auto& order :
         ora::game::network::order_packet_get_orders(opt_orders->vec_order_bytes, nullptr))
      map_script_orders[opt_orders->int4_frame].push_back(order);
  }
  check(map_script_orders.contains(6) && map_script_orders.contains(9) &&
            map_script_orders.contains(16) && map_script_orders.contains(26) &&
            map_script_orders.contains(551),
        "scripted order packets present (6/9/16/26/551)");
  {
    std::size_t idx_orders{};
    for (const auto& [int4_frame, vec_orders] : map_script_orders) idx_orders += vec_orders.size();
    check(idx_orders == 5, "exactly 5 scripted orders in stream");
  }

  const auto vec_changes_txt{read_file_bytes(
      std::format("{}/ore-lord.isync-changes.txt", ORA_REPLAY_GOLDEN_DIR))};
  const std::string str_changes{vec_changes_txt.begin(), vec_changes_txt.end()};
  check(!str_changes.empty(), "isync-changes golden readable");
  const auto vec_frames_txt{read_file_bytes(
      std::format("{}/ore-lord.frames.txt", ORA_REPLAY_GOLDEN_DIR))};
  const std::string str_frames{vec_frames_txt.begin(), vec_frames_txt.end()};
  const auto res_ctx{replay_oracle_context_parse(str_changes, str_frames)};
  check(res_ctx.has_value(), "oracle context parses");
  if (!res_ctx.has_value()) {
    std::println("  error: {}", res_ctx.error());
    return;
  }
  if (const char* env_dbg{std::getenv("ORA_ORACLE_DEBUG")}) {
    (void)env_dbg;
    std::println("  dbg move_end {} changes-frames {}", res_ctx->int4_146_move_end,
                 res_ctx->map_changes.size());
  }
  check(res_ctx->int4_146_move_end == 77, "146 move window ends f77 (data-derived)");
  check(res_ctx->map_rng_count.size() == 580, "rng ledger covers 580 frames");

  ReplayOracleWorld w2{*res_world};
  std::int32_t int4_hash2{};
  std::int32_t int4_first_div{-1};
  std::int32_t int4_checked_frames{};
  for (std::int32_t int4_n{1}; int4_n <= 580; ++int4_n) {
    if (int4_n >= 2)
      for (const ora::game::network::OrderModel& order : map_script_orders[int4_n])
        replay_oracle_apply_order(w2, order);
    if (int4_n == 1)
      replay_oracle_frame1(w2);
    else
      replay_oracle_frame(w2, *res_ctx, int4_n);
    const std::int32_t int4_hash_n{replay_oracle_sync_hash(w2)};
    if (int4_n == 1) int4_hash2 = int4_hash_n;

    const auto it_count{res_ctx->map_rng_count.find(int4_n)};
    if (it_count != res_ctx->map_rng_count.end())
      check(w2.rng_shared.int4_total_count == it_count->second,
            std::format("frame-{} rng ledger: got {} want {}", int4_n,
                        w2.rng_shared.int4_total_count, it_count->second));
    if (!str_dump_prefix.empty()) {
      FILE* ptr_file{std::fopen(std::format("{}-f{}.txt", str_dump_prefix, int4_n).c_str(), "wb")};
      if (ptr_file != nullptr) {
        const std::string str_dump{replay_oracle_pertrait_dump(w2)};
        std::fwrite(str_dump.data(), 1, str_dump.size(), ptr_file);
        std::fclose(ptr_file);
      }
    }
    const auto it_sync_n{res_feed->map_recorded_syncs.find(int4_n + 1)};
    if (it_sync_n == res_feed->map_recorded_syncs.end()) continue;
    ++int4_checked_frames;
    if (int4_hash_n != it_sync_n->second.first) {
      if (int4_first_div < 0) int4_first_div = int4_n + 1;
      check(false, std::format("frame-{}-end sync: got {} want {}", int4_n, int4_hash_n,
                               it_sync_n->second.first));
    } else {
      check(true, std::format("frame-{}-end sync matches recorded", int4_n));
    }
  }
  check(int4_first_div < 0, std::format("no divergence across {} checked frame ends (f1-f579 + init f1)",
                                        int4_checked_frames + 1));
  check(int4_checked_frames == 579,
        "all 580 recorded syncs compared (init f1 + frame ends f1-f579; f580 end = 未录 sync581)");

  {
    const auto res_read_orig{replay_read(vec_orarep)};
    check(res_read_orig.has_value(), "tamper-sync: file parses");
    bool b_flipped{false};
    std::vector<std::uint8_t> vec_tampered{vec_orarep};
    if (res_read_orig.has_value()) {
      for (const ReplayRecord& record : res_read_orig->vec_records) {
        if (record.vec_data.size() == 17 && record.vec_data[4] == 0x65 && record.vec_data[0] == 2) {
          vec_tampered[record.idx_offset + 8 + 6] ^= 0xff;
          b_flipped = true;
          break;
        }
      }
    }
    check(b_flipped, "tamper-sync: frame-2 sync packet located");
    if (b_flipped) {
      const auto res_read_t{replay_read(vec_tampered)};
      const auto res_feed_t{res_read_t.has_value() ? replay_feed_build(*res_read_t)
                                                   : std::unexpected<std::string>("parse failed")};
      check(res_feed_t.has_value(), "tamper-sync: feed builds");
      if (res_feed_t.has_value()) {
        const auto it_t{res_feed_t->map_recorded_syncs.find(2)};
        check(it_t != res_feed_t->map_recorded_syncs.end(), "tamper-sync: frame-2 recorded sync present");
        if (it_t != res_feed_t->map_recorded_syncs.end())
          check(int4_hash2 != it_t->second.first,
                "tamper-sync: tampered sync hash diverges from local replay hash (detected)");
      }
    }
  }

  const auto vec_oramap_bytes{
      read_file_bytes(std::format("{}/ore-lord.oramap", ORA_MAPS_GOLDEN_DIR))};
  check(run_oracle_replay(vec_orarep, vec_oramap_bytes, *res_ctx) == 0,
        "untampered full replay: no divergence (baseline)");
  {

    std::vector<std::uint8_t> vec_tampered{vec_orarep};
    bool b_tampered{false};
    for (const ReplayRecord& record : res_read->vec_records) {
      const auto opt_orders{ora::game::network::try_parse_order_packet(record.vec_data)};
      if (!opt_orders.has_value() || opt_orders->int4_frame != 16) continue;
      if (const char* env_dbg{std::getenv("ORA_ORACLE_DEBUG")}) {
        (void)env_dbg;
        std::println("  dbg f16 order bytes: {}", [&] {
          std::string s;
          for (std::size_t i = 0; i < record.vec_data.size() && i < 40; ++i)
            s += std::format("{:02x} ", record.vec_data[i]);
          return s;
        }());
      }

      const std::size_t idx_subject{record.idx_offset + 8 + 4 + 8};
      const std::uint32_t u4_subject{static_cast<std::uint32_t>(vec_tampered[idx_subject]) |
                                     (static_cast<std::uint32_t>(vec_tampered[idx_subject + 1])
                                      << 8) |
                                     (static_cast<std::uint32_t>(vec_tampered[idx_subject + 2])
                                      << 16) |
                                     (static_cast<std::uint32_t>(vec_tampered[idx_subject + 3])
                                      << 24)};
      check(u4_subject == 146, "tamper-order-subject: Move subject 146 located");
      vec_tampered[idx_subject] = 147;
      b_tampered = true;
      break;
    }
    check(b_tampered, "tamper-order-subject: frame-16 order packet located");
    if (b_tampered)
      check(run_oracle_replay(vec_tampered, vec_oramap_bytes, *res_ctx) > 0,
            "tamper-order-subject: full replay diverges (detected)");
  }
  {

    std::vector<std::uint8_t> vec_tampered{vec_orarep};
    bool b_tampered{false};
    for (const ReplayRecord& record : res_read->vec_records) {
      const auto opt_orders{ora::game::network::try_parse_order_packet(record.vec_data)};
      if (!opt_orders.has_value() || opt_orders->int4_frame != 9) continue;
      const std::size_t idx_base{record.idx_offset + 8 + 4};

      const std::size_t idx_name_len{idx_base + 1};
      const std::size_t idx_target_string{
          idx_base + 1 + 1 + vec_tampered[idx_name_len] + 2 + 4 + 1 + 1};
      check(vec_tampered[idx_target_string] == '1',
            "tamper-prod-name: StartProduction e1 target string located");
      vec_tampered[idx_target_string] = '2';
      b_tampered = true;
      break;
    }
    check(b_tampered, "tamper-prod-name: frame-9 order packet located");
    if (b_tampered)
      check(run_oracle_replay(vec_tampered, vec_oramap_bytes, *res_ctx) > 0,
            "tamper-prod-name: full replay diverges (detected)");
  }
}

}

int main() {
  test_assembly_and_frame1();

  return ora::test::test_exit_code("replay_oracle");
}
