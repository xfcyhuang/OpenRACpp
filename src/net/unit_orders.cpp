// UPSTREAM: OpenRA.Game/Network/UnitOrders.cs @b6fc03f(实现部分:可运行子集)
//          The implementation half of UnitOrders.cs (runnable subset).
#include "net/unit_orders.hpp"

#include "net/order_manager.hpp"
#include "net/session.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::net {

namespace {

std::function<void(OrderManager&, const std::string&)>& StartGameHandler() {
  static std::function<void(OrderManager&, const std::string&)> fn_start_game;
  return fn_start_game;
}

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

void SetStartGameHandler(
    std::function<void(OrderManager&, const std::string&)> fn_start_game) {
  StartGameHandler() = std::move(fn_start_game);
}

void ClearStartGameHandler() { StartGameHandler() = nullptr; }

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

  if (name == "SyncInfo") {
    // L308-313:LobbyInfo 装载(Game.SyncLobbyInfo 的 UI 面 no-op)
    // L308-313: the LobbyInfo load (Game.SyncLobbyInfo's UI face is a
    // no-op).
    order_manager.LobbyInfo() = Session::Deserialize(
        order.str_target_string.value_or(""), order.str_order_string);
    return;
  }

  if (name == "StartGame") {
    // L171-200:TargetString 的存档帧装载 + Game.StartGame(handler 注入面)
    // L171-200: the TargetString gamesave-frame load + Game.StartGame
    // (the handler injection face).
    if (order.str_target_string.has_value() &&
        !order.str_target_string->empty()) {
      const std::vector<yaml::MiniYamlNode> vec_data = yaml::MiniYaml::FromString(
          *order.str_target_string, order.str_order_string);
      for (const auto& node : vec_data) {
        const std::string text{
            node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
        if (*node.Key == "SaveLastOrdersFrame") {
          int parsed = 0;
          const auto [ptr, ec] =
              std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
          if (ec == std::errc{})
            order_manager.SetGameSaveLastFrame(parsed);
        } else if (*node.Key == "SaveSyncFrame") {
          int parsed = 0;
          const auto [ptr, ec] =
              std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
          if (ec == std::errc{})
            order_manager.SetGameSaveLastSyncFrame(parsed);
        }
      }
    }

    if (StartGameHandler())
      StartGameHandler()(order_manager,
                         order_manager.LobbyInfo().global_settings.Map);
    return;
  }

  // 其余已命名命令(Message/Chat/SyncLobby*/HandshakeRequest/GameSaved/
  // SaveTraitData/...)依赖 UI/Game/存档面(Phase 6/7);未实现分支按
  // "无本地副作用"处理 —— order 流继续
  // The remaining named commands (Message/Chat/SyncLobby*/HandshakeRequest/
  // GameSaved/SaveTraitData/…) depend on the UI/Game/gamesave faces
  // (Phase 6/7); unimplemented branches take the "no local side effect"
  // path — the order stream continues.

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
