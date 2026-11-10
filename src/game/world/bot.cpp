import std;

#include "game/world/bot.hpp"
#include "game/world/world.hpp"

namespace ora::game::world {

std::size_t bot_orders_to_issue(const std::size_t u_pending,
                                const std::int32_t int4_min_order_quotient) {
  if (u_pending == 0) return 0;
  // 上游 (count + Q - 1) / Q 的整数除法在 Q <= 0 时是 DivideByZeroException 崩溃路径；
  // C++ 侧按 D003 口径归一为终止（配置错误不可恢复）。
  if (int4_min_order_quotient < 1) std::abort();
  const std::size_t u_quotient{static_cast<std::size_t>(int4_min_order_quotient)};
  const std::size_t u_ceiled{(u_pending + u_quotient - 1) / u_quotient};
  return u_ceiled < u_pending ? u_ceiled : u_pending;
}

void bot_queue_order(WorldModel& w, const std::uint32_t u4_player,
                     network::OrderModel order) {
  for (BotModel& bot : w.vec_bots) {
    if (bot.u4_player != u4_player || !bot.b_enabled) continue;
    bot.deque_orders.push_back(std::move(order));
    return;
  }
}

void bot_activate(WorldModel& w, const std::uint32_t u4_player,
                  const std::string_view str_bot_type) {
  if (w.b_is_replay) return;
  if (u4_player >= w.vec_players.size()) return;
  for (const BotModel& bot : w.vec_bots)
    if (bot.u4_player == u4_player) return;
  BotModel bot;
  bot.str_type = std::string(str_bot_type);
  bot.u4_player = u4_player;
  bot.b_enabled = true;
  w.vec_players[u4_player].str_bot_type = std::string(str_bot_type);
  w.vec_bots.push_back(std::move(bot));
}

void bot_tick_all(WorldModel& w) {
  for (BotModel& bot : w.vec_bots) {
    if (!bot.b_enabled) continue;
    const std::size_t u_issue{
        bot_orders_to_issue(bot.deque_orders.size(), bot.int4_min_order_quotient)};
    for (std::size_t idx{0}; idx < u_issue; ++idx) {
      w.vec_bot_issued_orders.push_back(std::move(bot.deque_orders.front()));
      bot.deque_orders.pop_front();
      ++bot.u4_issued_total;
    }
  }
}

}
