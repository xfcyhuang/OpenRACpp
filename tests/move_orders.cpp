import std;

import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;
import ora.launcher;
import ora.test.util;

using ora::test::check;

// T31 移动 Activity 化订单流验收（#50）：Move/AttackMove 订单经订单管道注入世界
// （issue_order → OrderManager echo → process_orders_frame → Mobile/AttackMove trait
// resolve → movement_on_tick），同种子双跑逐帧 sync_hash_upstream 恒等（bot_framework
// 双跑断言同形态）。无阻塞面口径（D026 延续）：sandbox 全 Clear 纯地形。

namespace {

namespace lch = ora::launcher;
namespace net = ora::game::network;
namespace pfg = ora::game::pathfinding;
namespace ww = ora::game::world;

constexpr std::int32_t kSeed{3141};

std::int32_t cell_bits(const std::int32_t int4_x, const std::int32_t int4_y) {
  return static_cast<std::int32_t>(((int4_x & 0xfff) << 20) | ((int4_y & 0xfff) << 8));
}

net::OrderModel order_move(const std::uint32_t u4_subject, const std::int32_t int4_x,
                           const std::int32_t int4_y, const bool b_queued = false) {
  net::OrderModel order;
  order.str_order = "Move";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.target.kind = net::TargetKind::kTerrain;
  order.target.b_cell = true;
  order.target.int4_cell_bits = cell_bits(int4_x, int4_y);
  order.target.u1_subcell = 0;
  order.b_queued = b_queued;
  return order;
}

net::OrderModel order_attack_move(const std::uint32_t u4_subject, const std::int32_t int4_x,
                                  const std::int32_t int4_y) {
  net::OrderModel order;
  order.str_order = "AttackMove";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.target.kind = net::TargetKind::kTerrain;
  order.target.b_cell = true;
  order.target.int4_cell_bits = cell_bits(int4_x, int4_y);
  order.target.u1_subcell = 0;
  return order;
}

std::uint32_t actor_of_type(const lch::GameSession& s, std::string_view str_type) {
  for (const std::uint32_t u4_id : s.w.vec_in_world)
    if (s.w.vec_actors_all[u4_id].str_type == str_type) return u4_id;
  return 0xffffffffu;
}

const ww::MobileState* mobile_of(const lch::GameSession& s, const std::uint32_t u4_actor) {
  return ww::movement_state_of(s.w, u4_actor);
}

void run_ticks(lch::GameSession& s, const std::int32_t int4_ticks) {
  for (std::int32_t idx{0}; idx < int4_ticks; ++idx) lch::session_tick(s);
}

// 到格判定：移动活动结束且中心落格（cell_to == 目标）。
bool arrived_at(const lch::GameSession& s, const std::uint32_t u4_actor,
                const pfg::CPos cell) {
  const ww::MobileState* ptr_mobile{mobile_of(s, u4_actor)};
  if (ptr_mobile == nullptr) return false;
  const pfg::WPos pos_center{pfg::center_of_cell(cell)};
  return !ptr_mobile->move.b_has_activity && ptr_mobile->move.cell_to == cell &&
         ptr_mobile->move.pos_center.int4_x == pos_center.int4_x &&
         ptr_mobile->move.pos_center.int4_y == pos_center.int4_y;
}

// 双跑驱动：两个同种子会话按同一脚本（帧号 → 订单）推进，逐帧断言 sync hash 恒等。
template <typename FnScript>
bool double_run(const std::int32_t int4_frames, FnScript fn_script,
                std::int32_t* ptr_hash_a = nullptr) {
  auto res_a{lch::session_setup(lch::SessionEnv{.int4_seed = kSeed, .str_map_path = {}, .opt_lobby = {}})};
  auto res_b{lch::session_setup(lch::SessionEnv{.int4_seed = kSeed, .str_map_path = {}, .opt_lobby = {}})};
  if (!res_a.has_value() || !res_b.has_value()) return false;
  lch::GameSession& s_a{*res_a};
  lch::GameSession& s_b{*res_b};

  for (std::int32_t idx{0}; idx < int4_frames; ++idx) {
    fn_script(idx, s_a);
    fn_script(idx, s_b);
    lch::session_tick(s_a);
    lch::session_tick(s_b);
    const std::int32_t int4_hash_a{ww::sync_hash_upstream(s_a.w)};
    const std::int32_t int4_hash_b{ww::sync_hash_upstream(s_b.w)};
    if (int4_hash_a != int4_hash_b) return false;
    if (ptr_hash_a != nullptr) *ptr_hash_a = int4_hash_a;
  }
  return true;
}

}  // namespace

int main() {
  // —— Move 订单世界内 resolve + 双跑逐帧 SyncHash 恒等 ——
  {
    std::int32_t int4_hash_mid{};
    const bool b_equal{double_run(
        220,
        [](const std::int32_t int4_tick, lch::GameSession& s) {
          if (int4_tick == 4)
            lch::session_issue_order(s, order_move(actor_of_type(s, "e1"), 12, 16));
        },
        &int4_hash_mid)};
    check(b_equal, "move order: same-seed double-run per-frame SyncHash identical");
    check(int4_hash_mid != 0, "move order: sync hash captured mid-run");
  }

  // —— Move 到格 + 攻击桥位置消费 ——
  {
    auto res{lch::session_setup(lch::SessionEnv{.int4_seed = kSeed, .str_map_path = {}, .opt_lobby = {}})};
    check(res.has_value(), "move arrival: session setup");
    if (res.has_value()) {
      lch::GameSession& s{*res};
      const std::uint32_t u4_e1{actor_of_type(s, "e1")};
      check(u4_e1 != 0xffffffffu, "move arrival: e1 present");
      check(mobile_of(s, u4_e1) != nullptr, "move arrival: e1 in world mobiles");

      lch::session_issue_order(s, order_move(u4_e1, 12, 16));
      run_ticks(s, 200);
      check(arrived_at(s, u4_e1, pfg::CPos{12, 16}),
            "move arrival: e1 reaches (12,16) via order pipeline");
      check(s.w.vec_actors_all[u4_e1].int4_pos_x == 12 &&
                s.w.vec_actors_all[u4_e1].int4_pos_y == 16,
            "move arrival: actor pos synced from movement face");
    }
  }

  // —— AttackMove 等价移动（无攻击面：AttackMoveActivity 退化为 Move）——
  {
    auto res{lch::session_setup(lch::SessionEnv{.int4_seed = kSeed, .str_map_path = {}, .opt_lobby = {}})};
    check(res.has_value(), "attackmove: session setup");
    if (res.has_value()) {
      lch::GameSession& s{*res};
      const std::uint32_t u4_e1{actor_of_type(s, "e1")};
      lch::session_issue_order(s, order_attack_move(u4_e1, 25, 32));
      run_ticks(s, 300);
      check(arrived_at(s, u4_e1, pfg::CPos{25, 32}),
            "attackmove: e1 reaches (25,32) (no-combat degradation of AttackMoveActivity)");

      // 双跑恒等（AttackMove 管道）。
      const bool b_equal{double_run(160, [](const std::int32_t int4_tick,
                                            lch::GameSession& s) {
        if (int4_tick == 2)
          lch::session_issue_order(s, order_attack_move(actor_of_type(s, "e1"), 30, 10));
      })};
      check(b_equal, "attackmove: same-seed double-run per-frame SyncHash identical");
    }
  }

  // —— queued 链（Actor.QueueActivity(queued) 最小等价）——
  {
    auto res{lch::session_setup(lch::SessionEnv{.int4_seed = kSeed, .str_map_path = {}, .opt_lobby = {}})};
    check(res.has_value(), "queued: session setup");
    if (res.has_value()) {
      lch::GameSession& s{*res};
      const std::uint32_t u4_e1{actor_of_type(s, "e1")};
      lch::session_issue_order(s, order_move(u4_e1, 20, 24));
      lch::session_issue_order(s, order_move(u4_e1, 24, 28, true));
      const ww::MobileState* ptr_mobile{mobile_of(s, u4_e1)};
      check(ptr_mobile != nullptr, "queued: e1 mobile present");
      run_ticks(s, 6);
      check(mobile_of(s, u4_e1) != nullptr &&
                mobile_of(s, u4_e1)->vec_queued_destinations.size() == 1,
            "queued: second destination parked in next chain");
      run_ticks(s, 500);
      check(arrived_at(s, u4_e1, pfg::CPos{24, 28}),
            "queued: e1 runs parked destination after first completes");
    }
  }

  // —— 出界目标 Clamp（Map.Clamp 等价）——
  {
    auto res{lch::session_setup(lch::SessionEnv{.int4_seed = kSeed, .str_map_path = {}, .opt_lobby = {}})};
    check(res.has_value(), "clamp: session setup");
    if (res.has_value()) {
      lch::GameSession& s{*res};
      const std::uint32_t u4_e1{actor_of_type(s, "e1")};
      lch::session_issue_order(s, order_move(u4_e1, 90, 90));
      run_ticks(s, 500);
      check(arrived_at(s, u4_e1, pfg::CPos{39, 39}),
            "clamp: out-of-bounds target clamps to map corner (39,39)");
    }
  }

  // —— harv Move 订单 no-op（移动面外单位，economy FDA 自治）——
  {
    auto res{lch::session_setup(lch::SessionEnv{.int4_seed = kSeed, .str_map_path = {}, .opt_lobby = {}})};
    check(res.has_value(), "harv no-op: session setup");
    if (res.has_value()) {
      lch::GameSession& s{*res};
      run_ticks(s, 2);  // harv 由 proc 的 FreeActor 帧尾任务在首帧 spawn。
      const std::uint32_t u4_harv{actor_of_type(s, "harv")};
      check(u4_harv != 0xffffffffu, "harv no-op: harv present");
      check(mobile_of(s, u4_harv) == nullptr, "harv no-op: harv not in world mobiles");
      lch::session_issue_order(s, order_move(u4_harv, 5, 5));
      run_ticks(s, 10);
      check(mobile_of(s, u4_harv) == nullptr, "harv no-op: Move on harv never enters mobiles");
      check(s.om.str_fatal.empty(), "harv no-op: no fatal from move on harv");
    }
  }

  return ora::test::test_exit_code("move_orders");
}
