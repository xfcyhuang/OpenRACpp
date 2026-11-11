import std;

import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;
import ora.test.util;
using ora::test::check;

namespace {

constexpr std::int32_t kSeed{1234};

// 最小规则面：world 挂 RngProbe（每 tick 消耗 rng_shared 一次，为「bot 零额外
// 消耗」断言提供非空基准流）。
constexpr std::string_view kRules{
    "world:\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "\tRngProbe:\n"
    // T34 起 Invalid bot type 诊断面接入（D091② 兑现）：激活需 ModularBot 声明。
    // 本测试保持空模块集（T30 空 bot 口径：无 BotModule 声明）。
    "player:\n"
    "\tModularBot@NormalAI:\n"
    "\t\tType: normal\n"};

constexpr std::string_view kMapYaml{
    "Title: bot-framework\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Multi0:\n"
    "\t\tName: Multi0\n"
    "\t\tPlayable: True\n"
    "\t\tFaction: soviet\n"
    "Actors:\n"};

std::expected<ora::game::world::WorldModel, std::string> make_world(
    const bool b_with_bot, const bool b_is_replay = false) {
  auto res_map{ora::game::world::synthetic_map_from_yaml(kRules, kMapYaml)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  ora::game::production::LobbyModel lobby;
  lobby.vec_playable_factions.emplace_back("Multi0", "soviet");
  if (b_with_bot) lobby.vec_slot_bots.emplace_back("Multi0", "normal");
  return ora::game::world::world_create(*res_map, kSeed, lobby, b_is_replay);
}

ora::game::network::OrderModel stub_order(const std::uint32_t u4_subject) {
  ora::game::network::OrderModel order;
  order.str_order = "Move";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  return order;
}

std::size_t bot_index_of(const ora::game::world::WorldModel& w) {
  for (std::size_t idx{0}; idx < w.vec_bots.size(); ++idx)
    if (w.vec_bots[idx].str_type == "normal") return idx;
  return w.vec_bots.size();
}

void run_frames(ora::game::world::WorldModel& w, const std::int32_t int4_frames) {
  for (std::int32_t idx{0}; idx < int4_frames; ++idx) ora::game::world::world_tick(w);
}

}

int main() {
  // —— 装载与激活链（Player.cs:202-223 / ModularBot.Activate）——
  {
    auto res{make_world(true)};
    check(res.has_value(), "world with bot lobby loads");
    if (res.has_value()) {
      const auto& w{*res};
      check(w.vec_players.size() == 3, "Neutral + Multi0 + Everyone players");
      const ora::game::world::PlayerModel* ptr_multi{nullptr};
      for (const auto& player : w.vec_players)
        if (player.str_internal_name == "Multi0") ptr_multi = &player;
      check(ptr_multi != nullptr && ptr_multi->str_bot_type == "normal",
            "Multi0 carries BotType=normal (IsBot derived)");
      check(w.vec_bots.size() == 1 && w.vec_bots[0].b_enabled &&
                w.vec_bots[0].u4_player < w.vec_players.size() &&
                w.vec_bots[0].int4_min_order_quotient == 5,
            "bot activated: enabled, bound to Multi0, quotient default 5");
      check(w.rng_local.int4_total_count == 0,
            "rng_local untouched at load (bot modules empty, no decisions yet)");
    }
  }
  {
    // 无 bot lobby：零 bot、玩家无 BotType。
    auto res{make_world(false)};
    check(res.has_value(), "world without bot loads");
    if (res.has_value()) {
      check(res->vec_bots.empty(), "no bot activated without slot bot");
      bool b_any_bot_type{false};
      for (const auto& player : res->vec_players) b_any_bot_type |= !player.str_bot_type.empty();
      check(!b_any_bot_type, "no player carries BotType");
    }
  }
  {
    // 回放重演 gate：Activate 早退（ModularBot.cs:65-66 IsReplay）。
    auto res{make_world(true, true)};
    check(res.has_value(), "replay-flagged world loads");
    if (res.has_value()) {
      check(res->vec_bots.empty(), "replay world: no bot activated from lobby");
      ora::game::world::bot_activate(*res, 1, "normal");
      check(res->vec_bots.empty(), "replay world: manual activate is a no-op");
      bool b_any_bot_type{false};
      for (const auto& player : res->vec_players) b_any_bot_type |= !player.str_bot_type.empty();
      check(!b_any_bot_type, "replay world: BotType stays unset");
    }
  }

  // —— 节流公式（ModularBot.cs:101-103：min(ceil(pending/Q), pending)）——
  {
    using ora::game::world::bot_orders_to_issue;
    check(bot_orders_to_issue(0, 5) == 0, "throttle: 0 pending -> 0");
    check(bot_orders_to_issue(1, 5) == 1, "throttle: 1 -> 1");
    check(bot_orders_to_issue(4, 5) == 1, "throttle: 4 -> 1");
    check(bot_orders_to_issue(5, 5) == 1, "throttle: 5 -> 1");
    check(bot_orders_to_issue(6, 5) == 2, "throttle: 6 -> 2");
    check(bot_orders_to_issue(10, 5) == 2, "throttle: 10 -> 2");
    check(bot_orders_to_issue(13, 5) == 3, "throttle: 13 -> 3");
    check(bot_orders_to_issue(1, 1) == 1, "throttle: quotient 1 drains fully");
  }
  {
    // 集成：13 单八帧排空 3/2/2/2/1/1/1/1。
    auto res{make_world(true)};
    check(res.has_value(), "throttle world loads");
    if (res.has_value()) {
      auto& w{*res};
      const std::size_t idx_bot{bot_index_of(w)};
      check(idx_bot < w.vec_bots.size(), "bot present for throttle run");
      const std::uint32_t u4_bot_player{w.vec_bots[idx_bot].u4_player};
      for (int idx{0}; idx < 13; ++idx)
        ora::game::world::bot_queue_order(
            w, u4_bot_player, stub_order(static_cast<std::uint32_t>(idx)));
      check(w.vec_bots[idx_bot].deque_orders.size() == 13, "13 orders queued");

      const std::array<std::size_t, 8> vec_expected{3, 2, 2, 2, 1, 1, 1, 1};
      bool b_sequence_ok{true};
      std::size_t u_issued_before{0};
      for (const std::size_t u_expected : vec_expected) {
        ora::game::world::bot_tick_all(w);
        const std::size_t u_issued_now{w.vec_bot_issued_orders.size()};
        if (u_issued_now - u_issued_before != u_expected) {
          b_sequence_ok = false;
          break;
        }
        u_issued_before = u_issued_now;
      }
      check(b_sequence_ok, "throttle drains 13 orders as 3/2/2/2/1/1/1/1");
      check(w.vec_bots[idx_bot].deque_orders.empty() &&
                w.vec_bots[idx_bot].u4_issued_total == 13 &&
                w.vec_bot_issued_orders.size() == 13,
            "queue empty after drain, issued total 13");
    }
  }
  {
    // 未激活玩家入队 no-op。
    auto res{make_world(false)};
    check(res.has_value(), "queue no-op world loads");
    if (res.has_value()) {
      ora::game::world::bot_queue_order(*res, 1, stub_order(7));
      check(res->vec_bot_issued_orders.empty(), "queue order on botless player is a no-op");
    }
  }

  // —— 空 bot 确定性 + RunUnsynced 等价断言（rng_shared 账本）——
  {
    constexpr std::int32_t kFrames{120};
    auto res_a{make_world(true)};
    auto res_b{make_world(true)};
    check(res_a.has_value() && res_b.has_value(), "two bot worlds load");
    if (res_a.has_value() && res_b.has_value()) {
      bool b_sync_equal{true};
      std::int32_t int4_last_a{};
      std::int32_t int4_last_b{};
      for (std::int32_t idx{0}; idx < kFrames; ++idx) {
        ora::game::world::world_tick(*res_a);
        ora::game::world::world_tick(*res_b);
        int4_last_a = ora::game::world::sync_hash_upstream(*res_a);
        int4_last_b = ora::game::world::sync_hash_upstream(*res_b);
        if (int4_last_a != int4_last_b) {
          b_sync_equal = false;
          break;
        }
      }
      check(b_sync_equal, "empty bot double-run: per-frame SyncHash identical");
      check(res_a->int4_world_tick == kFrames && res_b->int4_world_tick == kFrames,
            "both worlds ticked 120 frames");
    }
  }
  {
    // bot 存在不扰动 rng_shared：与无 bot 世界同 seed 同帧数，账本恒等且非零。
    constexpr std::int32_t kFrames{120};
    auto res_bot{make_world(true)};
    auto res_plain{make_world(false)};
    check(res_bot.has_value() && res_plain.has_value(), "bot/plain worlds load");
    if (res_bot.has_value() && res_plain.has_value()) {
      run_frames(*res_bot, kFrames);
      run_frames(*res_plain, kFrames);
      check(res_bot->rng_shared.int4_total_count == res_plain->rng_shared.int4_total_count &&
                res_bot->rng_shared.int4_last == res_plain->rng_shared.int4_last,
            "RunUnsynced equivalence: rng_shared ledger identical with/without bot");
      check(res_plain->rng_shared.int4_total_count > 0,
            "rng baseline is non-zero (RngProbe consumed)");
    }
  }

  return ora::test::test_exit_code("bot_framework");
}
