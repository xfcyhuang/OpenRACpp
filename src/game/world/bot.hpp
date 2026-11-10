#pragma once

import std;

#include "game/network/order.hpp"

namespace ora::game::world {

struct WorldModel;

// ModularBot（ModularBot.cs:43-121）的数据面等价：IBot 激活态、待发订单队列与
// MinOrderQuotientPerTick 节流参数。BotModule 收集面（IBotTick/IBotRespondToAttack）
// 本票为空集，T34 起接入。
struct BotModel {
  std::string str_type;
  std::uint32_t u4_player{};
  bool b_enabled{};
  std::deque<network::OrderModel> deque_orders;
  std::int32_t int4_min_order_quotient{5};
  std::uint32_t u4_issued_total{};
};

// 每 tick 至少发出待发订单的 ceil(pending / quotient) 分之一（ModularBot.cs:101-103）。
[[nodiscard]] std::size_t bot_orders_to_issue(std::size_t u_pending,
                                              std::int32_t int4_min_order_quotient);

// IBot.QueueOrder（ModularBot.cs:87-90）等价：按持有玩家找已激活 bot 入队。
void bot_queue_order(WorldModel& w, std::uint32_t u4_player, network::OrderModel order);

// ModularBot.Activate（ModularBot.cs:60-74）等价：回放重演世界不激活
// （bot 订单已在录包内，Player.cs:217 host-only 激活在单机形态下恒真）。
// Invalid bot type 诊断面（Player.cs:220-221）待 ai.yaml 规则表接入（T34），本票透传。
void bot_activate(WorldModel& w, std::uint32_t u4_player, std::string_view str_bot_type);

// ModularBot.ITick.Tick（ModularBot.cs:93-106）等价：模块 tick（本票空）+
// 节流出单。出单暂存 vec_bot_issued_orders（上游 world.IssueOrder 进订单管道，
// T31 订单世界内 resolve 时改投）。RunUnsynced 包装不移植——bot 代码不得影响
// 同步面由测试侧 rng_shared 账本断言承担（D071 音效隔离断言同款口径）。
void bot_tick_all(WorldModel& w);

}
