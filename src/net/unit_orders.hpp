// UPSTREAM: OpenRA.Game/Network/UnitOrders.cs @b6fc03f L48-431(可运行子集:
//          default→ResolveOrder 分发链 + PauseGame + SyncInfo/StartGame 的
//          大厅装配面;其余 UI 命令族随 Phase 6/7 接线,未实现分支静默吞并
//          保持 order 流不中断 —— 上游多数分支本就是 UI 通知面)
//          The runnable subset: the default→ResolveOrder dispatch chain plus
//          PauseGame plus the lobby-assembly faces of SyncInfo/StartGame; the
//          remaining UI command families wire up in Phase 6/7 —
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

/// StartGame 分支的 Game.StartGame 注入面(上游直接调 Game 静态;C++ 分层
/// 下由 Game 装配注册 —— 缺省静默,order 流继续)
/// The StartGame branch's Game.StartGame injection face (upstream calls
/// the Game static directly; under C++ layering the Game assembly
/// registers it — silent by default, the order stream continues).
void SetStartGameHandler(
    std::function<void(OrderManager&, const std::string&)> fn_start_game);
void ClearStartGameHandler();

}  // namespace ora::net
