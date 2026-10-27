import std;

#include "server/server_traits.hpp"

#include "server/server.hpp"

namespace ora::server {

bool DebugServerTrait::InterpretCommand(Server& server,
                                        ServerConnection& conn,
                                        net::SessionClient* p_client,
                                        std::string_view str_cmd) {
  static_cast<void>(server);
  static_cast<void>(p_client);
  std::println("Server received command from player {}: {}",
               conn.int4_player_index, str_cmd);
  return false;
}

void DebugServerTrait::GameStarted(Server& server) {
  static_cast<void>(server);
  std::println("GameStarted()");
}

void DebugServerTrait::LobbyInfoSynced(Server& server) {
  static_cast<void>(server);
  std::println("LobbyInfoSynced()");
}

void DebugServerTrait::ServerStarted(Server& server) {
  static_cast<void>(server);
  std::println("ServerStarted()");
}

void DebugServerTrait::ServerShutdown(Server& server) {
  static_cast<void>(server);
  std::println("ServerShutdown()");
}

void DebugServerTrait::GameEnded(Server& server) {
  static_cast<void>(server);
  std::println("GameEnded()");
}

}
