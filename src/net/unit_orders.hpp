#pragma once
import std;

#include "core/color.hpp"
#include "net/order.hpp"

namespace ora::map {
class MapPreview;
}

namespace ora::net {

class OrderManager;

inline constexpr int kChatMessageMaxLength = 2500;

struct ClientJoinFace {
  std::string str_mod_id;
  std::string str_mod_version;
  std::string str_player_name;
  core::Color color_player{};
  std::string str_password;
  std::string str_fingerprint;

  std::function<std::string(std::string_view)> fn_sign;
};

void ProcessOrder(OrderManager& order_manager, sim::World* world,
                  int client_id, const Order& order);

std::optional<int> KickVoteTarget();
void SetKickVoteTarget(std::optional<int> int4_target);
void ClearUnitOrdersStatics();

void SetStartGameHandler(
    std::function<void(OrderManager&, const std::string&)> fn_start_game);
void ClearStartGameHandler();

void SetLobbyInfoChangedHandler(std::function<void()> fn_changed);
void ClearLobbyInfoChangedHandler();

void SetRuntimeMsFn(std::function<std::int64_t()> fn_runtime_ms);

void SetClientJoinFace(ClientJoinFace face_join);
const ClientJoinFace& ClientJoinFaceOf();

void SetMapCacheAtFn(std::function<ora::map::MapPreview*(
                         const std::string& str_uid)> fn_at);

}
