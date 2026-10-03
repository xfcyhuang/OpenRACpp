// UPSTREAM: OpenRA.Game/Network/UnitOrders.cs @7d57605(实现部分:可运行子集)
//          The implementation half of UnitOrders.cs (runnable subset).
#include "net/unit_orders.hpp"

#include "net/order_manager.hpp"
#include "sim/world.hpp"

namespace ora::net {

namespace {

/// ResolveOrder(L417-424):校验器全过 → Subject 分发
void ResolveOrder(const Order& order, sim::World& world,
                  OrderManager& order_manager, int client_id) {
  if (order.p_subject == nullptr || order.p_subject->IsDead())
    return;

  // world.OrderValidators(IValidateOrder trait 族,Phase 5;空面全过)
  for (auto* vo :
       world.WorldActor() ? world.WorldActor()->TraitsImplementing<sim::IValidateOrder>()
                          : std::vector<sim::IValidateOrder*>{})
    if (!vo->OrderValidation(order_manager, world, client_id, order))
      return;

  order.p_subject->ResolveOrder(order);
}

}  // namespace

void ProcessOrder(OrderManager& order_manager, sim::World* world,
                  int client_id, const Order& order) {
  const auto& name = order.str_order_string;

  if (name == "PauseGame") {
    // L217-236
    auto* client = order_manager.LobbyInfo().ClientWithIndex(client_id);
    if (client != nullptr && world != nullptr) {
      const bool pause = order.str_target_string.value_or("") == "Pause";

      // Prevent injected unpause orders from restarting a finished game
      if (order_manager.World() != nullptr &&
          order_manager.World()->IsGameOverProxy() && !pause)
        return;

      world->SetPaused(pause);
      world->SetPredictedPaused(pause);
    }
    return;
  }

  // 其余已命名命令(Message/Chat/StartGame/SyncLobby*/HandshakeRequest/
  // GameSaved/SaveTraitData/...)依赖 UI/Game/存档面(Phase 5/6/7);
  // 未实现分支按"无本地副作用"处理 —— order 流继续
  // The remaining named commands (Message/Chat/StartGame/SyncLobby*/
  // HandshakeRequest/GameSaved/SaveTraitData/…) depend on the UI/Game/
  // gamesave faces (Phase 5/6/7); unimplemented branches take the
  // "no local side effect" path — the order stream continues.

  // default(L401-413)
  if (world == nullptr)
    return;

  if (!order.vec_grouped_actors.has_value()) {
    ResolveOrder(order, *world, order_manager, client_id);
  } else {
    for (auto* subject : *order.vec_grouped_actors)
      ResolveOrder(Order::FromGroupedOrder(order, subject), *world,
                   order_manager, client_id);
  }
}

}  // namespace ora::net
