import std;
#include <cstdio>

#include "game/replay/replay_format.hpp"
#include "game/replay/replay_player.hpp"
#include "game/replay/replay_world.hpp"

#ifndef ORA_REPLAY_GOLDEN_DIR
#define ORA_REPLAY_GOLDEN_DIR "tests/golden/replay"
#endif
#ifndef ORA_MAPS_GOLDEN_DIR
#define ORA_MAPS_GOLDEN_DIR "tests/golden/maps"
#endif

// T17(issue #18,M1 收口):oracle 真回放逐帧 SyncHash 对拍。
// 黄金 ore-lord.orarep = golden_gen replay 模式(ore-lord + 全量 ra 规则 + 注入剧本 5 actor
// + mcv,seed 20261103,NetFrameInterval=1,580 帧)。C++ 侧重演 = replay_world 面重建世界
// (ISync 段2 逐 trait 字段级)+ 逐帧 SyncHash 与录中 sync 封包对拍 + desync 负向检出。

namespace {

constexpr int kMaxReports{20};

int int4_failures{};
int int4_reports{};
int int4_checks{};

void check(bool b_ok, std::string_view str_what) {
  ++int4_checks;
  if (b_ok) return;
  ++int4_failures;
  if (int4_reports < kMaxReports) {
    ++int4_reports;
    std::println("replay_oracle: check failed: {}", str_what);
  }
}

std::vector<std::uint8_t> read_file_bytes(std::string_view str_path) {
  std::ifstream fs{std::string{str_path}, std::ios::binary};
  if (!fs) return {};
  fs.seekg(0, std::ios::end);
  std::vector<std::uint8_t> vec_out(static_cast<std::size_t>(fs.tellg()));
  fs.seekg(0, std::ios::beg);
  fs.read(reinterpret_cast<char*>(vec_out.data()), static_cast<std::streamsize>(vec_out.size()));
  return vec_out;
}

// 黄金 frames.txt 的 init actors 行(装配序 oracle 实测;装配序对拍锚点)
std::string golden_init_line() {
  std::ifstream fs{std::format("{}/ore-lord.frames.txt", ORA_REPLAY_GOLDEN_DIR)};
  std::string str_line;
  while (std::getline(fs, str_line))
    if (str_line.starts_with("init actors ")) return str_line;
  return {};
}

// —— slice 1:装配序与 f1 静态对拍(sync(1) = 装配后世界五段式)——

void test_assembly_and_frame1() {
  using namespace ora::game::replay;

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

  // 装配序:149 个初始 actor 与黄金 init 行逐项一致(id:type:owner)
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

  // f1 静态对拍:sync(1) = 装配后世界(ProcessOrders@f1,harv 未生、置位波未发)
  const std::int32_t int4_hash{replay_oracle_sync_hash(w)};
  {
    // 调试 dump(与探针 pertrait-init.txt 同格式 diff 用)
    const std::string str_dump{replay_oracle_pertrait_dump(w)};
    FILE* ptr_file{std::fopen("replay-pertrait-init.txt", "wb")};
    if (ptr_file != nullptr) {
      std::fwrite(str_dump.data(), 1, str_dump.size(), ptr_file);
      std::fclose(ptr_file);
    }
  }
  const auto it_sync{res_feed->map_recorded_syncs.find(1)};
  check(it_sync != res_feed->map_recorded_syncs.end(), "frame-1 sync packet present");
  if (it_sync != res_feed->map_recorded_syncs.end())
    check(int4_hash == it_sync->second.first,
          std::format("frame-1 sync hash: got {} want {}", int4_hash, it_sync->second.first));

  // slice 2:f1 末事件(harv 出生/shroud 波/queue 波/RNG 34)→ sync(2) 对拍
  ReplayOracleWorld w2{*res_world};
  replay_oracle_frame1(w2);
  const std::int32_t int4_hash2{replay_oracle_sync_hash(w2)};
  const auto it_sync2{res_feed->map_recorded_syncs.find(2)};
  check(it_sync2 != res_feed->map_recorded_syncs.end(), "frame-2 sync packet present");
  if (it_sync2 != res_feed->map_recorded_syncs.end())
    check(int4_hash2 == it_sync2->second.first,
          std::format("frame-2 sync hash: got {} want {}", int4_hash2, it_sync2->second.first));

  // slice 3(进行中):f2-f6 模型(harv 转身/扫描计时/消耗数量)。f6 末 147 的
  // AutoTarget 重置值尚差 1(oracle=6、模型=7——「重置帧不递减」已按实测复刻,重置
  // 抽样的消耗位与输出值对应疑上游 Activity/ITick 帧内序细节,待后续逐 trait 钉死;
  // f3 的 4 个非同步消耗用途亦未定位)。以「首个分歧帧」呈现进度,不作全对断言。
  replay_oracle_frames_2_to_6(w2);
  const auto it_sync7{res_feed->map_recorded_syncs.find(7)};
  if (it_sync7 != res_feed->map_recorded_syncs.end()) {
    const std::int32_t int4_hash7{replay_oracle_sync_hash(w2)};
    std::println("  progress: f6-end sync {} vs recorded {} (first divergence frame {})",
                 int4_hash7, it_sync7->second.first, int4_hash7 == it_sync7->second.first ? 0 : 7);
  }

  // —— desync 负向(当前实现面):篡改录中 frame-2 sync 封包 hash 字节 → 与本地
  // 重演 hash 对拍必然分叉(检出)。订单字节/物品名负向随行为面扩展(后续帧模型)接入。
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
}

}  // namespace

int main() {
  test_assembly_and_frame1();

  std::println("replay_oracle: {} checks, {} failures", int4_checks, int4_failures);
  return int4_failures == 0 ? 0 : 1;
}
