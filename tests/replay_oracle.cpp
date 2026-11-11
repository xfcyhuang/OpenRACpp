import std;
#include <cstdio>
#include <cstdlib>
import ora.game.miniyaml;
import ora.game.support;
import ora.game.world;
import ora.test.util;

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

std::string read_file_text(const std::string& str_path) {
  std::ifstream fs{str_path, std::ios::binary};
  std::string str_out;
  char buf[4096];
  while (fs.read(buf, sizeof buf) || fs.gcount() > 0)
    str_out.append(buf, static_cast<std::size_t>(fs.gcount()));
  return str_out;
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
  const auto res_world{replay_oracle_create(
      res_feed->session.int4_seed, vec_oramap,
      read_file_bytes(std::format("{}/tilesets/temperat.yaml", ORA_MAPS_GOLDEN_DIR)))};
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
    if (!w.str_fatal.empty()) {
      std::println("replay_oracle: data-plane fatal at f{}: {}", int4_f, w.str_fatal);
      return 1000000 + int4_f;
    }
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
  const std::vector<std::uint8_t> vec_tileset{
      read_file_bytes(std::format("{}/tilesets/temperat.yaml", ORA_MAPS_GOLDEN_DIR))};
  const auto res_world{replay_oracle_create(session.int4_seed, vec_oramap, vec_tileset)};
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
    check(w2.str_fatal.empty(),
          std::format("frame-{} data-plane reconciliation: {}", int4_n, w2.str_fatal));
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

// —— T32 skirmish 互射剧本（issue #51）：机制面全语义对拍 ——
// 无数据面/无 rng 账本补齐：逐帧 SyncHash/rng Last/TotalCount/actors 数/fnv 与
// skirmish.frames.txt 全等，isync-changes 全行（AutoTarget/TakeCover/Health/
// GainsExperience/PlayerExperience/Mobile Facing/AttackFrontal）作为字段级断言集
// （黄金 new 值 == C++ 机制面当前值，逐帧行数一致禁止静默吞行）。
struct SkirmishFrameGolden {
  std::int32_t int4_frame{}, int4_sync{}, int4_rng_last{}, int4_rng_total{}, int4_actors{};
  std::string str_fnv;
};

std::string skirmish_fnv_of(const ora::game::replay::ReplayOracleWorld& w) {
  std::uint64_t u8_h{1469598103934665603ull};
  for (const ora::game::replay::FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world) continue;
    const std::string str_entry{
        std::format("{}:{}:1\n", actor.u4_id, actor.str_type)};
    for (const char ch : str_entry) {
      u8_h ^= static_cast<std::uint8_t>(ch);
      u8_h *= 1099511628211ull;
    }
  }
  return std::format("{:016x}", u8_h);
}

void test_skirmish_battle() {
  using ora::game::replay::FaceActor;
  using ora::game::replay::ReplayOracleWorld;
  using ora::game::replay::replay_oracle_create_skirmish;
  using ora::game::replay::replay_oracle_frame1_skirmish;
  using ora::game::replay::replay_oracle_frame_skirmish;
  using ora::game::replay::replay_oracle_sync_hash;

  const std::vector<std::uint8_t> vec_oramap{
      read_file_bytes(std::format("{}/ore-lord.oramap", ORA_MAPS_GOLDEN_DIR))};
  const std::vector<std::uint8_t> vec_tileset{
      read_file_bytes(std::format("{}/tilesets/temperat.yaml", ORA_MAPS_GOLDEN_DIR))};
  const auto res_world{replay_oracle_create_skirmish(20261103, vec_oramap, vec_tileset)};
  check(res_world.has_value(), "skirmish world creates");
  if (!res_world.has_value()) {
    std::println("  error: {}", res_world.error());
    return;
  }
  ReplayOracleWorld w{*res_world};

  std::vector<SkirmishFrameGolden> vec_golden;
  std::string str_golden_init;
  {
    std::ifstream fs{std::format("{}/skirmish.frames.txt", ORA_REPLAY_GOLDEN_DIR)};
    std::string str_line;
    while (std::getline(fs, str_line)) {
      if (str_line.starts_with("init actors ")) {
        str_golden_init = str_line;
        continue;
      }
      if (!str_line.starts_with("f ")) continue;
      // "f N sync S defeat D rng L T actors A fnv F" —— 定位子串取数
      const auto fn_int{[&](std::string_view str_key, std::int32_t& int4_out) {
        const std::size_t idx{str_line.find(str_key)};
        if (idx == std::string::npos) return false;
        const std::size_t idx_val{idx + str_key.size()};
        const auto res{std::from_chars(str_line.data() + idx_val,
                                       str_line.data() + str_line.find(' ', idx_val),
                                       int4_out)};
        return res.ec == std::errc{};
      }};
      SkirmishFrameGolden g;
      const std::size_t idx_f{str_line.find(' ')};
      const std::size_t idx_f_end{str_line.find(' ', idx_f + 1)};
      if (!fn_int("f ", g.int4_frame) || !fn_int("sync ", g.int4_sync) ||
          !fn_int("rng ", g.int4_rng_last) || !fn_int("actors ", g.int4_actors)) {
        check(false, std::format("skirmish frames line parses: {}", str_line));
        return;
      }
      const std::size_t idx_rng{str_line.find("rng ")};
      const std::size_t idx_total{str_line.find(' ', idx_rng + 4)};
      const auto res_total{std::from_chars(
          str_line.data() + idx_total + 1, str_line.data() + str_line.find(' ', idx_total + 1),
          g.int4_rng_total)};
      const std::size_t idx_fnv{str_line.find("fnv ")};
      if (res_total.ec != std::errc{} || idx_fnv == std::string::npos) {
        check(false, std::format("skirmish frames line parses: {}", str_line));
        return;
      }
      g.str_fnv = str_line.substr(idx_fnv + 4);
      (void)idx_f;
      (void)idx_f_end;
      vec_golden.push_back(g);
    }
  }
  check(vec_golden.size() == 160, std::format("skirmish golden 160 frames (got {})",
                                              vec_golden.size()));

  // isync-changes 黄金:帧号 → 行列表(字段级断言集)
  std::map<std::int32_t, std::vector<std::array<std::string, 6>>> map_isync;
  {
    std::ifstream fs{std::format("{}/skirmish.isync-changes.txt", ORA_REPLAY_GOLDEN_DIR)};
    std::string str_line;
    while (std::getline(fs, str_line)) {
      if (str_line.empty() || str_line.starts_with("#")) continue;
      // "f N actor ID TYPE trait TRAIT field FIELD OLD -> NEW" —— 空格分词
      std::vector<std::string_view> vec_tok;
      std::size_t idx_pos{};
      while (idx_pos <= str_line.size()) {
        const std::size_t idx_space{str_line.find(' ', idx_pos)};
        vec_tok.push_back(std::string_view{str_line}.substr(
            idx_pos, (idx_space == std::string::npos ? str_line.size() : idx_space) - idx_pos));
        if (idx_space == std::string::npos) break;
        idx_pos = idx_space + 1;
      }
      // tok: f / frame / actor / id / type / trait / trait-name / field / field-name /
      //      old / -> / new
      std::int32_t int4_frame{};
      if (vec_tok.size() != 12 || vec_tok[0] != "f" || vec_tok[2] != "actor" ||
          vec_tok[5] != "trait" || vec_tok[7] != "field" || vec_tok[10] != "->" ||
          std::from_chars(vec_tok[1].data(), vec_tok[1].data() + vec_tok[1].size(), int4_frame)
                  .ec != std::errc{}) {
        check(false, std::format("skirmish isync line parses: {}", str_line));
        return;
      }
      map_isync[int4_frame].push_back({std::string{vec_tok[3]}, std::string{vec_tok[6]},
                                       std::string{vec_tok[8]}, std::string{vec_tok[11]},
                                       std::string{}, std::string{}});
    }
  }
  std::size_t sz_isync_total{};
  for (const auto& [int4_frame, vec_lines] : map_isync) sz_isync_total += vec_lines.size();
  check(sz_isync_total == 751, std::format("skirmish isync golden 751 lines (got {})",
                                           sz_isync_total));

  // 字段当前值镜像(isync 断言面;未知组合硬失败)
  const auto fn_field_value{[&](std::string_view str_actor_id, std::string_view str_trait,
                                std::string_view str_field) -> std::string {
    std::int32_t int4_id{};
    if (std::from_chars(str_actor_id.data(), str_actor_id.data() + str_actor_id.size(), int4_id)
            .ec != std::errc{})
      return "<bad-actor>";
    if (int4_id < 0 || static_cast<std::size_t>(int4_id) >= w.vec_actors.size())
      return "<bad-actor>";
    const FaceActor& actor{w.vec_actors[static_cast<std::size_t>(int4_id)]};
    if (str_trait == "AutoTarget" && str_field == "nextScanTime")
      return std::to_string(actor.int4_next_scan);
    if (str_trait == "TakeCover" && str_field == "remainingDuration")
      return std::to_string(actor.int4_take_cover);
    if (str_trait == "Health" && str_field == "HP") return std::to_string(actor.int4_hp);
    if (str_trait == "ChangesHealth" && str_field == "damageTicks")
      // 上游 damageTicks（受击置 125）在 C++ 由 int4_ch_damage 承载（kChangesHealth
      // face hash 的 ch_ticks^ch_damage 折叠口径，ore-lord 锚定）。
      return std::to_string(actor.int4_ch_damage[0]);
    if (str_trait == "AttackFrontal" && str_field == "IsAiming")
      return actor.b_aiming ? "True" : "False";
    if (str_trait == "GainsExperience" && str_field == "Experience")
      return std::to_string(actor.int4_experience);
    if (str_trait == "PlayerExperience" && str_field == "Experience")
      return std::to_string(w.vec_players[actor.u4_owner].int4_pexp);
    if (str_trait == "Mobile" && str_field == "Facing")
      return std::to_string(actor.int4_facing);
    if (str_trait == "FrozenUnderFog" && str_field == "VisibilityHash")
      return std::to_string(actor.int4_frozen_vis);
    if (str_trait == "FrozenActorLayer" && str_field == "VisibilityHash")
      return std::to_string(w.vec_players[actor.u4_owner].int4_frozen_vis);
    if (str_trait == "FrozenActorLayer" && str_field == "FrozenHash")
      return std::to_string(w.vec_players[actor.u4_owner].int4_frozen_sum);
    if (str_trait == "ClassicProductionQueue" && str_field == "Enabled") {
      for (const bool b : w.vec_players[actor.u4_owner].b_queue_enabled)
        if (b) return "True";
      return "False";
    }
    return "<unknown>";
  }};

  // 机制面逐帧驱动 + 全面对拍。sync 包口径:黄金 f N 行的 sync 是 **N-1 帧末**
  // 状态的 hash（录包 OrderManager 时序，D055 同源）——C++ 帧 n 末与黄金 n+1 行
  // 对拍；rng Last/TotalCount/actors/fnv 是 dump 时点（帧末）直读，不错位。
  std::int32_t int4_first_div{-1};
  for (std::int32_t int4_n{1}; int4_n <= 160; ++int4_n) {
    if (int4_n == 1)
      replay_oracle_frame1_skirmish(w);
    else
      replay_oracle_frame_skirmish(w, int4_n);

    const SkirmishFrameGolden& g{vec_golden[static_cast<std::size_t>(int4_n - 1)]};

    // isync 字段断言集(先于 sync:字段错位时定位到字段)
    std::size_t sz_field_fail{};
    for (const auto& parts : map_isync[int4_n]) {
      // parts: id / trait / field / new / frame / unused
      const std::string str_got{fn_field_value(parts[0], parts[1], parts[2])};
      if (str_got != parts[3]) {
        ++sz_field_fail;
        check(false,
              std::format("skirmish f{} isync actor {} {} {}: got {} want {}", int4_n,
                          parts[0], parts[1], parts[2], str_got, parts[3]));
      }
    }
    if (sz_field_fail == 0) check(true, std::format("skirmish f{} isync fields ({} lines)",
                                                    int4_n, map_isync[int4_n].size()));

    if (int4_n < 160) {
      const SkirmishFrameGolden& g_next{vec_golden[static_cast<std::size_t>(int4_n)]};
      const std::int32_t int4_hash{replay_oracle_sync_hash(w)};
      if (int4_hash != g_next.int4_sync) {
        if (int4_first_div < 0) int4_first_div = int4_n + 1;
        check(false, std::format("skirmish f{}-end sync: got {} want {}", int4_n, int4_hash,
                                 g_next.int4_sync));
      } else {
        check(true, std::format("skirmish f{}-end sync matches", int4_n));
      }
    }
    check(w.rng_shared.int4_total_count == g.int4_rng_total,
          std::format("skirmish f{} rng total: got {} want {}", int4_n,
                      w.rng_shared.int4_total_count, g.int4_rng_total));
    check(w.rng_shared.int4_last == g.int4_rng_last,
          std::format("skirmish f{} rng last: got {} want {}", int4_n,
                      w.rng_shared.int4_last, g.int4_rng_last));
    std::int32_t int4_alive{};
    for (const FaceActor& a : w.vec_actors) int4_alive += a.b_in_world ? 1 : 0;
    check(int4_alive == g.int4_actors, std::format("skirmish f{} actors: got {} want {}",
                                                   int4_n, int4_alive, g.int4_actors));
    const std::string str_fnv{skirmish_fnv_of(w)};
    check(str_fnv == g.str_fnv,
          std::format("skirmish f{} fnv: got {} want {}", int4_n, str_fnv, g.str_fnv));
    if (sz_field_fail > 0 || int4_first_div >= 0) break;  // 首分歧帧即停,避免雪崩
  }
  check(int4_first_div < 0, "skirmish 160 帧零分歧(机制面全语义)");
}

}

// —— T34 bot 剧本重演（issue #53）：结构对拍 + 数据垫底推进（收口版）——
// 逐帧 SyncHash 全等的完整机制面（出生位挖掘/tsla 充能 rng 序/e3 面）随 T35
// BaseBuilder/SquadManager 对拍票收口（差口见 COVERAGE D095）；本面验收：
// 世界装配（init actors 全等）+ bot 订单流全类型触达 + 逐帧推进至首数据面缺口。
void test_bot_script() {
  using ora::game::replay::ReplayOracleWorld;
  using ora::game::replay::ReplayRecord;
  using ora::game::replay::replay_oracle_context_parse;
  using ora::game::replay::replay_oracle_create_bot;
  using ora::game::replay::replay_read;

  const std::vector<std::uint8_t> vec_orarep{
      read_file_bytes(std::format("{}/bot.orarep", ORA_REPLAY_GOLDEN_DIR))};
  check(!vec_orarep.empty(), "bot golden orarep readable");
  if (vec_orarep.empty()) return;
  const auto res_read{replay_read(vec_orarep)};
  check(res_read.has_value(), "bot orarep parses");
  if (!res_read.has_value()) return;
  const auto res_feed{replay_feed_build(*res_read)};
  check(res_feed.has_value(), "bot feed builds");
  if (!res_feed.has_value()) return;

  const std::vector<std::uint8_t> vec_oramap{
      read_file_bytes(std::format("{}/ore-lord.oramap", ORA_MAPS_GOLDEN_DIR))};
  const std::vector<std::uint8_t> vec_tileset{
      read_file_bytes(std::format("{}/tilesets/temperat.yaml", ORA_MAPS_GOLDEN_DIR))};
  const auto res_world{replay_oracle_create_bot(res_feed->session.int4_seed, vec_oramap,
                                                vec_tileset)};
  check(res_world.has_value(), "bot replay world creates");
  if (!res_world.has_value()) {
    std::println("  error: {}", res_world.error());
    return;
  }
  ReplayOracleWorld w{*res_world};

  // 装配序对拍：bot.frames.txt 的 init actors 行逐 actor 全等。
  std::string str_golden_init;
  {
    std::ifstream fs{std::format("{}/bot.frames.txt", ORA_REPLAY_GOLDEN_DIR)};
    std::string str_line;
    while (std::getline(fs, str_line))
      if (str_line.starts_with("init actors ")) {
        str_golden_init = str_line;
        break;
      }
  }
  check(!str_golden_init.empty(), "bot golden init line present");
  if (!str_golden_init.empty()) {
    std::size_t u_count{};
    std::size_t idx_next{std::string{"init actors "}.size()};
    bool b_ok{true};
    while (idx_next < str_golden_init.size()) {
      const std::size_t idx_space{str_golden_init.find(' ', idx_next)};
      const std::string str_want{str_golden_init.substr(
          idx_next, (idx_space == std::string::npos ? str_golden_init.size() : idx_space) -
                        idx_next)};
      idx_next = idx_space == std::string::npos ? str_golden_init.size() : idx_space + 1;
      if (u_count >= w.vec_actors.size()) {
        check(false, std::format("bot assembly: golden has more actors ({})", u_count));
        b_ok = false;
        break;
      }
      const std::string str_got{std::format(
          "{}:{}:{}", u_count, w.vec_actors[u_count].str_type,
          w.vec_actors[u_count].str_type == "world"
              ? std::string{"Neutral"}
              : w.vec_players[w.vec_actors[u_count].u4_owner].str_name)};
      if (str_got != str_want) {
        check(false, std::format("bot assembly actor {}: got {} want {}", u_count, str_got,
                                 str_want));
        b_ok = false;
        break;
      }
      ++u_count;
    }
    if (b_ok)
      check(u_count == w.vec_actors.size(),
            std::format("bot assembly full set ({}/{})", u_count, w.vec_actors.size()));
  }

  // bot 订单流：全类型触达计数（UnitBuilder 出兵/BaseBuilder 建造/PowerDown/
  // RepairBuilding/Dock/AttackMove）。
  std::map<std::int32_t, std::vector<ora::game::network::OrderModel>> map_orders;
  std::map<std::string, std::size_t> map_order_counts;
  for (const ReplayRecord& record : res_read->vec_records) {
    const auto opt_orders{ora::game::network::try_parse_order_packet(record.vec_data)};
    if (!opt_orders.has_value() || opt_orders->int4_frame == 0) continue;
    for (const auto& order :
         ora::game::network::order_packet_get_orders(opt_orders->vec_order_bytes, nullptr)) {
      map_orders[opt_orders->int4_frame].push_back(order);
      ++map_order_counts[order.str_order];
    }
  }
  check(map_order_counts["StartProduction"] >= 8, "bot stream: StartProduction >= 8");
  check(map_order_counts["PlaceBuilding"] >= 2, "bot stream: PlaceBuilding >= 2");
  check(map_order_counts["PowerDown"] >= 3, "bot stream: PowerDown >= 3");
  check(map_order_counts["Dock"] >= 1, "bot stream: Dock >= 1");
  check(map_order_counts["AttackMove"] >= 1, "bot stream: AttackMove >= 1");
  check(map_order_counts["Move"] >= 1, "bot stream: Move >= 1");
  check(map_order_counts["SetRallyPoint"] >= 3, "bot stream: SetRallyPoint >= 3");

  // 逐帧推进（数据垫底 + 订单 apply）：跑到首数据面缺口帧（fatal 或 sync 分歧），
  // 报告首分歧帧号（完整逐帧全等随 T35 收口）。
  const auto res_ctx{replay_oracle_context_parse(
      read_file_text(std::format("{}/bot.isync-changes.txt", ORA_REPLAY_GOLDEN_DIR)),
      read_file_text(std::format("{}/bot.frames.txt", ORA_REPLAY_GOLDEN_DIR)))};
  check(res_ctx.has_value(), "bot context parses");
  if (!res_ctx.has_value()) return;
  const ora::game::replay::OracleReplayContext& ctx{*res_ctx};

  std::int32_t int4_first_break{0};
  std::string str_break_kind;
  for (std::int32_t int4_f{1}; int4_f <= 660; ++int4_f) {
    if (int4_f >= 2)
      for (const ora::game::network::OrderModel& order : map_orders[int4_f])
        replay_oracle_apply_order(w, order);
    if (int4_f == 1)
      ora::game::replay::replay_oracle_frame1(w);
    else
      ora::game::replay::replay_oracle_frame(w, ctx, int4_f);
    if (!w.str_fatal.empty()) {
      int4_first_break = int4_f;
      str_break_kind = w.str_fatal;
      break;
    }
    const auto it_sync{res_feed->map_recorded_syncs.find(int4_f + 1)};
    if (it_sync == res_feed->map_recorded_syncs.end()) continue;
    if (ora::game::replay::replay_oracle_sync_hash(w) != it_sync->second.first) {
      int4_first_break = int4_f + 1;
      str_break_kind = std::format("sync divergence: got {} want {}",
                                   ora::game::replay::replay_oracle_sync_hash(w),
                                   it_sync->second.first);
      break;
    }
  }
  std::println("bot script: first data-plane break at f{} ({})", int4_first_break,
               str_break_kind.empty() ? std::string{"none - full 660 frames"} : str_break_kind);
  check(int4_first_break == 0 || int4_first_break >= 2,
        "bot replay advances past frame 1 before first gap");
}

int main() {
  test_assembly_and_frame1();
  test_skirmish_battle();
  test_bot_script();

  return ora::test::test_exit_code("replay_oracle");
}
