// UPSTREAM: OpenRA.Game/Network/UnitOrders.cs @b6fc03f L48-431(可运行子集:
//          default→ResolveOrder 分发链 + PauseGame;UI/大厅命令族随 Phase
//          6/7 接线,未实现分支静默吞并保持 order 流不中断 —— 上游多数
//          分支本就是 UI 通知面)
//          The runnable subset: the default→ResolveOrder dispatch chain plus
//          PauseGame; UI/lobby command families wire up in Phase 6/7 —
//          unimplemented branches are swallowed silently to keep the order
//          stream intact (most upstream branches are UI notification faces
//          anyway).
#pragma once
import std;

#include "net/order.hpp"

namespace ora::net {

class OrderManager;

/// UnitOrders.ProcessOrder(L48-415)
void ProcessOrder(OrderManager& order_manager, sim::World* world,
                  int client_id, const Order& order);

}  // namespace ora::net
