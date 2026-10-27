#pragma once
import std;

#include "server/game_information.hpp"
#include "server/server_settings.hpp"

namespace ora::server {

class Server;
class ServerConnection;

class ServerTrait {
 public:
  virtual ~ServerTrait() = default;
};

class IInterpretCommand {
 public:
  virtual ~IInterpretCommand() = default;
  virtual bool InterpretCommand(Server& server, ServerConnection& conn,
                                net::SessionClient* p_client,
                                std::string_view str_cmd) = 0;
};

class INotifySyncLobbyInfo {
 public:
  virtual ~INotifySyncLobbyInfo() = default;
  virtual void LobbyInfoSynced(Server& server) = 0;
};

class INotifyServerStart {
 public:
  virtual ~INotifyServerStart() = default;
  virtual void ServerStarted(Server& server) = 0;
};

class INotifyServerEmpty {
 public:
  virtual ~INotifyServerEmpty() = default;
  virtual void ServerEmpty(Server& server) = 0;
};

class INotifyServerShutdown {
 public:
  virtual ~INotifyServerShutdown() = default;
  virtual void ServerShutdown(Server& server) = 0;
};

class IStartGame {
 public:
  virtual ~IStartGame() = default;
  virtual void GameStarted(Server& server) = 0;
};

class IClientJoined {
 public:
  virtual ~IClientJoined() = default;
  virtual void ClientJoined(Server& server, ServerConnection& conn) = 0;
};

class IEndGame {
 public:
  virtual ~IEndGame() = default;
  virtual void GameEnded(Server& server) = 0;
};

class ITick {
 public:
  virtual ~ITick() = default;
  virtual void Tick(Server& server) = 0;
};

class DebugServerTrait final : public ServerTrait,
                               public IInterpretCommand,
                               public IStartGame,
                               public INotifySyncLobbyInfo,
                               public INotifyServerStart,
                               public INotifyServerShutdown,
                               public IEndGame {
 public:
  bool InterpretCommand(Server& server, ServerConnection& conn,
                        net::SessionClient* p_client,
                        std::string_view str_cmd) override;

  void GameStarted(Server& server) override;
  void LobbyInfoSynced(Server& server) override;
  void ServerStarted(Server& server) override;
  void ServerShutdown(Server& server) override;
  void GameEnded(Server& server) override;
};

}
